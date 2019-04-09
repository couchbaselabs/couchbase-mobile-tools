//
//  n1ql_2_json.cc
//  N1QL to JSON
//
//  Created by Jens Alfke on 4/4/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#include "n1ql_to_json.h"
#include "fleece/Fleece.hh"

#include <iostream>
#include "antlr4-runtime.h"
#include "tree/ParseTreeWalker.h"
#include "N1QLLexer.h"
#include "N1QLParser.h"
#include "N1QLBaseListener.h"
#include "N1QLTranslator.hh"

using namespace std;
using namespace antlr4;

namespace litecore { namespace n1ql {

    class ErrorListener : public BaseErrorListener {
    public:
        size_t line;
        size_t charPositionInLine;
        string message;

        virtual void syntaxError(Recognizer *recognizer, Token * offendingSymbol,
                                 size_t line_, size_t charPositionInLine_,
                                 const std::string &message_, std::exception_ptr e) override
        {
            line = line_;
            charPositionInLine = charPositionInLine_;
            message = message_;
        }
/*
        virtual void reportAmbiguity(Parser *recognizer, const dfa::DFA &dfa, size_t startIndex,
                                     size_t stopIndex, bool exact,
                                     const antlrcpp::BitSet &ambigAlts,
                                     atn::ATNConfigSet *configs) override
        { }

        virtual void reportAttemptingFullContext(Parser *recognizer, const dfa::DFA &dfa,
                                                 size_t startIndex, size_t stopIndex,
                                                 const antlrcpp::BitSet &conflictingAlts,
                                                 atn::ATNConfigSet *configs) override
        { }

        virtual void reportContextSensitivity(Parser *recognizer, const dfa::DFA &dfa,
                                              size_t startIndex, size_t stopIndex,
                                              size_t prediction, atn::ATNConfigSet *configs) override
        { }
*/
    };

} }


using namespace litecore::n1ql;


char* c4query_translateN1QL(C4String n1ql,
                            char** outErrorMessage,
                            unsigned* outErrorPosition,
                            unsigned* outErrorLine) C4API
{
    try {
        ErrorListener errors;
        ANTLRInputStream input((const char*)n1ql.buf, n1ql.size);
        N1QLLexer lexer(&input);
        lexer.removeErrorListeners();
        lexer.addErrorListener(&errors);
        CommonTokenStream tokens(&lexer);

        N1QLParser parser(&tokens);
        parser.removeErrorListeners();
        parser.addErrorListener(&errors);
        auto root = parser.sql();

        if (!errors.message.empty()) {
            if (outErrorMessage)
                *outErrorMessage = strdup(errors.message.c_str());
            if (outErrorPosition)
                *outErrorPosition = (unsigned) errors.charPositionInLine;
            if (outErrorLine)
                *outErrorLine = (unsigned) errors.line;
            return nullptr;
        }

//        cout << "N1QL parse tree:  " << root->toStringTree(&parser) << "\n";

        N1QLTranslator translator;
        Any result = translator.visit(root);
        Value tree;
        if (result.is<MutableDict>())
            tree = result.as<MutableDict>();
        else
            tree = result.as<MutableArray>();

        *outErrorMessage = nullptr;
        return strdup(tree.toJSONString().c_str());

    } catch (const std::exception &x) {
        if (outErrorPosition)
            *outErrorPosition = 0;
        if (outErrorLine)
            *outErrorLine = 0;
        if (outErrorMessage) {
            *outErrorMessage = (char*) malloc(30 + strlen(x.what()));
            sprintf(*outErrorMessage, "Unexpected exception: %s", x.what());
        }
        return nullptr;
    }
}



#ifdef __APPLE__
#ifdef _LIBCPP_DEBUG

#include <__debug>

namespace std {
    // Resolves a link error building with libc++ in debug mode. Apparently this symbol would be in
    // the debug version of libc++.dylib, but we don't have that on Apple platforms.
    __1::__libcpp_debug_function_type __1::__libcpp_debug_function;
}

#endif
#endif
