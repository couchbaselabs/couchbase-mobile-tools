//
// CBRemoteCommand.hh
//
// Copyright © 2022 Couchbase. All rights reserved.
//

#pragma once
#pragma once
#include "CBRemoteTool.hh"
#include <functional>
#include <string>

/** Abstract base class of the 'cbremote' tool's subcommands. */
class CBRemoteCommand : public CBRemoteTool {
public:
    /// Starts interactive mode; returns when user quits
    static void runInteractive(CBRemoteTool &parent);
    static void runInteractive(CBRemoteTool &parent, const std::string &databaseURL);

    CBRemoteCommand(CBRemoteTool &parent)
    :CBRemoteTool(parent)
    {
        if (auto parentCmd = dynamic_cast<CBRemoteCommand*>(&parent); parentCmd) {
            _parent = parentCmd;
            _collectionName = parentCmd->_collectionName;
        }
    }

    virtual void usage() override =0;
    virtual void runSubcommand() =0;

    [[noreturn]] virtual void failMisuse(const std::string &message) override {
        std::cerr << "Error: " << message << std::endl;
        std::cerr << "Please run `cbremote help " << name() << "` for usage information." << std::endl;
        fail();
    }

    virtual bool interactive() const                {return _parent && _parent->interactive();}

    C4Collection* collection();
    void setCollectionName(const std::string &name);

    virtual bool processFlag(const std::string &flag,
                             const std::initializer_list<FlagSpec> &specs) override;

protected:
    void writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs ="");

    void openDatabaseFromNextArg();

#pragma mark - COMMON FLAGS:

    void bodyFlag()      {_enumFlags |= kC4IncludeBodies;}
    void certFlag()      {_certFile = nextArg("certificate path");}
    void confFlag()      {_enumFlags &= ~kC4IncludeNonConflicted;}
    void delFlag()       {_enumFlags |= kC4IncludeDeleted;}
    void descFlag()      {_enumFlags |= kC4Descending;}
    void json5Flag()     {_json5 = true; _enumFlags |= kC4IncludeBodies;}
    void keyFlag()       {_keys.insert(fleece::alloc_slice(nextArg("key")));}
    void limitFlag()     {_limit = stol(nextArg("limit value"));}
    void offsetFlag()    {_offset = stoul(nextArg("offset value"));}
    void prettyFlag()    {_prettyPrint = true; _enumFlags |= kC4IncludeBodies;}
    void rawFlag()       {_prettyPrint = false; _enumFlags |= kC4IncludeBodies;}

    CBRemoteCommand*                _parent {nullptr};
    std::string                     _collectionName;

    std::string                     _certFile;
    C4EnumeratorFlags               _enumFlags {kC4IncludeNonConflicted};
    std::set<fleece::alloc_slice>   _keys;
    int64_t                         _limit {-1};
    uint64_t                        _offset {0};
    bool                            _prettyPrint {true};


private:
    virtual int run() override {return -1;}
};


#pragma mark - FACTORY FUNCTIONS:


CBRemoteCommand* newCatCommand(CBRemoteTool&);
CBRemoteCommand* newListCommand(CBRemoteTool&);
CBRemoteCommand* newOpenCommand(CBRemoteTool&);
CBRemoteCommand* newPutCommand(CBRemoteTool&);
CBRemoteCommand* newRmCommand(CBRemoteTool&);
