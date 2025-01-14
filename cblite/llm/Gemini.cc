//
// Gemini.cc
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

#include "Gemini.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

vector<alloc_slice> Gemini::run(const string& modelName, vector<Value> propVec) {
    const int BATCH_SIZE = 50;
    int docRemainder = propVec.size() % BATCH_SIZE;
    int batches = (int)((propVec.size() + BATCH_SIZE - 1) / BATCH_SIZE);
    vector<alloc_slice> responses;
    alloc_slice response, completeResponse;
    Value rawSrcPropValue;
    
    for (int i = 0; i < batches; i++) {
        // Create rest body
        string restBody = stringprintf("{\"requests\": [");
        int limit = (i == batches - 1) ? docRemainder : BATCH_SIZE;
        for(int j = 0; j < limit; j++) {
            rawSrcPropValue = propVec.at((BATCH_SIZE * i) + j);
            restBody += stringprintf(" {\"model\": \"models/%s\", \"content\": {\"parts\": [{\"text\": \"%.*s\"}]}, },", modelName.c_str(), SPLAT(rawSrcPropValue.asString()));
        }
        restBody += "]}";
        
        // Get headers
        Encoder enc;
        enc.beginDict();
        enc["Content-Type"_sl] = "application/json";
        enc.endDict();
        auto headers = enc.finishDoc();
        
        // Run request
        auto r = std::make_unique<REST::Response>("https", "POST", "generativelanguage.googleapis.com", 443, stringprintf("v1beta/models/%s:batchEmbedContents?key=%s", modelName.c_str(), getenv("LLM_API_KEY")));
        r->setHeaders(headers).setBody(restBody);
        response = LLMProvider::run(r);
        if (!response) {
            responses.clear();
            return responses;
        }
        
        // Parse responses
        Doc responseDoc = Doc::fromJSON(response);
        Dict singleResponse;
        for (int h = 0; h < responseDoc.asDict()["embeddings"].asArray().count(); h++) {
            singleResponse = responseDoc.asDict()["embeddings"].asArray()[h].asDict();
            responses.push_back(singleResponse.toJSON());
        }
    }
    cout << endl;
    return responses;
}

Value Gemini::getEmbedding(Doc newDoc) {
    return newDoc.asDict()["values"];
}

unique_ptr<LLMProvider> newGeminiModel() {
    return std::make_unique<Gemini>();
}
