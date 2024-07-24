//
// LLMProvider.hh
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

#pragma once

#include "CBLiteCommand.hh"
#include "Response.hh"
#include "Tool.hh"
#include "LiteCoreTool.hh"

class LLMProvider {
public:
    virtual ~LLMProvider() =default;
        
    virtual fleece::alloc_slice run(fleece::Value, const std::string&) =0;
    virtual fleece::Value getEmbedding(fleece::Doc) =0;
    enum Model {TYPE_OpenAI, TYPE_Gemini, TYPE_Bedrock};
    static std::unique_ptr<LLMProvider> create(const std::string&);
protected:
    fleece::alloc_slice run(std::unique_ptr<litecore::REST::Response>&);
};

std::unique_ptr<LLMProvider> newOpenAIModel();
std::unique_ptr<LLMProvider> newGeminiModel();
std::unique_ptr<LLMProvider> newBedrockModel();
