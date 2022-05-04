//
// LiteCoreTool.cc
//
// Copyright © 2022 Couchbase. All rights reserved.
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

#include "LiteCoreTool.hh"

#ifdef _MSC_VER
    #include <atlbase.h>
    #include <Shlwapi.h>
    #pragma comment(lib, "shlwapi.lib")
    #undef min
    #undef max
#else
    #include <fnmatch.h>        // POSIX (?)
#endif

using namespace std;
using namespace fleece;
using namespace litecore;


void LiteCoreTool::writeSize(uint64_t n) {
    static const char* kScaleNames[] = {" bytes", "KB", "MB", "GB"};
    int scale = 0;
    double scaled = n;
    while (scaled >= 1024 && scale < 3) {
        scaled /= 1024;
        ++scale;
    }
    auto prec = cout.precision();
    cout.precision(scale < 2 ? 0 : 1);
    cout << fixed << scaled << defaultfloat;
    cout.precision(prec);
    cout << kScaleNames[scale];
}


bool LiteCoreTool::canBeUnquotedJSON5Key(slice key) {
    if (key.size == 0 || isdigit(key[0]))
        return false;
    for (unsigned i = 0; i < key.size; i++) {
        if (!isalnum(key[i]) && key[i] != '_' && key[i] != '$')
            return false;
    }
    return true;
}


string LiteCoreTool::quoteJSONString(slice str) {
    if (std::any_of(str.begin(), str.end(),
                    [](char c) {return c < ' ' || c == '\"' || c == '\\' || c == 127;})) {
        JSONEncoder enc;
        enc << str;
        return string(enc.finish());
    } else {
        string json(str);
        json.insert(0, "\"");
        json.append("\"");
        return json;
    }
}


bool LiteCoreTool::isGlobPattern(string &str) {
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i) {
        char c = str[i];
        if ((c == '*' || c == '?') && (i == 0 || str[i-1] != '\\'))
            return true;
    }
    return false;
}

void LiteCoreTool::unquoteGlobPattern(string &str) {
    size_t size = str.size();
    for (size_t i = 0; i < size; ++i) {
        if (str[i] == '\\') {
            str.erase(i, 1);
            --size;
        }
    }
}


bool LiteCoreTool::globMatch(const char *name, const char *pattern) {
#ifdef _MSC_VER
    return PathMatchSpecA(name, pattern);
#else
    return fnmatch(pattern, name, 0) == 0;
#endif
}


string LiteCoreTool::tempDirectory() {
#ifdef _MSC_VER
    WCHAR pathBuffer[MAX_PATH + 1];
    GetTempPathW(MAX_PATH, pathBuffer);
    GetLongPathNameW(pathBuffer, pathBuffer, MAX_PATH);
    CW2AEX<256> convertedPath(pathBuffer, CP_UTF8);
    return convertedPath.m_psz;
#else // _MSC_VER
    const char *tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp/";
#endif // _MSC_VER
}


void LiteCoreTool::rawPrint(Value body, slice docID, slice revID) {
    alloc_slice jsonBuf = body.toJSON(_json5, true);
    slice restOfJSON = jsonBuf;
    if (docID) {
        // Splice a synthesized "_id" property into the start of the JSON object:
        cout << "{" << ansiDim() << ansiItalic()
        << (_json5 ? "_id" : "\"_id\"") << ":\""
        << ansiReset() << ansiDim()
        << docID << "\"";
        if (revID) {
            cout << "," << ansiItalic()
            << (_json5 ? "_rev" : "\"_rev\"") << ":\""
            << ansiReset() << ansiDim()
            << revID << "\"";
        }
        restOfJSON.moveStart(1);
        if (restOfJSON.size > 1)
            cout << ", ";
        cout << ansiReset();
    }
    cout << restOfJSON;
}


void LiteCoreTool::prettyPrint(Value value,
                                ostream &out,
                                const string &indent,
                                slice docID,
                                slice revID,
                                const set<alloc_slice> *onlyKeys)
{
    string reset, dim, italic;
    if (&out == &std::cout) {
        reset = ansiReset();
        dim = ansiDim();
        italic = ansiItalic();
    }

    switch (value.type()) {
        case kFLDict: {
            auto dict = value.asDict();
            string subIndent = indent + "  ";
            int n = 0;
            out << "{";
            if (docID) {
                n++;
                out << '\n' << subIndent << dim << italic;
                out << (_json5 ? "_id" : "\"_id\"");
                out << reset << dim << ": \"" << docID << "\"";
                if (revID) {
                    n++;
                    out << ",\n" << subIndent << italic;
                    out << (_json5 ? "_rev" : "\"_rev\"");
                    out << reset << dim << ": \"" << revID << "\"";
                }
            }
            vector<slice> keys;
            for (Dict::iterator i(dict); i; ++i) {
                slice key = i.keyString();
                if (!onlyKeys || onlyKeys->find(alloc_slice(key)) != onlyKeys->end())
                    keys.push_back(key);
            }
            sort(keys.begin(), keys.end());
            for (slice key : keys) {
                if (n++ > 0)
                    out << ',' << reset;
                out << '\n' << subIndent << italic;
                if (_json5 && canBeUnquotedJSON5Key(key))
                    out << key;
                else
                    out << '"' << key << '"';      //FIX: Escape quotes
                out << reset << ": ";

                prettyPrint(dict.get(key), out, subIndent);
            }
            out << '\n' << indent << "}";
            break;
        }
        case kFLArray: {
            string subIndent = indent + "  ";
            out << "[\n";
            for (Array::iterator i(value.asArray()); i; ++i) {
                out << subIndent;
                prettyPrint(i.value(), out, subIndent);
                if (i.count() > 1)
                    out << ',';
                out << '\n';
            }
            out << indent << "]";
            break;
        }
        case kFLData: {
            // toJSON would convert to base64, which isn't readable, so use escapes instead:
            static const char kHexDigits[17] = "0123456789abcdef";
            slice data = value.asData();
            auto end = (const uint8_t*)data.end();
            out << "«";
            bool dimmed = false;
            for (auto c = (const uint8_t*)data.buf; c != end; ++c) {
                if (*c >= 32 && *c < 127) {
                    if (dimmed)
                        out << reset;
                    dimmed = false;
                    out << (char)*c;
                } else {
                    if (!dimmed)
                        out << dim;
                    dimmed = true;
                    out << '\\' << kHexDigits[*c/16] << kHexDigits[*c%16];
                }
            }
            if (dimmed)
                out << reset;
            out << "»";
            break;
        }
        default: {
            alloc_slice json(value.toJSON());
            out << json;
            break;
        }
    }
}
