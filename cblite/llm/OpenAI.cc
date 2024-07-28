//
// OpenAI.cc
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

#include "OpenAI.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

vector<alloc_slice> OpenAI::run(const string& modelName, vector<Value> wordVec) {
    vector<alloc_slice> responses;
    for (int i = 0; i < wordVec.size(); i++) {
        // Create rest body
        Value rawSrcPropValue = wordVec.at(i);
        string restBody = format("{\"input\":\"%.*s\", \"model\":\"%s\"}", SPLAT(rawSrcPropValue.asString()), modelName.c_str());
       
        // Get headers
        Encoder enc;
        enc.beginDict();
        enc["Content-Type"_sl] = "application/json";
        enc["Authorization"] = format("Bearer %s", getenv("LLM_API_KEY"));
        enc.endDict();
        auto headers = enc.finishDoc();
        
        // Run request
        cout << "Running request " << i+1;
        auto r = std::make_unique<REST::Response>("https", "POST", "api.openai.com", 443, "v1/embeddings");
        r->setHeaders(headers).setBody(restBody);
        alloc_slice response = LLMProvider::run(r);
        if (!response) {
            cout << " Failed" << endl;
            responses.clear();
            return responses;
        }
        responses.push_back(response);
        cout << " Completed" << endl;
    }
    cout << endl;
    return responses;
}

Value OpenAI::getEmbedding(Doc newDoc) {
    return newDoc.asDict()["data"].asArray()[0].asDict()["embedding"];
}

unique_ptr<LLMProvider> newOpenAIModel() {
    return std::make_unique<OpenAI>();
}
