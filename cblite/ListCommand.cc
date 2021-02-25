//
// ListCommand.cc
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

#include "ListCommand.hh"

#ifdef _MSC_VER
    #include <Shlwapi.h>
    #pragma comment(lib, "shlwapi.lib")
#else
    #include <fnmatch.h>        // POSIX (?)
#endif

using namespace std;
using namespace litecore;
using namespace fleece;

static bool wildCardMatch(const char *name, const char *pattern) {
#ifdef _MSC_VER
    return PathMatchSpecA(name, pattern);
#else
    return fnmatch(pattern, name, 0) == 0;
#endif
}


static constexpr int kListColumnWidth = 24;


void ListCommand::usage() {
    writeUsageCommand("ls", true, "[PATTERN]");
    cerr <<
    "  Lists the IDs, and optionally other metadata, of the documents in the database.\n"
    "    -l : Long format (one doc per line, with metadata)\n"
    "    --offset N : Skip first N docs\n"
    "    --limit N : Stop after N docs\n"
    "    --desc : Descending order\n"
    "    --seq : Order by sequence, not docID\n"
    "    --del : Include deleted documents\n"
    "    --conf : Show only conflicted documents\n"
    "    --body : Display document bodies\n"
    "    --pretty : Pretty-print document bodies (implies --body)\n"
    "    --json5 : JSON5 syntax, i.e. unquoted dict keys (implies --body)\n"
    "    " << it("PATTERN") << " : pattern for matching docIDs, with shell-style wildcards '*', '?'\n"
    ;
}


void ListCommand::runSubcommand() {
    // Read params:
    processFlags({
        {"--offset", [&]{offsetFlag();}},
        {"--limit",  [&]{limitFlag();}},
        {"-l",       [&]{_longListing = true;}},
        {"--body",   [&]{bodyFlag();}},
        {"--pretty", [&]{prettyFlag();}},
        {"--raw",    [&]{rawFlag();}},
        {"--json5",  [&]{json5Flag();}},
        {"--desc",   [&]{descFlag();}},
        {"--seq",    [&]{_listBySeq = true;}},
        {"--del",    [&]{delFlag();}},
        {"--conf",   [&]{confFlag();}},
    });
    openDatabaseFromNextArg();
    string docIDPattern;
    if (hasArgs())
        docIDPattern = nextArg("docID pattern");
    endOfArgs();

    listDocs(docIDPattern);
}


void ListCommand::listDocs(string docIDPattern) {
    C4Error error;
    C4EnumeratorOptions options {_enumFlags};
    c4::ref<C4DocEnumerator> e;
    if (_listBySeq)
        e = c4db_enumerateChanges(_db, 0, &options, &error);
    else
        e = c4db_enumerateAllDocs(_db, &options, &error);
    if (!e)
        fail("creating enumerator", error);

    if (_offset > 0)
        cout << "(Skipping first " << _offset << " docs)\n";

    int64_t nDocs = 0;
    int xpos = 0;
    while (c4enum_next(e, &error)) {
        C4DocumentInfo info;
        c4enum_getDocumentInfo(e, &info);

        //TODO: Skip if docID is not in range of _startKey and _endKey

        if (!docIDPattern.empty()) {
            // Check whether docID matches pattern:
            string docID = slice(info.docID).asString();
            if (!wildCardMatch(docID.c_str(), docIDPattern.c_str()))
                continue;
        }

        // Handle offset & limit:
        if (_offset > 0) {
            --_offset;
            continue;
        }
        if (++nDocs > _limit && _limit >= 0) {
            cout << "\n(Stopping after " << _limit << " docs)";
            error.code = 0;
            break;
        }

        int idWidth = (int)info.docID.size;        //TODO: Account for UTF-8 chars
        if (_enumFlags & kC4IncludeBodies) {
            if (nDocs > 1)
                cout << "\n";
            c4::ref<C4Document> doc = c4enum_getDocument(e, &error);
            if (!doc)
                fail("reading document");
            catDoc(doc, true);

        } else if (_longListing) {
            // Long form:
            if (nDocs == 1) {
                cout << ansi("4") << "Document ID             Rev ID     Flags   Seq     Size" << ansiReset() << "\n";
            } else {
                cout << "\n";
            }
            info.revID.size = min(info.revID.size, (size_t)10);
            cout << info.docID << spaces(kListColumnWidth - idWidth);
            cout << info.revID << spaces(10 - (int)info.revID.size);
            cout << ((info.flags & kDocDeleted)        ? 'd' : '-');
            cout << ((info.flags & kDocConflicted)     ? 'c' : '-');
            cout << ((info.flags & kDocHasAttachments) ? 'a' : '-');
            cout << ' ';
            cout << setw(7) << info.sequence << " ";
            cout << setw(7) << setprecision(1) << (info.bodySize / 1024.0) << 'K';

        } else {
            // Short form:
            int lineWidth = terminalWidth();
            int nSpaces = xpos ? (kListColumnWidth - (xpos % kListColumnWidth)) : 0;
            int newXpos = xpos + nSpaces + idWidth;
            if (newXpos < lineWidth) {
                if (xpos > 0)
                    cout << spaces(nSpaces);
                xpos = newXpos;
            } else {
                cout << "\n";
                xpos = idWidth;
            }
            cout << info.docID;
        }
    }
    if (error.code)
        fail("enumerating documents", error);

    if (nDocs == 0) {
        if (docIDPattern.empty())
            cout << "(No documents)";
        else
            cout << "(No documents with IDs matching \"" << docIDPattern << "\")";
    }
    cout << "\n";
}


void ListCommand::catDoc(C4Document *doc, bool includeID) {
    Value body = Value::fromData(c4doc_getRevisionBody(doc));
    if (!body)
        fail("Unexpectedly couldn't parse document body!");
    slice docID, revID;
    if (includeID || _showRevID)
        docID = slice(doc->docID);
    if (_showRevID)
        revID = (slice)doc->selectedRev.revID;
    if (_prettyPrint)
        prettyPrint(body, "", docID, revID, (_keys.empty() ? nullptr : &_keys));
    else
        rawPrint(body, docID, revID);
}


CBLiteCommand* newListCommand(CBLiteTool &parent) {
    return new ListCommand(parent);
}
