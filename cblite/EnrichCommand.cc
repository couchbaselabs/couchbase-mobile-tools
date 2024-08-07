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
#include "LLMProvider.hh"
#include "OpenAI.hh"
#include "Gemini.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

void EnrichCommand::usage() {
    writeUsageCommand("enrich", false, "MODEL PROPERTY DESTINATION");
    cerr <<
    "Enriches given JSON with embeddings of selected field\n"
    "    --offset N : Skip first N docs\n"
    "    --limit N : Stop after N docs\n"
    "    " << it("MODEL") << " : AI model (required)\n"
    "Supports OpenAI Models: text-embedding-3-small, text-embedding-3-large, text-embedding-ada-002\n"
    "Supports Gemini Models: text-embedding-004\n"
    "    " << it("PROPERTY") << " : property for matching docs (required)\n"
    "    " << it("DESTINATION") << " : destination property, defaults to PROPERTY_vector\n"
    ;
}

void EnrichCommand::runSubcommand() {
    // Read params:
    processFlags({
        {"--offset", [&]{offsetFlag();}},
        {"--limit",  [&]{limitFlag();}},
    });
    openWriteableDatabaseFromNextArg();
    
    string srcProp, dstProp, modelName;
    modelName = nextArg("AI model");
    srcProp = nextArg("source property");
    if (hasArgs())
        dstProp = nextArg("destination property");
    else
        dstProp = srcProp + "_vector";
    endOfArgs();
    
    // Determine which model and provider to use based on user input
    unique_ptr<LLMProvider> provider = LLMProvider::create(modelName);

    if (!provider)
        fail("Model " + modelName + " not supported");
    
    if (!getenv("LLM_API_KEY"))
        fail("API Key not provided");
    
    // Run enrich command
    enrichDocs(srcProp, dstProp, provider, modelName);
}

void EnrichCommand::enrichDocs(const string& srcProp, const string& dstProp, unique_ptr<LLMProvider>& provider, const std::string& modelName) {
    EnumerateDocsOptions options{};
    options.flags       |= kC4IncludeBodies;
    options.bySequence  = true;
    options.offset      = _offset;
    options.limit       = _limit;
    
    cout << "\n";
    if (_offset > 0)
        cout << "(Skipping first " << _offset << " docs)\n";
    
    map<string, MutableDict> docDict;
    vector<Value> props;
    int64_t nDocs;
    
    cout << "Reading documents" << endl;
    tie(docDict, props, nDocs) = readData(srcProp, options);
    
    cout << "Running requests" << endl;
    vector<alloc_slice> responses = provider->run(modelName, props);
    if (responses.empty())
        fail("LLM Provider failed to return response");

    cout << "Enriching documents" << endl;
    writeResult(docDict, responses, options, dstProp, provider);
    
    // Output status to user
    if (nDocs > _limit && _limit > 0) {
        cout << "\n(Stopping after " << _limit << " docs)";
    }
    cout << "\n";
}

tuple <map<string, MutableDict>, vector<Value>, int64_t> EnrichCommand::readData(const string& srcProp, EnumerateDocsOptions options) {
    map<string, MutableDict> docDict;
    vector<Value> props;
    string docIDStr;
    // Gather properties and doc info
    int64_t nDocs = enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
        Dict body = c4doc_getProperties(doc);
        if (!body) {
            fail("Unexpectedly couldn't parse document body!");
        }
        
        Value rawSrcPropValue = body.get(srcProp);
        if (rawSrcPropValue.type() != kFLString) {
            cout << "Property type must be a string. Skipping " << doc->docID << endl;
            return;
        }
        auto mutableBody = body.mutableCopy(kFLDefaultCopy);
        props.push_back(rawSrcPropValue);
        docIDStr = string(doc->docID);
        docDict.insert(make_pair(docIDStr, mutableBody));
    });
    
    if (nDocs == 0) {
        cout << "(No documents with property matching \"" << srcProp << "\"" << ")";
        fail("No documents to enrich");
    }
    cout << "\n";
    
    return make_tuple(docDict, props, nDocs);
}

void EnrichCommand::writeResult(map<string, MutableDict> docDict, vector<alloc_slice> responses, EnumerateDocsOptions options, const string& dstProp, unique_ptr<LLMProvider>& provider) {
    // Start transaction
    C4Error error;
    c4::Transaction t(_db);
    if (!t.begin(&error))
        fail("Couldn't open database transaction");
    
    map<string, MutableDict>::iterator it;
    int i = 0;
    for (it = docDict.begin(); it != docDict.end(); it++) {
        // Parse response
        alloc_slice response = responses.at(i);
        C4String docID = c4str(it->first.c_str());
        auto mutableBody = it->second;
        C4Document* doc = c4coll_getDoc(collection(), docID, true, kDocGetAll, &error);
        Doc newDoc = Doc::fromJSON(response);
        
        // Update doc
        Value embedding = provider->getEmbedding(newDoc);
        mutableBody.set(dstProp, embedding);
        auto json = mutableBody.toJSON();
        auto newBody = alloc_slice(c4db_encodeJSON(_db, json, &error));
        if (!newBody) {
            fail("Couldn't encode body", error);
        }
        
        doc = c4doc_update(doc, newBody, 0, &error);
        if (!doc) {
            fail("Couldn't save document", error);
        }
        i++;
    }
    
    // End transaction (commit)
    if (!t.commit(&error))
        fail("Couldn't commit database transaction", error);
}

CBLiteCommand* newEnrichCommand(CBLiteTool &parent) {
    return new EnrichCommand(parent);
}
