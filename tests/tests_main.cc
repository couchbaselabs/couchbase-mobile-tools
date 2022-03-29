//
// tests_main.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#define CATCH_CONFIG_MAIN
#include "TestsCommon.hh"
#include "catch.hpp"
#include "CaseListReporter.hh"

#ifndef NO_TEMP_DIR
#include "LiteCoreTest.hh"
#include "FilePath.hh"
#ifdef _MSC_VER
#include <atlbase.h>
#endif
#endif

#ifndef NO_WAIT_UNTIL
#include <chrono>
#include <thread>
#include <atomic>
#include "fleece/function_ref.hh"
#endif

// HACK: If reusing tests from the LiteCore suite (like
// cbl-logtest does) then we need to implement this 
// because the normal file that does is not compiled.
// Other tests that don't use it don't need it, and including
// LiteCoreTest.hh introduces a large dependency chain
#ifndef NO_TEMP_DIR
litecore::FilePath GetTempDirectory() {
#ifdef _MSC_VER
    WCHAR pathBuffer[MAX_PATH + 1];
    GetTempPathW(MAX_PATH, pathBuffer);
    GetLongPathNameW(pathBuffer, pathBuffer, MAX_PATH);
    CW2AEX<256> convertedPath(pathBuffer, CP_UTF8);
    return FilePath(convertedPath.m_psz, "");
#else // _MSC_VER
    return FilePath("/tmp", "");
#endif // _MSC_VER
}

litecore::FilePath TestFixture::sTempDir = GetTempDirectory();
#endif

#ifndef NO_WAIT_UNTIL
bool WaitUntil(std::chrono::milliseconds timeout, function_ref<bool()> predicate) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (predicate())
            return true;
        std::this_thread::sleep_for(50ms);
    } while (std::chrono::steady_clock::now() < deadline);

    return false;
}

extern "C" std::atomic_int gC4ExpectExceptions;

// While in scope, suppresses warnings about errors, and debugger exception breakpoints (in Xcode)
ExpectingExceptions::ExpectingExceptions()    {
    ++gC4ExpectExceptions;
    c4log_warnOnErrors(false);
}

ExpectingExceptions::~ExpectingExceptions()   {
    if (--gC4ExpectExceptions == 0)
        c4log_warnOnErrors(true);
}
#endif


