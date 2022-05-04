//
// DBEndpoint.cc
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

#include "DBEndpoint.hh"
#include "RemoteEndpoint.hh"
#include "CBLiteTool.hh"
#include "Stopwatch.hh"
#include "fleece/Mutable.hh"
#include "Error.hh"
#include <algorithm>
#include <thread>

using namespace std;
using namespace fleece;
using namespace litecore;


static C4Document* c4enum_nextDocument(C4DocEnumerator *e, C4Error *outError) noexcept {
    return c4enum_next(e, outError) ? c4enum_getDocument(e, outError) : nullptr;
}

static string pathOfDB(C4Database *db) {
    C4StringResult path = c4db_getPath(db);
    string src(slice(path.buf, path.size));
    c4slice_free(path);
    return src;
}


DbEndpoint::DbEndpoint(const std::string &spec)
:Endpoint(spec)
{ }

DbEndpoint::DbEndpoint(C4Database *db)
:Endpoint(pathOfDB(db))
,_db(c4db_retain(db))
,_collection(c4db_getDefaultCollection(db))
{ }


DbEndpoint::DbEndpoint(C4Collection *coll)
:DbEndpoint(c4coll_getDatabase(coll))
{
    _collection = coll;
}


void DbEndpoint::prepare(bool isSource, bool mustExist, slice docIDProperty, const Endpoint *other) {
    Endpoint::prepare(isSource, mustExist, docIDProperty, other);
    _otherEndpoint = const_cast<Endpoint*>(other);
    if (!_db) {
        auto [otherDir, otherName] = CBLiteTool::splitDBPath(_spec);
        if (otherName.empty())
            fail("Database filename must have a '.cblite2' extension");
        C4DatabaseConfig2 config = {slice(otherDir), kC4DB_NonObservable};
        if (isSource) {
            if (!other->isDatabase())    // need write permission if replicating, even for push
                config.flags |= kC4DB_ReadOnly;
        } else {
            if (!mustExist)
                config.flags |= kC4DB_Create;
        }
        C4Error err;
        _db = c4db_openNamed(slice(otherName), &config, &err);
        if (!_db)
            LiteCoreTool::instance()->fail(format("Couldn't open database %s", _spec.c_str()), err);
        _openedDB = true;
        _collection = c4db_getDefaultCollection(_db);
    }

    // Only used for writing JSON:
    auto sk = c4db_getFLSharedKeys(_db);
    _encoder.setSharedKeys(sk);
}


alloc_slice DbEndpoint::path() const {
    if (_spec.empty())
        return c4db_getPath(_db);
    else
        return alloc_slice(_spec);

}


void DbEndpoint::enterTransaction() {
    if (!_inTransaction) {
        C4Error err;
        if (!c4db_beginTransaction(_db, &err))
            fail("starting transaction", err);
        _inTransaction = true;
    }
}


// As source:
void DbEndpoint::copyTo(Endpoint *dst, uint64_t limit) {
    // Special cases: database to database (local or remote)
    auto dstDB = dynamic_cast<DbEndpoint*>(dst);
    if (dstDB)
        return pushToLocal(*dstDB);
    auto remoteDB = dynamic_cast<RemoteEndpoint*>(dst);
    if (remoteDB)
        return replicateWith(*remoteDB);
    // Normal case, copying docs (to JSON, presumably):
    exportTo(dst, limit);
}


void DbEndpoint::exportTo(Endpoint *dst, uint64_t limit) {
    if (Tool::instance->verbose())
        cout << "Exporting documents...\n";
    C4EnumeratorOptions options = kC4DefaultEnumeratorOptions;
    C4Error err;
    c4::ref<C4DocEnumerator> e = c4coll_enumerateAllDocs(_collection, &options, &err);
    if (!e)
        fail("enumerating source db", err);
    uint64_t line;
    for (line = 0; line < limit; ++line) {
        c4::ref<C4Document> doc = c4enum_nextDocument(e, &err);
        if (!doc)
            break;
        alloc_slice json = c4doc_bodyAsJSON(doc, false, &err);
        if (!json) {
            errorOccurred("reading document body", err);
            continue;
        }
        dst->writeJSON(doc->docID, json);
    }
    if (err.code)
        errorOccurred("enumerating source db", err);
    else if (line == limit)
        cout << "Stopped after " << limit << " documents.\n";
}


