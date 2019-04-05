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

using namespace std;
using namespace fleece;
using namespace antlr4;
using namespace litecore_n1ql;

namespace litecore { namespace n1ql {

    typedef tree::TerminalNode* (N1QLParser::ExprContext::*BinaryOp)();
    static const BinaryOp kBinaryOps[] = {
        &N1QLParser::ExprContext::EQ,
        &N1QLParser::ExprContext::ASSIGN,
        &N1QLParser::ExprContext::LT,
        &N1QLParser::ExprContext::LT_EQ,
        &N1QLParser::ExprContext::GT,
        &N1QLParser::ExprContext::GT_EQ,
        &N1QLParser::ExprContext::NOT_EQ1,
        &N1QLParser::ExprContext::NOT_EQ2,
        &N1QLParser::ExprContext::STAR,
        &N1QLParser::ExprContext::PLUS,
        &N1QLParser::ExprContext::MINUS,
        &N1QLParser::ExprContext::DIV,
        &N1QLParser::ExprContext::MOD,
        &N1QLParser::ExprContext::AMP,
        &N1QLParser::ExprContext::PIPE,
        &N1QLParser::ExprContext::PIPE2,
        &N1QLParser::ExprContext::K_IS,
        &N1QLParser::ExprContext::K_IN,
        &N1QLParser::ExprContext::K_LIKE,
        &N1QLParser::ExprContext::K_AND,
        &N1QLParser::ExprContext::K_OR,
        nullptr
    };

    typedef tree::TerminalNode* (N1QLParser::Unary_operatorContext::*UnaryOp)();
    static const UnaryOp kUnaryOps[] = {
        &N1QLParser::Unary_operatorContext::MINUS,
        &N1QLParser::Unary_operatorContext::PLUS,
        &N1QLParser::Unary_operatorContext::TILDE,
        &N1QLParser::Unary_operatorContext::K_NOT,
        nullptr
    };

    static tree::TerminalNode* findBinaryOp(N1QLParser::ExprContext *expr) {
        for (int i = 0; kBinaryOps[i]; ++i) {
            auto op = (expr->*kBinaryOps[i])();
            if (op)
                return op;
        }
        return nullptr;
    }

    static tree::TerminalNode* findUnaryOp(N1QLParser::Unary_operatorContext *expr) {
        for (int i = 0; kUnaryOps[i]; ++i) {
            auto op = (expr->*kUnaryOps[i])();
            if (op)
                return op;
        }
        return nullptr;
    }

    static string getName(tree::TerminalNode *x) {
        return x->getSymbol()->getText();
    }

    static string getName(N1QLParser::Any_nameContext *x) {
        return getName(x->IDENTIFIER());
    }

    // SELECT col1, col2 FROM db WHERE type = 'cat'
    // (sql_stmt (factored_select_stmt (select_core SELECT (result_column (expr (column_name (any_name col1)))) , (result_column (expr (column_name (any_name col2)))) FROM (table_or_subquery (table_name (any_name db))) WHERE (expr (expr (column_name (any_name type))) = (expr (literal_value 'cat'))))))

    class Listener : public N1QLBaseListener {
    public:
        std::vector<N1QLParser::Ordering_termContext *> _ordering_term;
        N1QLParser::ExprContext* _limit = nullptr;
        N1QLParser::ExprContext* _offset = nullptr;

        template <class CTX>
        void getOrderLimitOffset(CTX *ctx) {
            _ordering_term = ctx->ordering_term();
            if (ctx->K_LIMIT()) {
                _limit = ctx->expr(0);
                if (ctx->K_OFFSET()) {
                    _offset = ctx->expr(1);
                }
            }
        }

        void enterCompound_select_stmt(N1QLParser::Compound_select_stmtContext *ctx) override {
            getOrderLimitOffset(ctx);
        }

        void enterFactored_select_stmt(N1QLParser::Factored_select_stmtContext *ctx)  override {
            getOrderLimitOffset(ctx);
        }

        void enterSimple_select_stmt(N1QLParser::Simple_select_stmtContext *ctx)  override {
            getOrderLimitOffset(ctx);
        }

        void enterSelect_stmt(N1QLParser::Select_stmtContext *ctx)  override {
            getOrderLimitOffset(ctx);
        }

