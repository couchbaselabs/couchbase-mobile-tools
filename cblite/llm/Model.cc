//
// Model.cc
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

#include "Model.hh"
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

Model* Model::instance;

Model::Model()
{
    if(!instance) {
        instance = this;
    }
}

Model::~Model() {
    if (this == instance) {
        instance = nullptr;
    }
}
