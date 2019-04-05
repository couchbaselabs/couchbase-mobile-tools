//
//  n1ql2json.cc
//  N1QL to JSON
//
//  Created by Jens Alfke on 4/4/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#include "n1ql2json.hh"
#include "fleece/Fleece.hh"

#include <iostream>
#include "antlr4-runtime.h"
#include "tree/ParseTreeWalker.h"
#include "N1QLLexer.h"
#include "N1QLParser.h"
#include "N1QLBaseListener.h"
#include "N1QLTranslator.hh"

using namespace std;
using namespace fleece;
using namespace antlr4;
using namespace litecore_n1ql;

namespace litecore { namespace n1ql {

    class ErrorListener : public BaseErrorListener {
    public:
        string errorMessage;

        virtual void syntaxError(Recognizer *recognizer, Token * offendingSymbol,
                                 size_t line, size_t charPositionInLine,
                                 const std::string &msg, std::exception_ptr e) override
        {
            errorMessage = msg;
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


    string N1QL_to_JSON(const string &n1ql, string &errorMessage) {
        ErrorListener errors;
        ANTLRInputStream input(n1ql);
        N1QLLexer lexer(&input);
        lexer.addErrorListener(&errors);
        CommonTokenStream tokens(&lexer);

        N1QLParser parser(&tokens);
        parser.addErrorListener(&errors);
        auto root = parser.sql();

        if (!errors.errorMessage.empty()) {
            errorMessage = errors.errorMessage;
            return "";
        }

#if 1
        cout << "N1QL parse tree:  " << root->toStringTree(&parser) << "\n";
#endif

        N1QLTranslator translator;
        auto tree = translator.visit(root).as<MutableDict>();
        errorMessage = "";
        return tree.toJSONString();
    }

} }
