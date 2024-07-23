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

alloc_slice Gemini::run(Value rawSrcPropValue, const string& modelName) {
    string restBody = format("{\"model\": \"models/%s\", \"content\": {\"parts\": [{\"text\": \"%.*s\"}]}, }'", modelName.c_str(), SPLAT(rawSrcPropValue.asString()));
    
    // Get headers
    Encoder enc;
    enc.beginDict();
    enc["Content-Type"_sl] = "application/json";
    enc["x-goog-api-key"_sl] = format("${%s}", getenv("LLM_API_KEY"));
    enc.endDict();
    auto headers = enc.finishDoc();
    
    // Run request
    auto r = std::make_unique<REST::Response>("https", "POST", "generativelanguage.googleapis.com", 443, format("v1beta/models/%s%%3AembedContent", modelName.c_str()));
    r->setHeaders(headers).setBody(restBody);
    return LLMProvider::run(r);
}

unique_ptr<LLMProvider> newGeminiModel() {
    return std::make_unique<Gemini>();
}
