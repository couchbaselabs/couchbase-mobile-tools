//
// Bedrock.cc
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

// Untested
#include "Bedrock.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

alloc_slice Bedrock::run(Value rawSrcPropValue, const string& modelName) {
    string restBody = format("{\"input\":\"%.*s\", \"model\":\"%s\"}", SPLAT(rawSrcPropValue.asString()), modelName.c_str());
    
    // Get headers
    Encoder enc;
    enc.beginDict();
    enc["accept:"_sl] = "*/*";
    enc["content-type:"_sl] = "application/json";
    enc.endDict();
    auto headers = enc.finishDoc();
    
    // Run request
    auto r = std::make_unique<REST::Response>("https", "POST", "bedrock-runtime.us-east-1.amazonaws.com", 443, format("model/%s/invoke", modelName.c_str()));
    r->setHeaders(headers).setBody(restBody);
    return LLMProvider::run(r);
}

Value Bedrock::getEmbedding(Doc newDoc) {
    return newDoc.asDict()["data"].asArray()[0].asDict()["embedding"];
}
