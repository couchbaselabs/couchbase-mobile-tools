//
//  ListCommand.hh
//  Tools
//
//  Created by Jens Alfke on 2/27/20.
//  Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "CBLiteCommand.hh"


class ListCommand : public CBLiteCommand {
public:
    ListCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }

    void usage() override;
    void runSubcommand() override;

protected:
    void listDocs(std::string docIDPattern);
    void catDoc(C4Document *doc, bool includeID);
    
    bool                    _showRevID {false};
    fleece::alloc_slice     _startKey, _endKey;
    bool                    _longListing {false};
    bool                    _listBySeq {false};
};
