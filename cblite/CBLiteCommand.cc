//
// CBLiteCommand.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#include "CBLiteCommand.hh"


void CBLiteCommand::writeSize(uint64_t n) {
    static const char* kScaleNames[] = {" bytes", "KB", "MB", "GB"};
    int scale = 0;
    double scaled = n;
    while (scaled >= 1024 && scale < 3) {
        scaled /= 1024;
        ++scale;
    }
    auto prec = cout.precision();
    cout.precision(scale < 2 ? 0 : 1);
    cout << fixed << scaled << defaultfloat;
    cout.precision(prec);
    cout << kScaleNames[scale];
}


void CBLiteCommand::enumerateDocs(C4EnumeratorFlags flags, function<bool(C4DocEnumerator*)> callback) {
    C4EnumeratorOptions options = kC4DefaultEnumeratorOptions;
    options.flags = flags;
    C4Error error;
    c4::ref<C4DocEnumerator> e = c4db_enumerateAllDocs(_db, &options, &error);
    if (e) {
        while (c4enum_next(e, &error)) {
            if (!callback(e))
                break;
        }
    }
    if (error.code != 0)
        fail("enumerating documents", error);
}


void CBLiteCommand::getDBSizes(uint64_t &dbSize, uint64_t &blobsSize, uint64_t &nBlobs) {
    dbSize = blobsSize = nBlobs = 0;
    alloc_slice pathSlice = c4db_getPath(_db);
    FilePath path(pathSlice.asString());
    path["db.sqlite3"].forEachMatch([&](const litecore::FilePath &file) {
        dbSize += file.dataSize();
    });
    auto attachmentsPath = path["Attachments/"];
    if (attachmentsPath.exists()) {
        attachmentsPath.forEachFile([&](const litecore::FilePath &file) {
            ++nBlobs;
            blobsSize += file.dataSize();
        });
    }
}


c4::ref<C4Document> CBLiteCommand::readDoc(string docID) {
    C4Error error;
    c4::ref<C4Document> doc = c4doc_get(_db, slice(docID), true, &error);
    if (!doc) {
        if (error.domain == LiteCoreDomain && error.code == kC4ErrorNotFound)
            cerr << "Error: Document \"" << docID << "\" not found.\n";
        else
            errorOccurred(format("reading document \"%s\"", docID.c_str()), error);
    }
    return doc;
}


bool CBLiteCommand::canBeUnquotedJSON5Key(slice key) {
    if (key.size == 0 || isdigit(key[0]))
        return false;
    for (unsigned i = 0; i < key.size; i++) {
        if (!isalnum(key[i]) && key[i] != '_' && key[i] != '$')
            return false;
    }
    return true;
}


bool CBLiteCommand::isGlobPattern(string &str) {
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i) {
        char c = str[i];
        if ((c == '*' || c == '?') && (i == 0 || str[i-1] != '\\'))
            return true;
    }
    return false;
}

void CBLiteCommand::unquoteGlobPattern(string &str) {
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i) {
        if (str[i] == '\\') {
            str.erase(i, 1);
            --size;
        }
    }
}


void CBLiteCommand::rawPrint(Value body, slice docID, slice revID) {
    alloc_slice jsonBuf = body.toJSON(_json5, true);
    slice restOfJSON = jsonBuf;
    if (docID) {
        // Splice a synthesized "_id" property into the start of the JSON object:
        cout << "{" << ansiDim() << ansiItalic()
        << (_json5 ? "_id" : "\"_id\"") << ":\""
        << ansiReset() << ansiDim()
        << docID << "\"";
        if (revID) {
            cout << "," << ansiItalic()
            << (_json5 ? "_rev" : "\"_rev\"") << ":\""
            << ansiReset() << ansiDim()
            << revID << "\"";
        }
        restOfJSON.moveStart(1);
        if (restOfJSON.size > 1)
            cout << ", ";
        cout << ansiReset();
    }
    cout << restOfJSON;
}


void CBLiteCommand::prettyPrint(Value value,
                                   const string &indent,
                                   slice docID,
                                   slice revID,
                                   const set<alloc_slice> *onlyKeys) {
    // TODO: Support an includeID option
    switch (value.type()) {
        case kFLDict: {
            auto dict = value.asDict();
            string subIndent = indent + "  ";
            int n = 0;
            cout << "{";
            if (docID) {
                n++;
                cout << '\n' << subIndent << ansiDim() << ansiItalic();
                cout << (_json5 ? "_id" : "\"_id\"");
                cout << ansiReset() << ansiDim() << ": \"" << docID << "\"";
                if (revID) {
                    n++;
                    cout << ",\n" << subIndent << ansiItalic();
                    cout << (_json5 ? "_rev" : "\"_rev\"");
                    cout << ansiReset() << ansiDim() << ": \"" << revID << "\"";
                }
            }
            vector<slice> keys;
            for (Dict::iterator i(dict); i; ++i) {
                slice key = i.keyString();
                if (!onlyKeys || onlyKeys->find(alloc_slice(key)) != onlyKeys->end())
                    keys.push_back(key);
            }
            sort(keys.begin(), keys.end());
            for (slice key : keys) {
                if (n++ > 0)
                    cout << ',' << ansiReset();
                cout << '\n' << subIndent << ansiItalic();
                if (_json5 && canBeUnquotedJSON5Key(key))
                    cout << key;
                else
                    cout << '"' << key << '"';      //FIX: Escape quotes
                cout << ansiReset() << ": ";

                prettyPrint(dict.get(key), subIndent);
            }
            cout << '\n' << indent << "}";
            break;
        }
        case kFLArray: {
            string subIndent = indent + "  ";
            cout << "[\n";
            for (Array::iterator i(value.asArray()); i; ++i) {
                cout << subIndent;
                prettyPrint(i.value(), subIndent);
                if (i.count() > 1)
                    cout << ',';
                cout << '\n';
            }
            cout << indent << "]";
            break;
        }
        case kFLData: {
            // toJSON would convert to base64, which isn't readable, so use escapes instead:
            static const char kHexDigits[17] = "0123456789abcdef";
            slice data = value.asData();
            auto end = (const uint8_t*)data.end();
            cout << "«";
            bool dim = false;
            for (auto c = (const uint8_t*)data.buf; c != end; ++c) {
                if (*c >= 32 && *c < 127) {
                    if (dim)
                        cout << ansiReset();
                    dim = false;
                    cout << (char)*c;
                } else {
                    if (!dim)
                        cout << ansiDim();
                    dim = true;
                    cout << '\\' << kHexDigits[*c/16] << kHexDigits[*c%16];
                }
            }
            if (dim)
                cout << ansiReset();
            cout << "»";
            break;
        }
        default: {
            alloc_slice json(value.toJSON());
            cout << json;
            break;
        }
    }
}
