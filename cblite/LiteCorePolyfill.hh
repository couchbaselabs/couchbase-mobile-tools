//
// LiteCorePolyfill.hh
//
// Copyright © 2021 Couchbase. All rights reserved.
//

// This header is a compatibility shim for pre-3.0 versions of LiteCore.
// It adds definitions of some newer types and functions that the cblite tool uses.

#pragma once
#include "c4.h"
#include "fleece/Fleece.h"
#include "fleece/slice.hh"


// Unofficial LiteCore C++ API helpers; in dev their header has been renamed
#if __has_include("tests/c4CppUtils.hh")
#   include "tests/c4CppUtils.hh"       // dev branch
#else
#   include "c4.hh"                     // master branch (as of May 2021); TODO: remove after merge
#   include "c4Transaction.hh"
#endif


#ifndef LITECORE_VERSION
    // LITECORE_VERSION was added to the dev and master branches May 19 2021.
    // If building with older source code, use some heuristics to figure out what it is:
    #ifdef C4_ASSUME_NONNULL_BEGIN              // (this macro was added post-2.8.)
        #define LITECORE_VERSION 30000
        #if __has_include("c4Collection.h")
            #define LITECORE_API_VERSION 350        // dev branch (May 2021)
        #else
            #define LITECORE_API_VERSION 300        // master branch (May 2021)
        #endif
    #else
        #define LITECORE_VERSION 20800          // else assume CBL 2.8
        #define LITECORE_API_VERSION 200
    #endif
#endif


// Test if collections are available
#if __has_include("c4Collection.h")
#   define HAS_COLLECTIONS
#endif


#if LITECORE_API_VERSION < 300

// Define stuff we use that's not in LiteCore 2.8:

enum C4DocContentLevel {
    kDocGetCurrentRev,
    kDocGetAll,
};

static inline C4StringResult c4db_getName(C4Database *db) {
    fleece::alloc_slice apath = c4db_getPath(db);
    fleece::slice path = apath;
    if (path.hasSuffix("/") || path.hasSuffix("\\"))
        path.setSize(path.size - 1);
    if (path.hasSuffix(".cblite2"))
        path.setSize(path.size - 8);
    for (auto c = (const char *)path.end(); c >= path.buf; --c)
        if (c[-1] == '/' || c[-1] == '\\') {
            path.setStart(c);
            break;
        }
    return C4StringResult(fleece::alloc_slice(path));
}

static inline C4Document* c4db_getDoc(C4Database *db, C4String docID, bool mustExist,
                                      C4DocContentLevel, C4Error *error)
{
    return c4doc_get(db, docID, mustExist, error);
}

static inline C4Slice c4doc_getRevisionBody(C4Document *doc) {
    return doc->selectedRev.body;
}

static inline FLDict c4doc_getProperties(C4Document *doc) {
    if (doc->selectedRev.body.buf)
        return FLValue_AsDict( FLValue_FromData(doc->selectedRev.body, kFLTrusted) );
    else
        return nullptr;
}

#endif