// As destination of JSON file(s):
void DbEndpoint::writeJSON(slice docID, slice json) {
    enterTransaction();

    _encoder.reset();
    if (!_encoder.convertJSON(json)) {
        errorOccurred(format("Couldn't parse JSON: %.*s", SPLAT(json)));
        return;
    }
    Doc body = _encoder.finishDoc();

    // Get the JSON's docIDProperty to use as the document ID:
    alloc_slice docIDBuf;
    if (!docID && _docIDProperty) {
        docID = docIDBuf = docIDFromDict(body, json);
        // Remove the docID property:
        MutableDict root = body.asDict().mutableCopy();
        root.remove(_docIDProperty);
        _encoder.reset();
        _encoder.writeValue(root);
        body = _encoder.finishDoc();
    }

    C4DocPutRequest put { };
    put.docID = docID;
    put.allocedBody = C4SliceResult(body.allocedData());
    put.save = true;
    C4Error err;
    c4::ref<C4Document> doc = c4coll_putDoc(_collection, &put, nullptr, &err);
    if (doc) {
        docID = slice(doc->docID);
    } else {
        if (docID)
            errorOccurred(format("saving document \"%.*s\"", SPLAT(put.docID)), err);
        else
            errorOccurred("saving document", err);
    }

    logDocument(docID);

    if (++_transactionSize >= kMaxTransactionSize) {
        commit();
        enterTransaction();
    }
}


void DbEndpoint::finish() {
    commit();
    C4Error err;
    if (_openedDB && !c4db_close(_db, &err))
        errorOccurred("closing database", err);
}


void DbEndpoint::commit() {
    if (_inTransaction) {
        if (Tool::instance->verbose() > 1) {
            cout << "[Committing ... ";
            cout.flush();
        }
        Stopwatch st;
        C4Error err;
        if (!c4db_endTransaction(_db, true, &err))
            fail("committing transaction", err);
        if (Tool::instance->verbose() > 1) {
            double time = st.elapsed();
            cout << time << " sec for " << _transactionSize << " docs]\n";
        }
        _inTransaction = false;
        _transactionSize = 0;
    }
}


#pragma mark - REPLICATION:


static const char* nameOfMode(C4ReplicatorMode push, C4ReplicatorMode pull) {
    if (push >= kC4OneShot && pull >= kC4OneShot)
        return "Pushing/pulling with";
    if (push >= kC4OneShot)
        return "Pushing to";
    else
        return "Pulling from";
}


void DbEndpoint::startReplicationWith(RemoteEndpoint &remote, bool pushing) {
    auto pushMode = (_continuous ? kC4Continuous : kC4OneShot);
    auto pullMode = (_bidirectional ? pushMode : kC4Disabled);
    if (!pushing)
        swap(pushMode, pullMode);
    if (Tool::instance->verbose())
        cout << nameOfMode(pushMode, pullMode) << " remote database";
    if (_continuous)
        cout << ", continuously";
    cout << "...\n";
    C4ReplicatorParameters params = replicatorParameters(pushMode, pullMode);
    C4Error err;
    startReplicator(c4repl_new(_db, remote.url(), remote.databaseName(), params, &err), err);
}


void DbEndpoint::replicateWith(RemoteEndpoint &remote, bool pushing) {
    startReplicationWith(remote, pushing);
    finishReplication();
}


void DbEndpoint::pushToLocal(DbEndpoint &dst) {
#ifdef COUCHBASE_ENTERPRISE
    auto pushMode = (_continuous ? kC4Continuous : kC4OneShot);
    auto pullMode = (_bidirectional ? pushMode : kC4Disabled);
    if (Tool::instance->verbose())
        cout << nameOfMode(kC4OneShot, pullMode)  << " local database";
    if (_continuous)
        cout << ", continuously";
    cout << "...\n";
    C4ReplicatorParameters params = replicatorParameters(kC4OneShot, pullMode);
    C4Error err;
    startReplicator(c4repl_newLocal(_db, dst._db, params, &err), err);
#else
    error::_throw(error::Domain::LiteCore, kC4ErrorUnimplemented);
#endif
}


void DbEndpoint::startReplicator(C4Replicator *repl, C4Error &err) {
    if (!repl) {
        errorOccurred("starting replication", err);
        fail();
    }
    _replicator = repl;
    _stopwatch.start();
    c4repl_start(_replicator, false);
}


void DbEndpoint::waitTillIdle() {
    C4ReplicatorStatus status;
    do {
        this_thread::sleep_for(chrono::milliseconds(100));
        status = c4repl_getStatus(_replicator);
    } while (status.level != kC4Idle && status.level != kC4Stopped);
    startLine();
    if (status.level == kC4Stopped)
        finishReplication();
}


void DbEndpoint::stopReplication() {
    if (!_replicator)
        return;
    c4repl_stop(_replicator);
    finishReplication();
}


void DbEndpoint::finishReplication() {
    assert(_replicator);
    C4ReplicatorStatus status;
    while ((status = c4repl_getStatus(_replicator)).level != kC4Stopped)
        this_thread::sleep_for(chrono::milliseconds(100));
    _replicator = nullptr;
    startLine();

    if (status.error.code) {
        errorOccurred("replicating", status.error);
        fail();
    }
}


