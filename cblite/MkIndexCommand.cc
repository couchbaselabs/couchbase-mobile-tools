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


#ifdef COUCHBASE_ENTERPRISE
static pair<string_view,string_view> split(string_view str, string_view sep) {
    if (auto pos = str.find(sep); pos != string::npos)
        return {str.substr(0, pos), str.substr(pos + 1) };
    else
        throw invalid_argument("Missing separator '"s + string(sep) + "'");
}
#endif


class MkIndexCommand : public CBLiteCommand {
public:

    MkIndexCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("mkindex", true, "NAME EXPRESSION");
        cerr <<
        "  Creates an index. The EXPRESSION does not need to be quoted.\n"
        "    --json   : Use JSON syntax for " << it("EXPRESSION") << ", instead of N1QL\n"
        "    --fts    : Create a Full-Text Search (FTS) index\n"
#ifdef COUCHBASE_ENTERPRISE
        "    --vector : Create a vector index\n\n"
#endif
        << bold("FTS index flags:\n") <<
        "    --language LANG    : (Human) language, by name or ISO-369 code:\n"
        "                         " << ansiDim() << "da/danish, nl/dutch, en/english, fi/finnish, fr/french, de/german,\n"
        "                         hu/hungarian, it/italian, no/norwegian, pt/portuguese,\n"
        "                         ro/romanian, ru/russian, es/spanish, sv/swedish, tr/turkish" << ansiReset() << "\n"
        "    --ignoreDiacritics : Ignore diacritical (accent) marks\n"
        "    --noStemming       : Don't try to recognize word variations like plurals.\n\n";
#ifdef COUCHBASE_ENTERPRISE
        cerr << bold("Vector index flags:\n") <<
        "    --dim N        : Number of dimensions (required)\n"
        "    --metric M     : Distance metric (M = 'euclidean' or 'cosine')\n"
        "    --centroids N  : Flat clustering with N centroids\n"
        "    --multi NxB    : Multi-index clustering with N subquantizers, B bits\n"
        "    --encoding ENC : Encoding type (ENC = 'none', 'SQ8', 'PQ32x8', etc.)\n";
        if (! getenv("CBLITE_EXTENSION_PATH")) {
            cerr <<
            "NOTE: Vector indexes require the CouchbaseLiteVectorSearch extension.\n"
            "      The environment variable CBLITE_EXTENSION_PATH must be set to its parent directory.\n";
        }
#endif
    }


    void runSubcommand() override {
        C4QueryLanguage language = kC4N1QLQuery;
        bool ftsFlags = false;
        bool vectorFlags = false;
        C4IndexOptions options = {};
        string ftsLanguage;
        processFlags({
            {"--json", [&]{language = kC4JSONQuery;}},

            {"--fts",  [&]{ftsFlags = true;}},
            {"--language", [&]{
                ftsFlags = true;
                ftsLanguage = nextArg("FTS language");
                options.language = ftsLanguage.c_str();
            }},
            {"--ignoreDiacritics", [&]{
                ftsFlags = true;
                options.ignoreDiacritics = true;
            }},
            {"--noStemming", [&]{
                ftsFlags = true;
                options.disableStemming = true;
            }},

#ifdef COUCHBASE_ENTERPRISE
            {"--vector",  [&]{vectorFlags = true;}},
            {"--dim", [&]{
                vectorFlags = true;
                options.vector.dimensions = nextIntArg("dimensions", 2, 2048);
            }},
            {"--metric", [&]{
                vectorFlags = true;
                if (string m = lowercase(nextArg("metric name")); m == "euclidean")
                    options.vector.metric = kC4VectorMetricEuclidean2;
                else if (m == "cosine")
                    options.vector.metric = kC4VectorMetricCosine;
                else
                    throw invalid_argument("--metric value must be euclidean or cosine");
            }},
            {"--centroids", [&]{
                vectorFlags = true;
                options.vector.clustering.type = kC4VectorClusteringFlat;
                options.vector.clustering.flat_centroids = nextIntArg("number of centroids",
                                                                      2, 65536);
            }},
            {"--multi", [&]{
                vectorFlags = true;
                options.vector.clustering.type = kC4VectorClusteringMulti;
                string arg = nextArg("multi-index parameters");
                auto [sub, bits] = split(arg, "x");
                options.vector.clustering.multi_subquantizers = parseInt(sub, 2);
                options.vector.clustering.multi_bits = parseInt(bits, 4);
            }},
            {"--encoding", [&]{
                vectorFlags = true;
                string arg = lowercase(nextArg("encoding type"));
                if (hasPrefix(arg, "pq")) {
                    auto [sub, bits] = split(arg, "x");
                    options.vector.encoding.pq_subquantizers = parseInt(sub, 2, 2048);
                    options.vector.encoding.bits = parseInt(bits, 4);
                } else if (hasPrefix(arg, "sq")) {
                    options.vector.encoding.type = kC4VectorEncodingSQ;
                    options.vector.encoding.bits = parseInt(arg.substr(2), 4, 8);
                } else {
                    throw invalid_argument("encoding type must start with PQ or SQ");
                }
            }},
#endif
        });
        
#ifdef COUCHBASE_ENTERPRISE
        if (ftsFlags && vectorFlags)
            throw invalid_argument("Can't combine FTS and vector options");
        if (vectorFlags && options.vector.dimensions == 0)
            throw invalid_argument("Number of dimensions (--dim) is required in a vector index");
#endif
        
        openWriteableDatabaseFromNextArg();
        string name = nextArg("index name");
        string expression = restOfInput("expression");

        C4IndexType indexType;
        const char* message;
        if (ftsFlags) {
            indexType = kC4FullTextIndex;
            message = "Creating FTS index '";
        } else if (vectorFlags) {
            indexType = kC4VectorIndex;
            message = "Creating vector index '";
        } else {
            indexType = kC4ValueIndex;
            message = "Creating index '";
        }
        cout << message << name << "' ...";
        cout.flush();

        C4Error error;
        bool ok;
        ok = c4coll_createIndex(collection(), slice(name), slice(expression), language, indexType,
                                &options, &error);
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
