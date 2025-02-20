//
// CBLiteTool.hh
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

#include "LiteCoreTool.hh"
#include "c4.h"
#include "fleece/Fleece.h"
#include "fleece/slice.hh"
#include "FilePath.hh"
#include "StringUtil.hh"
#include <functional>
#include <memory>
#include <string>

#if !defined(LITECORE_API_VERSION) || LITECORE_VERSION < 351
#   error "You are building with an old pre-3.0 version of LiteCore"
#endif

class CBLiteCommand;


class CBLiteTool : public LiteCoreTool {
public:
    CBLiteTool()
    :LiteCoreTool("cblite")
    { }

    CBLiteTool(const CBLiteTool &parent)
    :LiteCoreTool(parent)
    { }

    // Main handlers:
    void usage() override;
    int run() override;

protected:
    virtual void addLineCompletions(ArgumentTokenizer&, std::function<void(const std::string&)>) override;

    std::unique_ptr<CBLiteCommand> subcommand(const std::string &name);

    virtual void helpCommand();

    [[noreturn]] virtual void failMisuse(const std::string &message);
};


/** A wrapper around C4CollectionSpec with backing store for the strings. */
class CollectionSpec {
public:
    /// Constructor takes a 'keyspace' string of the form "collection" or "scope.collection".
    explicit CollectionSpec(std::string keyspace);

    CollectionSpec(C4CollectionSpec const&);

    fleece::slice scope() const {return _spec.scope;}
    fleece::slice name() const {return _spec.name;}

    operator C4CollectionSpec() const noexcept {return _spec;}

    std::string const& keyspace() const noexcept {return _keyspace;}

    /// Utility that checks a collection spec for validity.
    static bool isValid(const C4CollectionSpec&) noexcept;

    friend bool operator< (const CollectionSpec&, const CollectionSpec&) noexcept;

private:
    std::string      _keyspace;
    C4CollectionSpec _spec;
};

static const CollectionSpec kDefaultCollectionSpec{"_default"};
