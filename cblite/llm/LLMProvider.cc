//
// LLMProvider.cc
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

#include "LLMProvider.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

fleece::alloc_slice LLMProvider::run(unique_ptr<litecore::REST::Response>& r) {
    alloc_slice response;
    if (r->run()) {
        response = r->body();
    } else {
        if ( r->error() == C4Error{NetworkDomain, kC4NetErrTimeout} ) {
            C4Warn("REST request timed out. Current timeout is %f seconds", r->getTimeout());
            return nullslice;
        }
        else
        {
            C4Warn("REST request failed. %d/%d", r->error().domain, r->error().code);
            return nullslice;
        }
    }
    return response;
}

std::unique_ptr<LLMProvider> LLMProvider::create(const std::string& modelName) {
    // Initialize model and dict
    unique_ptr<LLMProvider> model;
    map <string, LLMProvider::Model> modelsDict = {{"text-embedding-3-small", LLMProvider::Model::OpenAI}, {"text-embedding-3-large", LLMProvider::Model::OpenAI}, {"text-embedding-ada-002", LLMProvider::Model::OpenAI}, {"text-embedding-004", LLMProvider::Model::Gemini}};
    
    // Determine which model to use
    if(modelsDict.find(modelName) != modelsDict.end()){
        LLMProvider::Model provider = modelsDict[modelName];
        switch (provider) {
            case LLMProvider::Model::OpenAI:
                model = newOpenAIModel();
                break;
            case LLMProvider::Model::Gemini:
                model = newGeminiModel();
                break;
            default:
                break;
        }
    }
    return model;
}
