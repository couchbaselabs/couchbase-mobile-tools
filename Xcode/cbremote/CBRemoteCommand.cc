//
// CBRemoteCommand.cc
//
// Copyright © 2022 Couchbase. All rights reserved.
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

#include "CBRemoteCommand.hh"

using namespace litecore;
using namespace std;
using namespace fleece;


void CBRemoteCommand::writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs) {
    cerr << ansiBold();
    if (!interactive())
        cerr << "cblite ";
    cerr << cmd << ' ' << ansiItalic();
    if (hasFlags)
        cerr << "[FLAGS]" << ' ';
    if (!interactive())
        cerr << "DBPATH ";
    cerr << otherArgs << ansiReset() << "\n";
}


bool CBRemoteCommand::processFlag(const std::string &flag,
                                const std::initializer_list<FlagSpec> &specs)
{
    if (CBRemoteTool::processFlag(flag, specs)) {
        return true;
#ifdef HAS_COLLECTIONS
    } else if (flag == "--collection") {
        setCollectionName(nextArg("collection name"));
        return true;
#endif
    } else {
        return false;
    }
}


void CBRemoteCommand::openDatabaseFromNextArg() {
    if (!_db)
        openDatabase(nextArg("database URL"), false);
}


void CBRemoteCommand::setCollectionName(const std::string &name) {
    _collectionName = name;
    if (_parent)
        _parent->setCollectionName(name);
}
