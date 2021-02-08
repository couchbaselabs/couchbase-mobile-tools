//
// Endpoint.hh
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
#include "Tool.hh"
#include <memory>


/** Abstract base class for a source or target of copying/replication. */
class Endpoint {
public:
    Endpoint(const std::string &spec)
    :_spec(spec)
    { }

    static std::unique_ptr<Endpoint> create(std::string str);
    static std::unique_ptr<Endpoint> create(C4Database*);
    virtual ~Endpoint() { }

    virtual bool isDatabase() const     {return false;}
    virtual bool isRemote() const       {return false;}

    virtual void prepare(bool isSource, bool mustExist, fleece::slice docIDProperty, const Endpoint * other) {
        _docIDProperty = docIDProperty;
        if (_docIDProperty) {
            _docIDPath.reset(new fleece::KeyPath(_docIDProperty, nullptr));
            if (!*_docIDPath)
                Tool::instance->fail("Invalid docID");
        }
    }

    virtual void copyTo(Endpoint*, uint64_t limit) =0;

    virtual void writeJSON(fleece::slice docID, fleece::slice json) = 0;

    virtual void finish() { }

    uint64_t docCount()             {return _docCount;}
    void setDocCount(uint64_t n)    {_docCount = n;}

    void logDocument(fleece::slice docID) {
        ++_docCount;
        if (Tool::instance->verbose() >= 2)
            std::cout << to_string(docID) << std::endl;
        else if (Tool::instance->verbose() == 1 && (_docCount % 1000) == 0)
            std::cout << _docCount << std::endl;
    }

    void logDocuments(unsigned n) {
        _docCount += n;
        if (Tool::instance->verbose() >= 2)
            std::cout << n << " more documents" << std::endl;
        else if (Tool::instance->verbose() == 1 && (_docCount % 1000) < n)
            std::cout << _docCount << std::endl;
    }

protected:

    fleece::alloc_slice docIDFromJSON(fleece::slice json) {
        return docIDFromDict(fleece::Doc::fromJSON(json, nullptr).asDict(), json);
    }

    fleece::alloc_slice docIDFromDict(fleece::Dict root, fleece::slice json) {
        fleece::alloc_slice docIDBuf;
        fleece::Value docIDProp = root[*_docIDPath];
        if (docIDProp) {
            docIDBuf = docIDProp.toString();
            if (!docIDBuf)
                Tool::instance->fail(litecore::format("Property \"%.*s\" is not a scalar in JSON: %.*s", SPLAT(_docIDProperty), SPLAT(json)));
        } else {
            Tool::instance->errorOccurred(litecore::format("No property \"%.*s\" in JSON: %.*s", SPLAT(_docIDProperty), SPLAT(json)));
        }
        return docIDBuf;
    }

    const std::string _spec;
    fleece::Encoder _encoder;
    fleece::alloc_slice _docIDProperty;
    uint64_t _docCount {0};
    std::unique_ptr<fleece::KeyPath> _docIDPath;
};


