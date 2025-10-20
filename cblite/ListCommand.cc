//
// ListCommand.cc
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

#include "ListCommand.hh"
#include "c4Collection.hh"
#include "c4Database.hh"
#include <iomanip>

using namespace std;
using namespace litecore;
using namespace fleece;


static constexpr int kListColumnWidth = 24;


void ListCommand::usage() {
    if (_listCollections) {
        writeUsageCommand("lscoll", false);
        cerr <<
        "  Lists the collections in the database, with document counts etc.\n";
    } else {
        writeUsageCommand("ls", true, "[PATTERN]");
        cerr <<
        "  Lists the IDs, and optionally other metadata, of the documents in the database.\n"
        "    -l : Long format (one doc per line, with metadata)\n"
        "    --offset N : Skip first N docs\n"
        "    --limit N  : Stop after N docs\n"
        "    --desc     : Descending order\n"
        "    --seq      : Order by sequence, not docID\n"
        "    --del      : Include deleted documents\n"
        "    --conf     : Show only conflicted documents\n"
        "    --body     : Display document bodies\n"
        "    --raw      : Show revIDs as-is, and don't pretty-print doc bodies\n"
        "    --json5    : JSON5 syntax, i.e. unquoted dict keys (implies --body)\n"
        "    -c         : List collections, not documents (same as `lscoll` command)\n"
        "    " << it("PATTERN") << " : pattern for matching docIDs, with shell-style wildcards '*', '?'\n"
        ;
    }
}


void ListCommand::runSubcommand() {
    // Read params:
    if (!_listCollections) {
        processFlags({
            {"--offset", [&]{offsetFlag();}},
            {"--limit",  [&]{limitFlag();}},
            {"-l",       [&]{_longListing = true;}},
            {"--body",   [&]{bodyFlag();}},
            {"--pretty", [&]{_prettyPrint = true;}},  // note: it's true by default
            {"--raw",    [&]{_prettyPrint = false;}},
            {"--json5",  [&]{json5Flag();}},
            {"--desc",   [&]{descFlag();}},
            {"--seq",    [&]{_listBySeq = true;}},
            {"--del",    [&]{delFlag();}},
            {"--conf",   [&]{confFlag();}},
            {"-c",       [&]{_listCollections = true;}},
            {"--collections", [&]{_listCollections = true;}},
        });
    }
    openDatabaseFromNextArg();
    string docIDPattern;
    if (hasArgs() && !_listCollections)
        docIDPattern = nextArg("docID pattern");
    endOfArgs();

    if (_listCollections)
        listCollections();
    else
        listDocs(docIDPattern);
}


