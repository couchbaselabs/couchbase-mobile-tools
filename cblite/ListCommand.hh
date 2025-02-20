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
    ListCommand(CBLiteTool &parent, bool listCollections = false)
    :CBLiteCommand(parent)
    ,_listCollections(listCollections)
    { }

    void usage() override;
    void runSubcommand() override;


    void addLineCompletions(ArgumentTokenizer &tokenizer,
                            std::function<void(const std::string&)> add) override
    {
        addDocIDCompletions(tokenizer, add);
    }

protected:
    void listDocs(std::string docIDPattern);
    void listCollections();
    void catDoc(C4Document *doc, bool includeID);
    
    bool    _showRevID {false};
    bool    _longListing {false};
    bool    _listBySeq {false};
    bool    _listCollections {false};
};
