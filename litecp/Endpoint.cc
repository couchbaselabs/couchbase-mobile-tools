//
// Endpoint.cc
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

#include "Endpoint.hh"
#include "DBEndpoint.hh"
#include "RemoteEndpoint.hh"
#include "JSONEndpoint.hh"
#include "DirEndpoint.hh"
#include "c4Database.h"

using namespace std;
using namespace litecore;
using namespace fleece;


unique_ptr<Endpoint> Endpoint::create(string desc, std::vector<CollectionName> const& collections) {
    if (hasPrefix(desc, "ws://") || hasPrefix(desc, "wss://")) {
        return createRemote(desc);
    }
    if (desc.find("://") != string::npos) {
        throw runtime_error("Replication URLs must use the 'ws:' or 'wss:' schemes");
    }

#ifndef _MSC_VER
    if (hasPrefix(desc, "~/")) {
        desc.erase(desc.begin(), desc.begin()+1);
        desc.insert(0, getenv("HOME"));
    }
#endif

    if (hasSuffix(desc, kC4DatabaseFilenameExtension)) {
        return make_unique<DbEndpoint>(desc, collections);
    } else if (hasSuffix(desc, ".json")) {
        return make_unique<JSONEndpoint>(desc);
    } else if (hasSuffix(desc, FilePath::kSeparator)) {
        return make_unique<DirectoryEndpoint>(desc);
    } else {
        if (FilePath(desc).existsAsDir() || desc.find('.') == string::npos)
            throw runtime_error("Unknown endpoint (directory path needs to end with a separator)");
        throw runtime_error("Unknown endpoint type");
    }
}


unique_ptr<Endpoint> Endpoint::createRemote(string str) {
    return make_unique<RemoteEndpoint>(str);
}


unique_ptr<Endpoint> Endpoint::create(C4Database *db, std::vector<CollectionName> collections) {
    return make_unique<DbEndpoint>(db, std::move(collections));
}