void ListCommand::listDocs(string docIDPattern) {
    EnumerateDocsOptions options;
    options.flags       = _enumFlags;
    options.bySequence  = _listBySeq;
    options.offset      = _offset;
    options.limit       = _limit;
    options.pattern     = docIDPattern;

    if (_offset > 0)
        cout << "(Skipping first " << _offset << " docs)\n";

    int xpos = 0;
    int lineWidth = terminalWidth();
    int revIDWidth;
    if (usingVersionVectors())
        revIDWidth = _prettyPrint ? 30 : (16+1+22);   // In raw mode, use max expected length
    else
        revIDWidth = _prettyPrint ? 15 : (3+1+40);

    bool firstDoc = true;
    int64_t nDocs = enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
        int idWidth = (int)info.docID.size;        //TODO: Account for UTF-8 chars
        if (_enumFlags & kC4IncludeBodies) {
            // 'cat' form:
            if (!firstDoc)
                cout << "\n";

            firstDoc = false;
            catDoc(doc, true);

        } else if (_longListing) {
            // Long form:
            if (firstDoc) {
                firstDoc = false;
                cout << ansi("4") << "Document ID             "
                     << setw(revIDWidth) << left << "Rev ID" << right << "  Flags   Seq     Size"
                     << ansiReset() << "\n";
            } else {
                cout << "\n";
            }

            string revID = formatRevID(info.revID, _prettyPrint);
            if (revID.size() > revIDWidth)
                revID = revID.substr(0, revIDWidth - 3) + "...";
            else
                revID.resize(revIDWidth, ' ');

            cout << info.docID << spaces(kListColumnWidth - idWidth);
            cout << revID << "  ";
            cout << ((info.flags & kDocDeleted)        ? 'd' : '-');
            cout << ((info.flags & kDocConflicted)     ? 'c' : '-');
            cout << ((info.flags & kDocHasAttachments) ? 'a' : '-');
            cout << ' ';
            cout << setw(7) << info.sequence << " ";
            cout << setw(7) << setprecision(1) << (info.bodySize / 1024.0) << 'K';

        } else {
            // Short form:
            int nSpaces = xpos ? (kListColumnWidth - (xpos % kListColumnWidth)) : 0;
            int newXpos = xpos + nSpaces + idWidth;
            if (newXpos < lineWidth) {
                if (xpos > 0)
                    cout << spaces(nSpaces);
                xpos = newXpos;
            } else {
                cout << "\n";
                xpos = idWidth;
            }
            cout << info.docID;
        }
    });

    if (nDocs == 0) {
        if (docIDPattern.empty())
            cout << "(No documents";
        else if (isGlobPattern(docIDPattern))
            cout << "(No documents with IDs matching \"" << docIDPattern << "\"";
        else
            cout << "(Document \"" << docIDPattern << "\" not found";
        if (!_collectionName.empty())
            cout << " in collection \"" << _collectionName << "\"";
        cout << ")";
    } else if (nDocs > _limit && _limit > 0) {
        cout << "\n(Stopping after " << _limit << " docs)";
    }
    cout << "\n";
}


void ListCommand::catDoc(C4Document *doc, bool includeID) {
    Dict body = c4doc_getProperties(doc);
    if (!body)
        fail("Unexpectedly couldn't parse document body!");
    slice docID, revID;
    if (includeID || _showRevID)
        docID = slice(doc->docID);
    if (_showRevID)
        revID = (slice)doc->selectedRev.revID;
    if (_prettyPrint)
        prettyPrint(body, cout, "", docID, revID, (_keys.empty() ? nullptr : &_keys));
    else
        rawPrint(body, docID, revID);
}


void ListCommand::listCollections() {
    vector<CollectionName> specs = allCollections();
    int nameWidth = 10;
    for (auto& spec : specs)
        nameWidth = std::max(nameWidth, int(nameOfCollection(spec).size()));

    cout << ansi("4") << left << setw(nameWidth) << "Collection";
    cout << "     Docs  Deleted  Expiring" << ansiReset() << "\n";

    for (auto const& spec : specs) {
        string fullName = nameOfCollection(spec);
        cout << ansiBold() << left << setw(nameWidth) << fullName << ansiReset() << "  ";
        cout.flush(); // the next results may take a few seconds to print

        auto coll = _db->getCollection(C4CollectionSpec(spec));
        auto docCount = coll->getDocumentCount();
        cout << right << setw(7) << docCount << "  ";

        auto nDeletedDocs = countDocsWhere(spec, "_deleted");
        cout << setw(7) << nDeletedDocs << "  ";

//        cout << setw(7) << coll->getLastSequence() << "  ";

        C4Timestamp nextExpiration = coll->nextDocExpiration();
        if (nextExpiration > 0) {
            cout << setw(8) << countDocsWhere(spec, "_expiration > 0");
            auto when = std::max((long long)nextExpiration - c4_now(), 0ll);
            cout << ansiItalic() << " (next in " << when << " sec)" << ansiReset();
        } else {
            cout << setw(8) << 0;
        }
        cout << endl;
    }
}


CBLiteCommand* newListCommand(CBLiteTool &parent) {
    return new ListCommand(parent);
}

CBLiteCommand* newListCollectionsCommand(CBLiteTool &parent) {
    return new ListCommand(parent, true);
}
