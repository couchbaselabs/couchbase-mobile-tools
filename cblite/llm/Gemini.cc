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

// LiteCore Request and Response
alloc_slice Gemini::run(const string& restBody, C4Error error) {
    Encoder enc;
    enc.beginDict();
    enc["Content-Type"_sl] = "application/json";
    enc["Content-Length"_sl] = restBody.length();
    if (getenv("API_KEY") == NULL) {
        cout << "API Key not provided\n";
        return 0;
    }
    
    enc["Authorization"] = format("Bearer %s", getenv("API_KEY"));
    enc.endDict();
    auto headers = enc.finishDoc();
    auto r = std::make_unique<REST::Response>("https", "POST", "generativelanguage.googleapis.com", 443, "models/text-embedding-004");
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
        }
        return NULL;
    }
    return response;
}

Model* newGeminiModel() {
    return new Gemini();
}
