//
//  n1ql_to_json.h
//
// Copyright (c) 2019 Couchbase, Inc. All rights reserved.
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
#include "c4Base.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef C4_OPTIONS(uint32_t, C4TranslateN1QLFlags) {
    kN1QLToCanonicalJSON = 1,
    kN1QLToJSON5 = 2,
};

    
/** Translates a N1QL query to JSON syntax.
    @param n1ql  The N1QL query string.
    @param outErrorMessage  On error, a pointer to the error message will be stored here.
                (This is a heap-allocated C string. Caller must call `free` afterwards.)
    @param outErrorPosition  On error, the offset of the error in the query will be stored here.
    @return  On success, the JSON query (as a heap-allocated C string); on error, NULL. */
char* c4query_translateN1QL(C4String n1ql,
                            C4TranslateN1QLFlags flags,
                            char** outErrorMessage,
                            unsigned* outErrorPosition,
                            unsigned* outErrorLine) C4API;

#ifdef __cplusplus
}
#endif
