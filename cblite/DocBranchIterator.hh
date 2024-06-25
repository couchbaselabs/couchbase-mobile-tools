//
//  DocBranchIterator.hh
//  Tools
//
//  Created by Jens Alfke on 2/25/20.
//  Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "c4Document.h"
#include <stdexcept>


class DocBranchIterator {
public:
    DocBranchIterator(C4Document *doc)
    :_doc(doc)
    {
        c4doc_selectCurrentRevision(doc);
        _branchID = _doc->selectedRev.revID;
    }

    operator bool() const {
        return _branchID != fleece::nullslice;
    }

    DocBranchIterator& operator++() {
        C4Error err;
        if (!c4doc_selectRevision(_doc, _branchID, false, &err)) {
            // should never occur...
            throw std::runtime_error("unexpected error iterating document branches");
        }
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
