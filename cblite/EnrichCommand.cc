//
// EnrichCommand.cc
//
// Copyright (c) 2024 Couchbase, Inc All rights reserved.
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

#include <string>
#include <iostream>
#include <fstream>
#include "EnrichCommand.hh"
#include "fleece/Fleece.hh"
#include "StringUtil.hh"
#include "fleece/Mutable.hh"
#include "Response.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

void EnrichCommand::usage() {
    writeUsageCommand("enrich", false, "PROP");
    cerr <<
    "Enriches given JSON with embeddings of selected field\n"
    "    --offset N : Skip first N docs\n"
    "    --limit N : Stop after N docs\n"
    "    " << it("PROPERTY") << " : property for matching docs\n"
    "    " << it("DESTINATION") << " : destination property\n"
    ;
}

void EnrichCommand::runSubcommand() {
    // Read params:
    processFlags({
        {"--offset", [&]{offsetFlag();}},
        {"--limit",  [&]{limitFlag();}},
        //{"--writeable", [&]{writeableFlag();}}
    });
    openWriteableDatabaseFromNextArg();
    string srcProp, dstProp;
    srcProp = nextArg("source property");
    if (hasArgs())
        dstProp = nextArg("destination property");
    else
        dstProp = srcProp + "_vector";
    endOfArgs();
    
    enrichDocs(srcProp, dstProp);
}

void EnrichCommand::enrichDocs(const string& srcProp, const string& dstProp) {
    EnumerateDocsOptions options{};
    options.flags       |= kC4IncludeBodies;
    options.bySequence  = true;
    options.offset      = _offset;
    options.limit       = _limit;
    
    cout << "\n";
    if (_offset > 0)
        cout << "(Skipping first " << _offset << " docs)\n";
    
    // Start transaction
    C4Error error;
    c4::Transaction t(_db);
    if (!t.begin(&error))
        fail("Couldn't open database transaction");
    
    // Loop through docs and get properties
    int64_t nDocs = enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
        Dict body = c4doc_getProperties(doc);
        if (!body)
            fail("Unexpectedly couldn't parse document body!");
        
        Value rawSrcPropValue = body.get(srcProp);
        if (rawSrcPropValue.type() != kFLString) {
            cout << "Property type must be a string" << endl;
            return;
        }
        
        string restBody = format("{\"input\":\"%.*s\", \"model\":\"text-embedding-3-small\"}", SPLAT(rawSrcPropValue.asString()));

        // LiteCore Request and Response
        Encoder enc;
        enc.beginDict();
        enc["Content-Type"_sl] = "application/json";
        enc["Content-Length"_sl] = restBody.length();
        if (getenv("API_KEY") == NULL)
            fail("API Key not provided", error);
        
        enc["Authorization"] = format("Bearer %s", getenv("API_KEY"));
        enc.endDict();
        auto headers = enc.finishDoc();
        auto r = std::make_unique<REST::Response>("https", "POST", "api.openai.com", 443, "v1/embeddings");
        r->setHeaders(headers).setBody(restBody);
        alloc_slice response;
        
        if (r->run()) {
            response = r->body();
        } else {
            if ( r->error() == C4Error{NetworkDomain, kC4NetErrTimeout} ) {
                C4Warn("REST request timed out. Current timeout is %f seconds", r->getTimeout());
            }
            else
            {
                C4Warn("REST request failed. %d/%d", r->error().domain, r->error().code);
                r->error() = C4Error{NetworkDomain, kC4NetErrUnknown};
            }
            return;
        }
        
        // Parse response
        Doc newDoc = Doc::fromJSON(response);
        Value embedding = newDoc.asDict()["data"].asArray()[0].asDict()["embedding"];
        auto mutableBody = body.mutableCopy(kFLDefaultCopy);
        mutableBody.set(dstProp, embedding);
        auto json = mutableBody.toJSON();
        
        // Update doc
        doc = c4doc_update(doc, json, 0, &error);
        if (!doc)
            fail("Couldn't save document", error);
    });
    
    // End transaction (commit)
    if (!t.commit(&error))
        fail("Couldn't commit database transaction", error);
    
    // Output status to user
    if (nDocs == 0) {
            cout << "(No documents with property matching \"" << srcProp << "\"" << ")";
    } else if (nDocs > _limit && _limit > 0) {
        cout << "\n(Stopping after " << _limit << " docs)";
    }
    cout << "\n";
}

CBLiteCommand* newEnrichCommand(CBLiteTool &parent) {
    return new EnrichCommand(parent);
}
