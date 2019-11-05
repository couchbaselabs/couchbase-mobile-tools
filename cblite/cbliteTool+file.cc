//
// cbliteTool+file.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "cbliteTool.hh"


void CBLiteTool::fileUsage() {
    writeUsageCommand("info", false);
    cerr <<
    "  Displays information about the database, like sizes and counts.\n"
    "    --verbose or -v : Gives more detail.\n"
    ;
}


const Tool::FlagSpec CBLiteTool::kFileFlags[] = {
    {"--verbose", (FlagHandler)&CBLiteTool::verboseFlag},
    {"-v",        (FlagHandler)&CBLiteTool::verboseFlag},
};


static uint64_t countDocsWhere(C4Database *db, const char *what) {
    string n1ql = "SELECT count(*) WHERE "s + what;
    C4Error error;
    c4::ref<C4Query> q = c4query_new2(db, kC4N1QLQuery, slice(n1ql), nullptr, &error);
    assert(q);
    c4::ref<C4QueryEnumerator> e = c4query_run(q, nullptr, nullslice, &error);
    assert(e);
    c4queryenum_next(e, &error);
    return FLValue_AsUnsigned(FLArrayIterator_GetValueAt(&e->columns, 0));
}


void CBLiteTool::fileInfo() {
    // Read params:
    processFlags(nullptr);
    if (_showHelp) {
        fileUsage();
        return;
    }
    openDatabaseFromNextArg();
    endOfArgs();

    // Database path and size:
    alloc_slice pathSlice = c4db_getPath(_db);
    cout << "Database:    " << pathSlice << "\n";

    FilePath path(pathSlice.asString());
    uint64_t dbSize = 0, blobsSize = 0, nBlobs = 0;
    path["db.sqlite3"].forEachMatch([&](const litecore::FilePath &file) {
        dbSize += file.dataSize();
    });
    auto attachmentsPath = path["Attachments/"];
    if (attachmentsPath.exists()) {
        attachmentsPath.forEachFile([&](const litecore::FilePath &file) {
            blobsSize += file.dataSize();
        });
    }
    cout << "Total size:  "; writeSize(dbSize + blobsSize); cerr << "\n";

    // Document counts:
    cout << "Documents:   ";
    cout.flush(); // the next results may take a few seconds to print
    cout << c4db_getDocumentCount(_db);

    auto nDeletedDocs = countDocsWhere(_db, "_deleted");
    if (nDeletedDocs > 0)
        cout << " live, " << nDeletedDocs << " deleted";

    C4Timestamp nextExpiration = c4db_nextDocExpiration(_db);
    if (nextExpiration > 0) {
        cout << ", " << countDocsWhere(_db, "_expiration > 0") << " with expirations";
        auto when = std::max(nextExpiration - c4_now(), 0ll);
        cout << " (next in " << when << " sec)";
    }

    cout  << ", last sequence #" << c4db_getLastSequence(_db) << "\n";

    if (nBlobs > 0) {
        cout << "Blobs:       " << nBlobs << ", ";
        writeSize(blobsSize);
        cerr << "\n";
    }

    // Indexes:
    alloc_slice indexesFleece = c4db_getIndexes(_db, nullptr);
    auto indexes = Value::fromData(indexesFleece).asArray();
    if (indexes.count() > 0) {
        cout << "Indexes:     ";
        int n = 0;
        for (Array::iterator i(indexes); i; ++i) {
            if (n++)
                cout << ", ";
            cout << i.value().asString();
        }
        cout << "\n";
    }

    // UUIDs:
    C4UUID publicUUID, privateUUID;
    if (c4db_getUUIDs(_db, &publicUUID, &privateUUID, nullptr)) {
        cout << "UUIDs:       public "
             << slice(&publicUUID, sizeof(publicUUID)).hexString().c_str()
             << ", private " << slice(&privateUUID, sizeof(privateUUID)).hexString().c_str()
             << "\n";
    }

    // Shared keys:
    auto sk = c4db_getFLSharedKeys(_db);
    FLSharedKeys_Decode(sk, 0);
    fleece::Doc stateDoc(alloc_slice(FLSharedKeys_GetStateData(sk)));
    auto keyArray = stateDoc.asArray();
    cout << "Shared keys: ";
    if (verbose() && !keyArray.empty()) {
        cout << '"';
        int n = 0;
        for (Array::iterator i(keyArray); i; ++i) {
            if (++n) cout << "\", \"";
            cout << i->asString();
        }
        cout << "\" (" << keyArray.count() << ")\n";
    } else {
        auto count = FLSharedKeys_Count(sk);
        cout << count;
        if (count > 0)
            cout << "  (-v to list them)";
        cout << '\n';
    }
}


void CBLiteTool::writeSize(uint64_t n) {
    static const char* kScales[] = {" bytes", "KB", "MB", "GB"};
    int scale = 0;
    while (n >= 1024 && scale < 3) {
        n = (n + 512) / 1024;
        ++scale;
    }
    cout << n << kScales[scale];
}
