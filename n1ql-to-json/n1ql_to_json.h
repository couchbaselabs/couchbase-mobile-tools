//
//  n1ql_to_json.h
//  Tools
//
//  Created by Jens Alfke on 4/8/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "c4Base.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Translates a N1QL query to JSON syntax.
    @param n1ql  The N1QL query string.
    @param outErrorMessage  On error, a pointer to the error message will be stored here.
                (This is a heap-allocated C string. Caller must call `free` afterwards.)
    @param outErrorPosition  On error, the offset of the error in the query will be stored here.
    @return  On success, the JSON query (as a heap-allocated C string); on error, NULL. */
char* c4query_translateN1QL(C4String n1ql,
                            char** outErrorMessage,
                            unsigned* outErrorPosition,
                            unsigned* outErrorLine) C4API;

#ifdef __cplusplus
}
#endif
