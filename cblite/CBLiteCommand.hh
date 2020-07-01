//
// CBLiteCommand.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "CBLiteTool.hh"
#include <functional>

/** Abstract base class of the 'cblite' tool's subcommands. */
class CBLiteCommand : public CBLiteTool {
public:
    CBLiteCommand(const CBLiteTool &parent)
    :CBLiteTool(parent)
    { }

    virtual void usage() override =0;
    virtual void runSubcommand() =0;

    [[noreturn]] virtual void failMisuse(const string &message) override {
        cerr << "Error: " << message << "\n";
        cerr << "Please run `cblite help " << name() << "` for usage information.\n";
        fail();
    }

protected:
    c4::ref<C4Document> readDoc(string docID);

    void rawPrint(Value body, slice docID, slice revID =nullslice);
    void prettyPrint(Value value,
                     const string &indent ="",
                     slice docID =nullslice,
                     slice revID =nullslice,
                     const std::set<alloc_slice> *onlyKeys =nullptr);

    void enumerateDocs(C4EnumeratorFlags flags, function<bool(C4DocEnumerator*)> callback);
    void getDBSizes(uint64_t &dbSize, uint64_t &blobsSize, uint64_t &nBlobs);
    std::tuple<alloc_slice, alloc_slice, alloc_slice> getCertAndKeyArgs();

    static void writeSize(uint64_t n);
    static bool canBeUnquotedJSON5Key(slice key);
    static bool isGlobPattern(string &str);
    static void unquoteGlobPattern(string &str);

#pragma mark - COMMON FLAGS:

    void bodyFlag()      {_enumFlags |= kC4IncludeBodies;}
    void certFlag()      {_certFile = nextArg("certificate path");}
    void confFlag()      {_enumFlags &= ~kC4IncludeNonConflicted;}
    void delFlag()       {_enumFlags |= kC4IncludeDeleted;}
    void descFlag()      {_enumFlags |= kC4Descending;}
    void json5Flag()     {_json5 = true; _enumFlags |= kC4IncludeBodies;}
    void keyFlag()       {_keys.insert(alloc_slice(nextArg("key")));}
    void limitFlag()     {_limit = stol(nextArg("limit value"));}
    void offsetFlag()    {_offset = stoul(nextArg("offset value"));}
    void prettyFlag()    {_prettyPrint = true; _enumFlags |= kC4IncludeBodies;}
    void rawFlag()       {_prettyPrint = false; _enumFlags |= kC4IncludeBodies;}

    std::string             _certFile;
    C4EnumeratorFlags       _enumFlags {kC4IncludeNonConflicted};
    bool                    _json5 {false};
    std::set<alloc_slice>   _keys;
    int64_t                 _limit {-1};
    uint64_t                _offset {0};
    bool                    _prettyPrint {true};


private:
    virtual int run() override {return -1;}
};


#pragma mark - FACTORY FUNCTIONS:


CBLiteCommand* newCatCommand(CBLiteTool&);
CBLiteCommand* newCheckCommand(CBLiteTool&);
CBLiteCommand* newCompactCommand(CBLiteTool&);
CBLiteCommand* newCpCommand(CBLiteTool&);
CBLiteCommand* newImportCommand(CBLiteTool&);
CBLiteCommand* newExportCommand(CBLiteTool&);
CBLiteCommand* newPushCommand(CBLiteTool&);
CBLiteCommand* newPullCommand(CBLiteTool&);
CBLiteCommand* newInfoCommand(CBLiteTool&);
CBLiteCommand* newLogcatCommand(CBLiteTool&);
CBLiteCommand* newListCommand(CBLiteTool&);
CBLiteCommand* newPutCommand(CBLiteTool&);
CBLiteCommand* newQueryCommand(CBLiteTool&);
CBLiteCommand* newReindexCommand(CBLiteTool&);
CBLiteCommand* newRevsCommand(CBLiteTool&);
CBLiteCommand* newRmCommand(CBLiteTool&);
CBLiteCommand* newServeCommand(CBLiteTool&);
CBLiteCommand* newSelectCommand(CBLiteTool&);
CBLiteCommand* newSQLCommand(CBLiteTool&);
#ifdef COUCHBASE_ENTERPRISE
CBLiteCommand* newEncryptCommand(CBLiteTool&);
CBLiteCommand* newDecryptCommand(CBLiteTool&);
#endif
