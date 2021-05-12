//
// CBLiteCommand.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "CBLiteTool.hh"
#include <functional>
#include <string>

/** Abstract base class of the 'cblite' tool's subcommands. */
class CBLiteCommand : public CBLiteTool {
public:
    /// Starts interactive mode; returns when user quits
    static void runInteractive(CBLiteTool &parent);
    static void runInteractive(CBLiteTool &parent, const std::string &databasePath);
    static void runInteractiveWithURL(CBLiteTool &parent, const std::string &databaseURL);

    CBLiteCommand(CBLiteTool &parent)
    :CBLiteTool(parent)
    { }

    void setParent(CBLiteCommand *parent) {
        _parent = parent;
        _collectionName = parent->_collectionName;
    }

    virtual void usage() override =0;
    virtual void runSubcommand() =0;

    [[noreturn]] virtual void failMisuse(const std::string &message) override {
        std::cerr << "Error: " << message << std::endl;
        std::cerr << "Please run `cblite help " << name() << "` for usage information." << std::endl;
        fail();
    }

    virtual bool interactive() const                {return _parent && _parent->interactive();}

#ifdef HAS_COLLECTIONS
    C4Collection* collection();
    void setCollectionName(const std::string &name);
#endif

    virtual bool processFlag(const std::string &flag,
                             const std::initializer_list<FlagSpec> &specs) override;

    std::string tempDirectory();

protected:
    void writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs ="");

    void openDatabaseFromNextArg();
    void openWriteableDatabaseFromNextArg();

    c4::ref<C4Document> readDoc(std::string docID, C4DocContentLevel);

    void rawPrint(fleece::Value body, fleece::slice docID, fleece::slice revID =fleece::nullslice);
    void prettyPrint(fleece::Value value,
                     std::ostream &out,
                     const std::string &indent ="",
                     fleece::slice docID =fleece::nullslice,
                     fleece::slice revID =fleece::nullslice,
                     const std::set<fleece::alloc_slice> *onlyKeys =nullptr);

    void getDBSizes(uint64_t &dbSize, uint64_t &blobsSize, uint64_t &nBlobs);
    std::tuple<fleece::alloc_slice, fleece::alloc_slice, fleece::alloc_slice> getCertAndKeyArgs();

    static void writeSize(uint64_t n);
    static bool canBeUnquotedJSON5Key(fleece::slice key);

    static bool isGlobPattern(std::string &str);
    static void unquoteGlobPattern(std::string &str);
    bool globMatch(const char *name, const char *pattern);

    struct EnumerateDocsOptions {
#ifdef HAS_COLLECTIONS
        C4Collection* collection = nullptr;
#endif
        C4EnumeratorFlags flags = kC4IncludeNonConflicted;
        bool bySequence = false;
        int64_t offset = 0, limit = -1;
        std::string pattern;
    };

    using EnumerateDocsCallback = fleece::function_ref<void(const C4DocumentInfo&,
                                                            C4DocEnumerator*)>;
    int64_t enumerateDocs(EnumerateDocsOptions, EnumerateDocsCallback);

    void addDocIDCompletions(ArgumentTokenizer&, std::function<void(const std::string&)> add);

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

    CBLiteCommand*                  _parent {nullptr};
    std::string                     _collectionName;

    std::string                     _certFile;
    C4EnumeratorFlags               _enumFlags {kC4IncludeNonConflicted};
    bool                            _json5 {false};
    std::set<fleece::alloc_slice>   _keys;
    int64_t                         _limit {-1};
    uint64_t                        _offset {0};
    bool                            _prettyPrint {true};


private:
    virtual int run() override {return -1;}
};


#pragma mark - FACTORY FUNCTIONS:


CBLiteCommand* newCatCommand(CBLiteTool&);
CBLiteCommand* newCheckCommand(CBLiteTool&);
CBLiteCommand* newCompactCommand(CBLiteTool&);
CBLiteCommand* newCpCommand(CBLiteTool&);
CBLiteCommand* newEditCommand(CBLiteTool&);
CBLiteCommand* newExportCommand(CBLiteTool&);
CBLiteCommand* newImportCommand(CBLiteTool&);
CBLiteCommand* newInfoCommand(CBLiteTool&);
CBLiteCommand* newLogcatCommand(CBLiteTool&);
CBLiteCommand* newListCommand(CBLiteTool&);
CBLiteCommand* newOpenCommand(CBLiteTool&);
CBLiteCommand* newOpenRemoteCommand(CBLiteTool&);
CBLiteCommand* newPullCommand(CBLiteTool&);
CBLiteCommand* newPushCommand(CBLiteTool&);
CBLiteCommand* newPutCommand(CBLiteTool&);
CBLiteCommand* newQueryCommand(CBLiteTool&);
CBLiteCommand* newReindexCommand(CBLiteTool&);
CBLiteCommand* newRevsCommand(CBLiteTool&);
CBLiteCommand* newRmCommand(CBLiteTool&);
CBLiteCommand* newServeCommand(CBLiteTool&);
CBLiteCommand* newSelectCommand(CBLiteTool&);
CBLiteCommand* newSQLCommand(CBLiteTool&);
#ifdef HAS_COLLECTIONS
CBLiteCommand* newCdCommand(CBLiteTool&);
CBLiteCommand* newMkCollCommand(CBLiteTool&);
CBLiteCommand* newMvCommand(CBLiteTool&);
#endif
#ifdef COUCHBASE_ENTERPRISE
CBLiteCommand* newEncryptCommand(CBLiteTool&);
CBLiteCommand* newDecryptCommand(CBLiteTool&);
#endif

void pullRemoteDatabase(CBLiteTool &parent, const std::string &url);
