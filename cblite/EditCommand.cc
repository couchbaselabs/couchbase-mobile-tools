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
#include <algorithm>
#include <set>

#ifndef _MSC_VER
    #include <unistd.h>
    // Weirdly, this variable is defined by POSIX but doesn't appear in a system header(?)
    extern char** environ;
#endif

using namespace std;
using namespace fleece;
using namespace litecore;


class EditCommand : public CBLiteCommand {
public:
    EditCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("edit", true, "DOCID");
        cerr <<
        "  Update a document by editing it in your favorite $EDITOR.\n"
        "    --raw : Raw JSON (not pretty-printed)\n"
        "    --json5 : JSON5 syntax (no quotes around dict keys)\n"
        "    -- : End of arguments (use if DOCID starts with '-')\n"
        ;
    }


    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--raw",    [&]{_prettyPrint = false;}},
            {"--json5",  [&]{_json5 = true;}},
        });
        openWriteableDatabaseFromNextArg();
        string docID = nextArg("document ID");
        unquoteGlobPattern(docID); // remove any protective backslashes
        endOfArgs();

        // Read the doc:
        c4::ref<C4Document> doc = readDoc(docID, kDocGetCurrentRev);
        if (!doc)
            return;
        // Convert to JSON:
        string json;
        if (_prettyPrint) {
            stringstream out;
            out << "// You are editing document \"" << docID
                        << "\" of database \"" << slice(c4db_getName(_db)) << "\".\n"
                   "// Any changes you make will be saved back to the database when you exit.\n"
                   "// To abort, exit the editor without saving changes.\n\n";
            prettyPrint(Dict(c4doc_getProperties(doc)), out);
            json = out.str();
        } else {
            auto raw = alloc_slice(c4doc_bodyAsJSON(doc, true, nullptr));
            json = string(raw) + "\n";
        }

        while (true) {
            // Write the JSON to a temp file:
            FILE *fd = nullptr;
            FilePath tmpFile = FilePath(tempDirectory(), "").mkTempFile(&fd);
            string tmpFilePath = tmpFile.path();
            bool ok = (fputs(json.c_str(), fd) >= 0);
            fclose(fd);
            if (!ok)
                fail("Couldn't write to temporary file");

            // Run the editor and wait for it to finish:
            const char *editor = getenv("EDITOR");
            if (!editor)
                fail("$EDITOR environment variable is not set");
            invokeEditor(editor, tmpFilePath.c_str());

            // Reread the file:
            alloc_slice newJSON = fleece::readFile(tmpFilePath.c_str());
            tmpFile.del();
            if (newJSON == slice(json)) {
                cout << "(No changes made in editor)\n";
                return;
            }

            C4Error error;
            c4::Transaction t(_db);
            if (!t.begin(&error))
                fail("starting a transaction", error);

            // Parse the edited body as JSON5:
            alloc_slice newBody;
            FLStringResult msg;
            auto cvtd = alloc_slice(FLJSON5_ToJSON(newJSON, &msg, nullptr, nullptr));
            if (cvtd) {
                newBody = alloc_slice(c4db_encodeJSON(_db, cvtd, &error));
                if (!newBody)
                    msg = c4error_getMessage(error);
            }
            if (!newBody) {
                cout << "Sorry, that isn't valid JSON. Want to correct it [y/n]?";
                FLSliceResult_Release(msg);
                char input[100];
                if (!fgets(input, 100, stdin) || tolower(input[0]) == 'n')
                    return;
                json = string(newJSON);
                continue; // try again...
            }

            // Finally, update the document:
            c4::ref<C4Document> newDoc = c4doc_update(doc, newBody, doc->selectedRev.flags, &error);
            if (!newDoc || !t.commit(&error))
                fail("updating document", error);
            cout << "Updated \"" << docID << "\".\n";
            break;
        }
    }


#ifdef _MSC_VER
    void invokeEditor(const char *editor, const char *fileToEdit) {
        fail("Invoking editor is unimplemented on Windows");
    }
#else
    void invokeEditor(const char *editor, const char *fileToEdit) {
        // In Unix, running a new process involves forking and calling `execve` in the child:
        pid_t pid = fork();
        if (pid < 0) {
            // Fork failed:
            fail("Couldn't start editor " + string(editor));
        } else if (pid == 0) {
            // I am the child process -- exec the editor:
            char* const argv[3] = {(char*)editor, (char*)fileToEdit, nullptr};
            execve(editor, argv, environ);
            // If execve returned it failed, but as we've forked there's nothing we can do
            ::exit(1);
        }

        // In the original process, wait for the forked process to exit...
        int status;
        if (waitpid(pid, &status, 0) < 0)
            fail(format("Error waiting for the editor to finish (errno=%d)", errno));
        else if (!WIFEXITED(status))
            fail(format("Editor %s crashed (signal %d)",
                        editor, WTERMSIG(status)));
        else if (WEXITSTATUS(status) != 0)
            fail(format("Editor %s exited abnormally (status %d)",
                        editor, WEXITSTATUS(status)));
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
