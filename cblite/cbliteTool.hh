//
// cbliteTool.hh
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
#include "c4.h"
#include "FilePath.hh"
#include "StringUtil.hh"
#include <exception>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <vector>

using namespace std;
using namespace fleece;

namespace litecore {
    class MultiLogDecoder;
}

class Endpoint;
class DbEndpoint;


enum PutMode {kPut, kUpdate, kCreate, kDelete};

// Per-subcommand flags are separated into their own struct so that they can be reset to
// default values easily (see the clearFlags() method below.)
// CBLiteTool inherits from this struct, so it can reference the flags as member variables.
struct CBLiteFlags {
    uint64_t                _offset {0};
    int64_t                 _limit {-1};
    alloc_slice             _startKey, _endKey;
    std::set<alloc_slice>   _keys;
    C4EnumeratorFlags       _enumFlags {kC4IncludeNonConflicted};
    bool                    _longListing {false};
    bool                    _listBySeq {false};
    bool                    _prettyPrint {true};
    bool                    _json5 {false};
    bool                    _showRevID {false};
    bool                    _showRemotes {false};
    bool                    _showHelp {false};
    bool                    _createDst {true};
    bool                    _bidi {false};
    bool                    _continuous {false};
    bool                    _replicate {false};
    bool                    _explain {false};
    bool                    _dbNeedsPassword {false};
    bool                    _full {false};
    bool                    _csv {false};
    alloc_slice             _jsonIDProperty {"_id"};
    string                  _user;
    PutMode                 _putMode {kPut};
    std::string             _listenerDirectory;
    C4ListenerConfig        _listenerConfig {};  // all false/0
    std::string             _outputFile;
    std::string             _certFile;
    std::string             _rootCertsFile;
};


class CBLiteTool : public Tool, public CBLiteFlags {
public:
    CBLiteTool() : Tool("cblite") {
    }

    virtual ~CBLiteTool() {
        c4db_release(_db);
    }

    // Main handlers:
    void usage() override;
    int run() override;

private:

    bool isDatabasePath(const string &path);
    void openDatabase(string path);
    void openDatabaseFromNextArg();
    void openWriteableDatabaseFromNextArg();

    // cat command
    void catUsage();
    void catDocs();
    void catDoc(C4Document *doc, bool includeID);

    // cp command
    void cpUsage();
    void copyDatabase(bool reversed);
    void copyDatabase()                                     {copyDatabase(false);}
    void copyDatabaseReversed()                             {copyDatabase(true);}
    void copyDatabase(Endpoint *src, Endpoint *dst);
    void copyLocalToLocalDatabase(DbEndpoint *src, DbEndpoint *dst);
    std::tuple<alloc_slice, alloc_slice, alloc_slice> getCertAndKeyArgs();

#ifdef COUCHBASE_ENTERPRISE
    // encrypt / decrypt commands
    void encrypt()                  {rekey(true);}
    void decrypt()                  {rekey(false);}
    void encryptUsage();
    void decryptUsage();
    void rekey(bool encrypting);
#endif

    // info / file command
    void fileUsage();
    void fileInfo();
    void indexInfo(const string &name ="");
    void getDBSizes(uint64_t &dbSize, uint64_t &blobsSize, uint64_t &nBlobs);
    void getTotalDocSizes(uint64_t &dataSize, uint64_t &metaSize, uint64_t &conflictCount);
    uint64_t countDocsWhere(const char *what);

    // logcat command
    void logcatUsage();
    void logcat();
    void writeLog(litecore::MultiLogDecoder &decoder, ostream &out);
    void writeLogCSV(litecore::MultiLogDecoder &decoder, ostream &out);

    // ls command
    void listUsage();
    void listDocsCommand();
    void listDocs(string docIDPattern);

    // put command
    void putUsage();
    void putDoc();

    // query command
    void queryUsage();
    void selectUsage();
    void queryDatabase();
    C4Query* compileQuery(C4QueryLanguage, string queryStr, size_t *outErrorPos, C4Error*);
    void displayQueryAsJSON(C4Query*, C4QueryEnumerator*);
    void displayQueryAsTable(C4Query*, C4QueryEnumerator*);

    // revs command
    void revsUsage();
    void revsInfo();

    // serve command
    void serveUsage();
    void serve();
    void startListener();
    void shareDatabase(const char *path, string name);
    void shareDatabaseDir(const char *dirPath);

    // sql command
    void sqlUsage();
    void sqlQuery();

    // shell command
    void shell();
    void runInteractively();
    void helpCommand();
    void quitCommand();

    using RevTree = map<alloc_slice,set<alloc_slice>>; // Maps revID to set of child revIDs
    using RemoteMap = vector<alloc_slice>;