C4ReplicatorParameters DbEndpoint::replicatorParameters(C4ReplicatorMode push, C4ReplicatorMode pull) {
    C4ReplicatorParameters params = {};
    params.push = push;
    params.pull = pull;
    params.callbackContext = this;

    {
        fleece::Encoder enc;
        enc.beginDict();

        //enc[slice(kC4ReplicatorOptionProgressLevel)] = 1;   // callback on every doc
        enc[slice(kC4ReplicatorOptionMaxRetries)] = _maxRetries;

        if (!_credentials.first.empty() || _clientCert) {
            enc.writeKey(slice(kC4ReplicatorOptionAuthentication));
            enc.beginDict();
            enc.writeKey(slice(kC4ReplicatorAuthType));
            if (_clientCert) {
                enc.writeString(kC4AuthTypeClientCert);
                enc.writeKey(slice(kC4ReplicatorAuthClientCert));
                enc.writeData(_clientCert);
                if (_clientCertKey) {
                    enc.writeKey(slice(kC4ReplicatorAuthClientCertKey));
                    enc.writeData(_clientCertKey);
                    if (_clientCertKeyPassword) {
                        enc.writeKey(slice(kC4ReplicatorAuthPassword));
                        enc.writeData(_clientCertKeyPassword);
                    }
                }
            } else {
                enc.writeString(kC4AuthTypeBasic);
                enc.writeKey(slice(kC4ReplicatorAuthUserName));
                enc.writeString(_credentials.first);
                enc.writeKey(slice(kC4ReplicatorAuthPassword));
                enc.writeString(_credentials.second);
            }
            enc.endDict();
        } else if(!_sessionToken.empty()) {
            enc.writeKey(FLSTR(kC4ReplicatorOptionCookies));
            enc.writeString(format("SyncGatewaySession=%s", _sessionToken.c_str()));
        }

        if (_rootCerts) {
            enc.writeKey(slice(kC4ReplicatorOptionRootCerts));
            enc.writeData(_rootCerts);
        }

        enc.endDict();
        _options = enc.finish();
        params.optionsDictFleece = _options;
    }

    params.onStatusChanged = [](C4Replicator *replicator,
                                C4ReplicatorStatus status,
                                void *context)
    {
        ((DbEndpoint*)context)->onStateChanged(status);
    };

    params.onDocumentsEnded = [](C4Replicator *repl,
                                bool pushing,
                                size_t count,
                                const C4DocumentEnded* docs[],
                                void *context)
    {
        ((DbEndpoint*)context)->onDocsEnded(pushing, count, docs);
    };
    return params;
}


void DbEndpoint::onStateChanged(C4ReplicatorStatus status) {
    auto documentCount = status.progress.documentCount;
    if (LiteCoreTool::instance()->verbose()) {
        cout << "\r" << kC4ReplicatorActivityLevelNames[status.level] << " ... ";
        _needNewline = true;
        if (documentCount > 0) {
            auto elapsed = _stopwatch.elapsed();
            cout << documentCount << " documents (";
            cout << int(documentCount / elapsed) << "/sec)";
#if 0
            auto nDocs = documentCount - _lastDocCount;
            if (nDocs > 0) {
                cout << "    (+" << nDocs << ", " << int(nDocs / (elapsed-_lastElapsed)) << "/sec)";
                _lastDocCount = documentCount;
                _lastElapsed = elapsed;
            }
#endif
        }
#if 0 // Progress estimation is too inaccurate, currently (v2.0)
        if (status.progress.unitsTotal > 0) {
            double progress = status.progress.unitsTotal ? (status.progress.unitsCompleted / (double)status.progress.unitsTotal) : 0.0;
            printf("(%.0f%%)", round(progress * 100.0));
        }
#endif
        if (status.level == kC4Stopped)
            startLine();
        cout.flush();
    }

    if (status.error.code != 0) {
        startLine();
        char message[200];
        c4error_getDescriptionC(status.error, message, sizeof(message));
        C4Log("** Replicator error: %s", message);
    }

    setDocCount(documentCount);
    _otherEndpoint->setDocCount(documentCount);
}


void DbEndpoint::onDocsEnded(bool pushing,
                            size_t count,
                            const C4DocumentEnded* docs[])
{
    for(size_t i = 0; i < count; i++) {
        auto doc = docs[i];
        if (doc->error.code == 0) {
           // _otherEndpoint->logDocument(docID);
        } else {
            startLine();
            char message[200];
            c4error_getDescriptionC(doc->error, message, sizeof(message));
            C4Log("** Error %s doc \"%.*s\": %s",
                  (pushing ? "pushing" : "pulling"), FMTSLICE(doc->docID), message);
        }
    }
}


void DbEndpoint::startLine() {
    if (_needNewline) {
        cout << "\n";
        _needNewline = false;
    }
}
