//
// EnrichCommand.hh
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
#include "LLMProvider.hh"

class EnrichCommand : public CBLiteCommand {
public:
    EnrichCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }
    void usage() override;
    void runSubcommand() override;
private:
    void enrichDocs(const std::string&, const std::string&, std::unique_ptr<LLMProvider>&, const std::string&);
    std::tuple <std::map<std::string, fleece::MutableDict>, std::vector<fleece::Value>, int64_t> readData(const std::string&, EnumerateDocsOptions);
    void writeResult(std::map<std::string, fleece::MutableDict>, std::vector<fleece::alloc_slice>, EnumerateDocsOptions, const std::string&, std::unique_ptr<LLMProvider>&);
};
