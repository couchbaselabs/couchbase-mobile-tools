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
#include <string>
#include <iostream>
#include <fstream>
#include "fleece/Fleece.hh"
#include "StringUtil.hh"
#include "fleece/Mutable.hh"
#include "CBLiteTool.hh"
#include "LiteCoreTool.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

LLMProvider* LLMProvider::instance;

fleece::alloc_slice LLMProvider::run(const std::string& restBody, C4Error error, unique_ptr<litecore::REST::Response>& r) {
    auto headers = getHeaders(restBody);
    r->setHeaders(headers).setBody(restBody);
    alloc_slice response = errorHandle(r, error);
    return response;
}

Doc LLMProvider::getHeaders(const string& restBody) {
    // Get headers and check for API key
    Encoder enc;
    enc.beginDict();
    enc["Content-Type"_sl] = "application/json";
    enc["Content-Length"_sl] = restBody.length();
    if (getenv("LLM_API_KEY") == NULL) {
        cout << "API Key not provided\n";
        return NULL;
    }
    
    enc["Authorization"] = format("Bearer %s", getenv("LLM_API_KEY"));
    enc.endDict();
    return enc.finishDoc();
}

fleece::alloc_slice LLMProvider::errorHandle(unique_ptr<litecore::REST::Response>& r, C4Error error) {
    // Check for request errors
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
        }
        return NULL;
    }
    
    return response;
}

std::unique_ptr<LLMProvider> create(const std::string& modelName) {
    // Initialize model and dict
    unique_ptr<LLMProvider> model;
    map <string, LLMProvider::Model> modelsDict = {{"text-embedding-3-small", LLMProvider::Model::TYPE_OpenAI}, {"text-embedding-3-large", LLMProvider::Model::TYPE_OpenAI}, {"text-embedding-ada-002", LLMProvider::Model::TYPE_OpenAI}, {"models/text-embedding-004", LLMProvider::Model::TYPE_Gemini}, {"amazon.titan-embed-text-v2:0", LLMProvider::Model::TYPE_Bedrock}, {"amazon.titan-embed-text-v1", LLMProvider::Model::TYPE_Bedrock}};
    
    // Determine which model to use
    if(modelsDict.find(modelName) != modelsDict.end()){
        LLMProvider::Model provider = modelsDict[modelName];
        switch (provider) {
            case LLMProvider::Model::TYPE_OpenAI:
                model = newOpenAIModel();
                break;
            case LLMProvider::Model::TYPE_Gemini:
                model = newGeminiModel();
                break;
            case LLMProvider::Model::TYPE_Bedrock:
                model = newBedrockModel();
                break;
            default:
                break;
        }
    }
    return model;
}

