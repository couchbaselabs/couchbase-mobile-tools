//
// MkIndexCommand.cc
//
// Copyright © 2022 Couchbase. All rights reserved.
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

using namespace std;
using namespace litecore;


class MkIndexCommand : public CBLiteCommand {
public:

    MkIndexCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("mkindex", true, "NAME EXPRESSION");
        cerr <<
        "  Creates an index.\n"
        "    --json : Use JSON syntax for " << it("EXPRESSION") << ", instead of N1QL\n"
        "    --fts : Create a Full-Text Search index\n"
        "    --language LANG : (Human) language for FTS index, by name or ISO-369 code:\n"
        "                      " << ansiDim() << "da/danish, nl/dutch, en/english, fi/finnish, fr/french, de/german,\n"
        "                      hu/hungarian, it/italian, no/norwegian, pt/portuguese,\n"
        "                      ro/romanian, ru/russian, es/spanish, sv/swedish, tr/turkish" << ansiReset() << "\n"
        "    --ignoreDiacritics : FTS index should ignore diacritical (accent) marks\n"
        "    --noStemming : FTS index should not try to recognize word variations like plurals.\n"
        ;
    }


    void runSubcommand() override {
        C4QueryLanguage language = kC4N1QLQuery;
        C4IndexType indexType = kC4ValueIndex;
        C4IndexOptions ftsOptions = {};
        string ftsLanguage;
        processFlags({
            {"--json", [&]{language = kC4JSONQuery;}},
            {"--fts",  [&]{indexType = kC4FullTextIndex;}},
            {"--language", [&]{
                indexType = kC4FullTextIndex;
                ftsLanguage = nextArg("FTS language");
                ftsOptions.language = ftsLanguage.c_str();
            }},
            {"--ignoreDiacritics", [&]{
                indexType = kC4FullTextIndex;
                ftsOptions.ignoreDiacritics = true;
            }},
            {"--noStemming", [&]{
                indexType = kC4FullTextIndex;
                ftsOptions.disableStemming = true;
            }},
        });
        openWriteableDatabaseFromNextArg();
        string name = nextArg("index name");
        string expression = restOfInput("expression");

        cout << "Creating index '" << name << "' ...";
        cout.flush();
        C4Error error;
        bool ok;
#ifdef HAS_COLLECTIONS
        ok = c4coll_createIndex(collection(), slice(name), slice(expression), language, indexType,
                                &ftsOptions, &error);
#else
        ok = c4db_createIndex2(_db, slice(name), slice(expression), language, indexType,
                               &ftsOptions, &error);
#endif
        if (!ok) {
            cout << endl;
            fail("Couldn't create index", error);
        }

        cout << " Done!\n";
    }
};


CBLiteCommand* newMkIndexCommand(CBLiteTool &parent) {
    return new MkIndexCommand(parent);
}
