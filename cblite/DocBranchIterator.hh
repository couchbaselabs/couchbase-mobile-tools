//
//  DocBranchIterator.hh
//  Tools
//
//  Created by Jens Alfke on 2/25/20.
//  Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "c4Document.h"


class DocBranchIterator {
public:
    DocBranchIterator(C4Document *doc C4NONNULL)
    :_doc(doc)
    {
        c4doc_selectCurrentRevision(doc);
        _branchID = _doc->selectedRev.revID;
    }

    operator bool() const {
        return _branchID != fleece::nullslice;
    }

    DocBranchIterator& operator++() {
        c4doc_selectRevision(_doc, _branchID, false, nullptr);
        _branchID = fleece::nullslice;
        while (c4doc_selectNextRevision(_doc)) {
            if (_doc->selectedRev.flags & kRevLeaf) {
                _branchID = _doc->selectedRev.revID;
                break;
            }
        }
        return *this;
    }

private:
    C4Document* _doc;
    fleece::alloc_slice _branchID;
};
