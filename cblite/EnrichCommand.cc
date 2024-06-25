//
// EnrichCommand.cc
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

#include <string>
#include <iostream>
#include <fstream>
#include "EnrichCommand.hh"
#include "fleece/Fleece.hh"
#include "StringUtil.hh"

using namespace std;
using namespace fleece;

void EnrichCommand::usage() {
    writeUsageCommand("enrich", false, "PROP");
    cerr <<
    "Outputs property found in 10 documents in the database.\n"
    "    --offset N : Skip first N docs\n"
    "    --limit N : Stop after N docs\n"
    "    " << it("PROPERTY") << " : property for matching docs\n"
    "    " << it("DESTINATION") << " : output filename\n"
    ;
}

void EnrichCommand::runSubcommand() {
    // Read params:
    processFlags({
        {"--offset", [&]{offsetFlag();}},
        {"--limit",  [&]{limitFlag();}},
    });
    openDatabaseFromNextArg();
    string prop;
    string outputFileName;
    prop = nextArg("property");
    if (hasArgs())
        outputFileName = nextArg("output file");
    endOfArgs();
    
    enrichDocs(prop, outputFileName);
}

void EnrichCommand::enrichDocs(string propArg, string outputFileName) {
    EnumerateDocsOptions options{};
    options.flags       |= kC4IncludeBodies;
    options.bySequence  = true;
    options.offset      = _offset;
    options.limit       = _limit;
    
    //Open AI documentation make call to their server for encoding
    //newDoc = c4coll_createDoc(collection(), slice(docID), newBody, {}, &error);
    ofstream outputFile;
    
    if (!outputFileName.empty()) {
        outputFileName = outputFileName + ".txt";
        outputFile.open(outputFileName + ".txt");
    } else {
        outputFileName = propArg + "_vector.txt";
        outputFile.open(propArg + "_vector.txt");
    }
    cout << "\n";
    
    if (_offset > 0)
        cout << "(Skipping first " << _offset << " docs)\n";
    
    //print writing [name of property] to [document] on [docID]
    // docID --> printf("something something something .%*s\n", SPLAT(slice_value));
    cout << "(Writing \"" << propArg << "\"" << " to \"" << outputFileName << "\"" << " on ____\")\n";
    // docID << "\"" << ")";
    //printf(outputFileName + ".%*s\n");
    int64_t nDocs = enumerateDocs(options, [&](const C4DocumentInfo &info, C4Document *doc) {
        Dict body = c4doc_getProperties(doc);
        if (!body)
            fail("Unexpectedly couldn't parse document body!");
        Value property = body.get(propArg);
        if (property.type() != kFLString)
        {
            cout << "Property type must be a string" << endl;
            return;
        }
        string docName = property.asString().asString();
        
        cout << docName << "\n";
        outputFile << docName << "\n";
    });
    
    outputFile.close();

    if (nDocs == 0) {
            cout << "(No documents with property matching \"" << propArg << "\"" << ")";
    } else if (nDocs > _limit && _limit > 0) {
        cout << "\n(Stopping after " << _limit << " docs)";
    }
    cout << "\n";
}

CBLiteCommand* newEnrichCommand(CBLiteTool &parent) {
    return new EnrichCommand(parent);
}