        void enterSelect_core(N1QLParser::Select_coreContext * ctx) override {
            _enc.beginDict();

            if (ctx->K_DISTINCT())
                _enc["DISTINCT"_sl] = true;

            _enc.writeKey("WHAT"_sl);
            _enc.beginArray();
            for (N1QLParser::Result_columnContext *col : ctx->result_column())
                visitResultColumn(col);
            _enc.endArray();

            if (!ctx->table_or_subquery().empty()) {
                _enc.writeKey("FROM"_sl);
                _enc.beginArray();
                auto alias = ctx->table_or_subquery(0)->table_alias();
                if (alias) {
                    _enc.beginDict();
                    _enc.writeKey("AS"_sl);
                    _enc.writeString(getName(alias->any_name()));
                    _enc.endDict();
                }
#if 0 // under construction
                auto join = ctx->join_clause();
                if (join) {
                    for (size_t i = 0; true; ++i) {
                        auto op = join->join_operator(i);
                        auto constraint = join->join_constraint(i);
                        _enc.beginDict();
                        _enc.writeKey("JOIN"_sl);
                        _enc.writeString("INNER");  //FIXME: Get the join type
                        _enc.writeKey("ON"_sl);
                        visitExpr(constraint);
                        _enc.writeString(getName(alias->any_name()));
                        _enc.endDict();
                    }
                }
#endif
                _enc.endArray();
            }

            if (ctx->K_WHERE()) {
                _enc.writeKey("WHERE"_sl);
                visitExpr(ctx->expr()[0]);
            }

            if (!_ordering_term.empty()) {
                _enc.writeKey("ORDER_BY"_sl);
                _enc.beginArray();
                for (auto term : _ordering_term) {
                    if (term->K_DESC()) {
                        _enc.beginArray();
                        _enc.writeString("DESC"_sl);
                    }
                    visitExpr(term->expr());
                    if (term->K_DESC()) {
                        _enc.endArray();
                    }
                    //TODO: Handle COLLATE
                }
                _enc.endArray();
            }

            if (_limit) {
                _enc.writeKey("LIMIT"_sl);
                visitExpr(_limit);
            }
            if (_offset) {
                _enc.writeKey("OFFSET"_sl);
                visitExpr(_offset);
            }
        }

        void exitSelect_core(N1QLParser::Select_coreContext * /*ctx*/) override {
            _enc.endDict();
        }

        void visitResultColumn(N1QLParser::Result_columnContext *col) {
            auto expr = col->expr();
            if (expr) {
                auto alias = col->column_alias();
                if (alias) {
                    _enc.beginArray();
                    _enc.writeString("AS"_sl);
                }
                visitExpr(expr);
                if (alias) {
                    _enc.writeString(getName(alias->IDENTIFIER()));
                }
            }
        }

        void visitExpr(N1QLParser::ExprContext *expr) {
            tree::TerminalNode* op;
            if (expr->literal_value()) {
                auto literal = expr->literal_value();
                if (literal->NUMERIC_LITERAL()) {
                    string nstr = getName(literal->NUMERIC_LITERAL());
                    size_t pos;
                    long long ll = stoll(nstr, &pos);
                    if (pos == nstr.size()) {
                        _enc.writeInt((int64_t)ll);
                    } else {
                        double d = stod(nstr, &pos);
                        assert(pos == nstr.size());
                        _enc.writeDouble(d);
                    }
                } else if (literal->STRING_LITERAL()) {
                    string str = getName(literal->STRING_LITERAL());
                    str = str.substr(1, str.size()-2);
                    // Unquote the string:
                    string::size_type pos = 0;
                    while (string::npos != (pos = str.find("''", pos)))
                        str.replace(pos, 2, "");
                    _enc.writeString(slice(str));
                } else {
                    throw runtime_error("Unhandled literal type");
                }
            } else if (expr->column_name()) {
                visitColumnName(expr->column_name());
            } else if (expr->unary_operator()) {
                auto unary = expr->unary_operator();
                auto unaryOp = findUnaryOp(unary);
                _enc.beginArray();
                _enc.writeString(getName(unaryOp));
                visitExpr(expr->expr(0));
                _enc.endArray();
            } else if (nullptr != (op = findBinaryOp(expr))) {
                if (expr->K_NOT()) {
                    _enc.beginArray();
                    _enc.writeString("NOT"_sl);
                }
                _enc.beginArray();
                _enc.writeString(getName(op));
                visitExpr(expr->expr(0));
                visitExpr(expr->expr(1));
                _enc.endArray();
                if (expr->K_NOT()) {
                    _enc.endArray();
                }
            } else if (expr->function_name()) {
                _enc.beginArray();
                _enc.writeString(getName(expr->function_name()->any_name()) + "()");
                for (auto param : expr->expr())
                    visitExpr(param);
                _enc.endArray();
            } else if (expr->K_ISNULL()) {
                _enc.beginArray();
                _enc.writeString("IS"_sl);
                visitExpr(expr->expr(0));
                _enc.writeNull();
                _enc.endArray();
            } else if (expr->K_NOTNULL()) {
                _enc.beginArray();
                _enc.writeString("IS NOT"_sl);
                visitExpr(expr->expr(0));
                _enc.writeNull();
                _enc.endArray();
            } else if (expr->OPEN_PAR()) {
                visitExpr(expr->expr(0));
            } else {
                throw runtime_error("Unhandled expr");
            }
        }

        void visitColumnName(N1QLParser::Column_nameContext *col) {
            string name = getName(col->any_name());
            _enc.beginArray();
            _enc.writeString("." + name);
            _enc.endArray();
        }

        string JSON() {
            return _enc.finish().asString();
        }

    private:
        JSONEncoder _enc;
    };


    string N1QL_to_JSON(const string &n1ql) {
        ANTLRInputStream input(n1ql);
        N1QLLexer lexer(&input);
        CommonTokenStream tokens(&lexer);

//        tokens.fill();
//        for (auto token : tokens.getTokens()) {
//            std::cout << token->toString() << std::endl;
//        }

        N1QLParser parser(&tokens);
        auto stmt = parser.sql_stmt();

#if 1
        tree::ParseTree* tree = stmt;
        cout << "N1QL parse tree:  " << tree->toStringTree(&parser) << "\n";
#endif

        Listener listener;
        tree::ParseTreeWalker::DEFAULT.walk(&listener, stmt);
        return listener.JSON();
        return "";
    }

} }
