//
// cbliteTool+encrypt.cc
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

#include "cbliteTool.hh"

#ifdef COUCHBASE_ENTERPRISE

void CBLiteTool::encryptUsage() {
    writeUsageCommand("encrypt", false);
    cerr <<
    "    --raw : Take an AES256 key (64 hex digits) instead of a password\n"
    "  Encrypts the database, or changes the current encryption.\n"
    "  Prompts securely for a new password/key.\n"
    ;
}


void CBLiteTool::decryptUsage() {
    writeUsageCommand("decrypt", false);
    cerr <<
    "  Removes database encryption.\n"
    ;
}


static bool decodeHex(const string &hex, uint8_t* bytes, size_t byteCount) {
    if (hex.size() != 2*byteCount)
        return false;
    for (size_t i=0; i<hex.size(); i+=2) {
        if (!isxdigit(hex[i]) || !isxdigit(hex[i+1]))
            return false; // digest is not hex
        *bytes++ = (uint8_t)(16*digittoint(hex[i]) + digittoint(hex[i+1]));
    }
    return true;
}


void CBLiteTool::rekey(bool encrypting) {
    // Read params:
    bool usePassword = _prettyPrint;        // The --raw flag clears _prettyPrint
    processFlags(nullptr);
    if (_showHelp) {
        encrypting ? encryptUsage() : decryptUsage();
        return;
    }
    openWriteableDatabaseFromNextArg();
    endOfArgs();

    C4EncryptionKey newKey;
    if (encrypting) {
        if (!_interactive)
            fail("encrypt can only be used in interactive mode");
        if (usePassword) {
            string newPassword = readPassword("New password: ");
            if (!c4key_setPassword(&newKey, slice(newPassword), kC4EncryptionAES256))
                fail("Could not convert password to AES256 key");
        } else {
            string keyStr = readPassword("New encryption key (64 hex digits): ");
            if (!decodeHex(keyStr, newKey.bytes, kC4EncryptionKeySizeAES256))
                fail("Invalid hex key");
            newKey.algorithm = kC4EncryptionAES256;
        }
    } else {
        newKey.algorithm = kC4EncryptionNone;
    }

    C4Error error;
    if (!c4db_rekey(_db, &newKey, &error))
        fail("Rekeying failed", error);

    if (encrypting)
        cout << "The password has been changed. Don't forget it!\n";
    else
        cout << "The database is now decrypted.\n";
}

#endif