    void writeRevisionTree(C4Document *doc,
                           RevTree &tree,
                           RemoteMap &remotes,
                           alloc_slice root,
                           int metaColumn,
                           int indent);
    void writeRevisionChildren(C4Document *doc,
                               RevTree &tree,
                               RemoteMap &remotes,
                               alloc_slice root,
                               int metaColumn,
                               int indent);

#pragma mark - UTILITIES:


    [[noreturn]] virtual void failMisuse(const string &message) override {
        cerr << "Error: " << message << "\n";
        if (_currentCommand.empty())
            cerr << "Please run `cblite help` for usage information.\n";
        else
            cerr << "Please run `cblite help " << _currentCommand << "` for usage information.\n";
        fail();
    }

    static void writeSize(uint64_t n);

    void writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs ="");

    c4::ref<C4Document> readDoc(string docID);

    void rawPrint(Value body, slice docID, slice revID =nullslice);
    void prettyPrint(Value value,
                     const string &indent ="",
                     slice docID =nullslice,
                     slice revID =nullslice,
                     const std::set<alloc_slice> *onlyKeys =nullptr);

    static bool canBeUnquotedJSON5Key(slice key);

    static bool isGlobPattern(string &str);
    static void unquoteGlobPattern(string &str);


#pragma mark - FLAGS:


    void clearFlags() {
        *(CBLiteFlags*)this = CBLiteFlags();
        _verbose = 0;
    }

    virtual const FlagSpec* initialFlags() override {
        return kPreCommandFlags;
    }

    void bidiFlag()      {_bidi = true;}
    void bodyFlag()      {_enumFlags |= kC4IncludeBodies;}
    void carefulFlag()   {_failOnError = true;}
    void certFlag()      {_certFile = nextArg("certificate path");}
    void confFlag()      {_enumFlags &= ~kC4IncludeNonConflicted;}
    void continuousFlag(){_continuous = true;}
    void createDBFlag()  {_dbFlags |= kC4DB_Create; _dbFlags &= ~kC4DB_ReadOnly;}
    void createDocFlag() {_putMode = kCreate;}
    void csvFlag()       {_csv = true;}
    void delFlag()       {_enumFlags |= kC4IncludeDeleted;}
    void deleteDocFlag() {_putMode = kDelete;}
    void descFlag()      {_enumFlags |= kC4Descending;}
    void dirFlag()       {_listenerDirectory = nextArg("directory");}
    void encryptedFlag() {_dbNeedsPassword = true;}
    void existingFlag()  {_createDst = false;}
    void explainFlag()   {_explain = true;}
    void fullFlag()      {_full = true;}
    void helpFlag()      {_showHelp = true;}
    void json5Flag()     {_json5 = true; _enumFlags |= kC4IncludeBodies;}
    void jsonIDFlag()    {_jsonIDProperty = nextArg("JSON-id property");}
    void keyFlag()       {_keys.insert(alloc_slice(nextArg("key")));}
    void limitFlag()     {_limit = stol(nextArg("limit value"));}
    void longListFlag()  {_longListing = true;}
    void offsetFlag()    {_offset = stoul(nextArg("offset value"));}
    void outFlag()       {_outputFile = nextArg("output file");}
    void portFlag()      {_listenerConfig.port = (uint16_t)stoul(nextArg("port"));}
    void prettyFlag()    {_prettyPrint = true; _enumFlags |= kC4IncludeBodies;}
    void rawFlag()       {_prettyPrint = false; _enumFlags |= kC4IncludeBodies;}
    void readonlyFlag()  {_dbFlags = (_dbFlags | kC4DB_ReadOnly) & ~kC4DB_Create;}
    void remotesFlag()   {_showRemotes = true;}
    void replicateFlag() {_replicate = true;}
    void revIDFlag()     {_showRevID = true;}
    void rootCertsFlag() {_rootCertsFile = nextArg("root certificates path");}
    void seqFlag()       {_listBySeq = true;}
    void updateDocFlag() {_putMode = kUpdate;}
    void userFlag()      {_user = nextArg("user name for replication");}
    void versionFlag();
    void writeableFlag() {_dbFlags &= ~kC4DB_ReadOnly;}

    static const FlagSpec kPreCommandFlags[];
    static const FlagSpec kSubcommands[];
    static const FlagSpec kInteractiveSubcommands[];
    static const FlagSpec kCatFlags[];
    static const FlagSpec kCpFlags[];
    static const FlagSpec kFileFlags[];
    static const FlagSpec kListFlags[];
    static const FlagSpec kLogcatFlags[];
    static const FlagSpec kPutFlags[];
    static const FlagSpec kQueryFlags[];
    static const FlagSpec kRevsFlags[];
    static const FlagSpec kServeFlags[];

    C4Database* _db {nullptr};
    C4Listener* _listener {nullptr};
    string _currentCommand;

    C4DatabaseFlags _dbFlags {kC4DB_SharedKeys | kC4DB_NonObservable | kC4DB_ReadOnly};
    bool _interactive {false};
};
