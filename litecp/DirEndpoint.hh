//
// DirEndpoint.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
#include "Endpoint.hh"
#include "FilePath.hh"
#include <fstream>


class DirectoryEndpoint : public Endpoint {
public:
    DirectoryEndpoint(const std::string &spec)
    :Endpoint(spec)
    ,_dir(spec, "")
    { }

    virtual void prepare(bool isSource, bool mustExist, fleece::slice docIDProperty, const Endpoint*) override;
    virtual void copyTo(Endpoint*, uint64_t limit) override;
    virtual void writeJSON(fleece::slice docID, fleece::slice json) override;

private:
    fleece::slice readFile(const std::string &path, fleece::alloc_slice &buffer);

    litecore::FilePath _dir;
};

