//
// EditCommand.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "CBLiteCommand.hh"
#include "fleece/Fleece.hh"
#include "FilePath.hh"
#include "sliceIO.hh"
#include "Defer.hh"
#include <algorithm>
#include <set>

#ifndef _MSC_VER
    #include <sys/wait.h>
    #include <unistd.h>
    // Weirdly, this variable is defined by POSIX but doesn't appear in a system header:
    extern char** environ;
#endif

using namespace std;
using namespace fleece;
using namespace litecore;


class EditCommand : public CBLiteCommand {
private:
    string _editor;

public:
    EditCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("edit", true, "DOCID");
        cerr <<
        "  Update or create a document as JSON in a text editor.\n"
        "    --with EDITOR : Path to text editor program (defaults to `$EDITOR`)\n"
        "    --raw         : Raw JSON (not pretty-printed)\n"
        "    --json5       : JSON5 syntax (no quotes around dict keys)\n"
        "    --            : End of arguments (use if DOCID starts with '-')\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--with",  [&]{_editor = nextArg("Path to editor");}},
            {"--raw",   [&]{_prettyPrint = false;}},
            {"--json5", [&]{_json5 = true;}},
        });
        openWriteableDatabaseFromNextArg();
        string docID = nextArg("document ID");
        unquoteGlobPattern(docID); // remove any protective backslashes
        endOfArgs();

        // Use the default editor ($EDITOR) if one was not specified:
        if (_editor.empty()) {
            if (const char *defaultEditor = getenv("EDITOR"); defaultEditor && *defaultEditor)
                _editor = defaultEditor;
            else
                fail("Please use --with to specify the editor, or set $EDITOR in your shell.");
        }
        if (!FilePath(_editor).exists())
            fail("Editor " + _editor + " doesn't exist");

        // Read the doc, if it exists, and convert to JSON:
        c4::ref<C4Document> doc = readDoc(docID, kDocGetCurrentRev);
        string json;
        if (!doc) {
            stringstream out;
            out << "// You are creating document \"" << docID
                        << "\" of database \"" << slice(c4db_getName(_db)) << "\".\n"
                   "// Add JSON properties below, then save changes and exit.\n"
                   "// To cancel, exit the editor without saving changes.\n\n"
                   "{\n}\n";
            json = out.str();
        } else if (_prettyPrint) {
            stringstream out;
            out << "// You are editing document \"" << docID
                        << "\" of database \"" << slice(c4db_getName(_db)) << "\".\n"
                   "// Any changes you make will be saved back to the database when you exit.\n"
                   "// To cancel, exit the editor without saving changes.\n\n";
            prettyPrint(Dict(c4doc_getProperties(doc)), out);
            json = out.str();
        } else {
            auto raw = alloc_slice(c4doc_bodyAsJSON(doc, true, nullptr));
            json = string(raw) + "\n";
        }

        while (true) {
            // Let the user edit the JSON:
            string newJSON = editInTemporaryFile(json);
            if (newJSON == json) {
                cout << "No changes were made.\n";
                return;
            }

            C4Error error;
            c4::Transaction t(_db);
            if (!t.begin(&error))
                fail("starting a transaction", error);

            // Convert the edited body from JSON5 to Fleece:
            alloc_slice newBody;
            FLStringResult msg;
            auto cvtd = alloc_slice(FLJSON5_ToJSON(slice(newJSON), &msg, nullptr, nullptr));
            if (cvtd) {
                newBody = alloc_slice(c4db_encodeJSON(_db, cvtd, &error));
                if (!newBody)
                    msg = c4error_getMessage(error);
            }
            if (!newBody) {
                cout << "Sorry, that isn't valid JSON. Want to correct it [y/n]?";
                FLSliceResult_Release(msg);
                char input[100];
                if (!fgets(input, sizeof(input), stdin) || tolower(input[0]) == 'n')
                    return;
                json = newJSON;
                continue; // try again...
            }

            // Finally, update the document:
            c4::ref<C4Document> newDoc;
            if (doc) {
                newDoc = c4doc_update(doc, newBody, doc->selectedRev.flags, &error);
            } else {
#ifdef HAS_COLLECTIONS
                newDoc = c4coll_createDoc(collection(), slice(docID), newBody, {}, &error);
#else
                newDoc = c4doc_create(_db, slice(docID), newBody, {}, &error);
#endif
            }
            if (!newDoc || !t.commit(&error))
                fail("saving document", error);
            cout << (doc ? "Updated \"" : "Created \"") << docID << "\".\n";
            break;
        }
    }


    string editInTemporaryFile(const string &contents) {
        FILE *fd = nullptr;
        FilePath tmpFile = FilePath(tempDirectory(), "").mkTempFile(&fd);
        DEFER {tmpFile.del();};

        bool ok = (fputs(contents.c_str(), fd) >= 0);
        fclose(fd);
        if (!ok)
            fail("Couldn't write to temporary file");

        string tmpFilePath = tmpFile.path();
        invokeEditor(_editor, tmpFilePath);
        return string(fleece::readFile(tmpFilePath.c_str()));
    }


#ifdef _MSC_VER
    void invokeEditor(const string &editor, const string &fileToEdit) {
        fail("Invoking editor is unimplemented on Windows");
    }
#else
    void invokeEditor(const string &editor, const string &fileToEdit) {
        // In Unix, running a new process involves forking and calling `execve` in the child:
        const char *editorc = editor.c_str();
        pid_t pid = fork();
        if (pid < 0) {
            // Fork failed:
            fail("Couldn't start editor " + editor);
        } else if (pid == 0) {
            // I am the child process -- exec the editor:
            char* const argv[3] = {(char*)editorc, (char*)fileToEdit.c_str(), nullptr};
            execve(editorc, argv, environ);
            // If execve returned it failed, but as we've forked there's nothing we can do
            ::exit(1);
        }

        // In the original process, wait for the forked process to exit...
        int status;
        if (waitpid(pid, &status, 0) < 0)
            fail(format("Error waiting for the editor to finish (errno=%d)", errno));
        else if (!WIFEXITED(status))
            fail(format("Editor %s crashed (signal %d)",
                        editorc, WTERMSIG(status)));
        else if (WEXITSTATUS(status) != 0)
            fail(format("Editor %s exited abnormally (status %d)",
                        editorc, WEXITSTATUS(status)));
    }
#endif


    void addLineCompletions(ArgumentTokenizer &tokenizer,
                            function<void(const string&)> add) override
    {
        addDocIDCompletions(tokenizer, add);
    }

}; // end editCommand


CBLiteCommand* newEditCommand(CBLiteTool &parent) {
    return new EditCommand(parent);
}
