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

#include "Bedrock.hh"

using namespace std;
using namespace fleece;
using namespace litecore;

alloc_slice Bedrock::run(const string& restBody, C4Error error) {
    // Run request
    auto headers = getHeaders(restBody);
    auto r = std::make_unique<REST::Response>("https", "POST", "bedrock-runtime.us-east-1.amazonaws.com", 443, "model/amazon.titan-embed-text-v2:0");
    r->setHeaders(headers).setBody(restBody);
    alloc_slice response = errorHandle(r, error);
    return response;
}

unique_ptr<LLMProvider> newBedrockModel() {
    unique_ptr<Bedrock> ptr(new Bedrock);
    return ptr;
}
