//
//  N1QLVisitor.cc
//  N1QL to JSON
//
//  Copyright © 2019 Couchbase. All rights reserved.
//

#include "N1QLBaseVisitor.h"
#include "N1QLParser.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include "antlr4-runtime.h"
#include <stdexcept>


namespace litecore { namespace n1ql {
    using namespace std;
    using namespace antlrcpp;
    using namespace antlr4::tree;
    using namespace litecore_n1ql;
    using namespace fleece;

    // Translates a N1QL parse tree into LiteCore's JSON query syntax (as Fleece objects)
    class N1QLTranslator : public N1QLBaseVisitor {

        Any visitSelectStatement(N1QLParser::SelectStatementContext *ctx) override {
            auto result = MutableDict::newDict();

            // SELECT:
            N1QLParser::SelectContext *select = ctx->select();
            if (select->DISTINCT() != nullptr) {
                result["DISTINCT"_sl] = true;
            }
            addVisit(result, "WHAT", select);

            // WHERE:
            addVisit(result, "WHERE", ctx->where());

            // GROUP BY:
            N1QLParser::GroupByContext *groupBy = ctx->groupBy();
            if (groupBy != nullptr) {
                addVisit(result, "GROUP_BY", groupBy);
                // HAVING:
                addVisit(result, "HAVING", groupBy->having());
            }

            // ORDER BY:
            addVisit(result, "ORDER_BY", ctx->orderBy());

            // LIMIT:
            N1QLParser::LimitContext *limit = ctx->limit();
            if (limit != nullptr) {
                addVisit(result, "LIMIT", limit->expression());
                // OFFSET
                N1QLParser::OffsetContext *offset = limit->offset();
                if (offset != nullptr) {
                    addVisit(result, "OFFSET", offset->expression());
                }
            }

            return result;
        }


        Any visitSelect(N1QLParser::SelectContext *ctx) override {
            return arrayOfVisits(ctx->selectResult());
        }


        Any visitSelectResult(N1QLParser::SelectResultContext *ctx) override {
            Any selectResult;
            if (ctx->STAR() != nullptr) {
                stringstream sb;
                N1QLParser::DataSourceNameContext *dataSourceName = ctx->dataSourceName();
                if (dataSourceName != nullptr) {
                    sb << '.';
                    sb << dataSourceName->getText();
                }
                sb << '.';
                selectResult = arrayWith(sb.str().c_str());
            } else {
                selectResult = visit(ctx->expression());
            }

            if (ctx->AS() != nullptr) {
                auto alias = MutableArray::newArray();
                alias.append("AS");
                appendVisit(alias, selectResult);
                selectResult = alias;
            }
            return selectResult;
        }


        Any visitGroupBy(N1QLParser::GroupByContext *ctx) override {
            return arrayOfVisits(ctx->expression());
        }


        Any visitOrderBy(N1QLParser::OrderByContext *ctx) override {
            return arrayOfVisits(ctx->ordering());
        }


        Any visitOrdering(N1QLParser::OrderingContext *ctx) override {
            N1QLParser::OrderContext *order = ctx->order();
            if (order != nullptr && order->DESC()) {
                return visitExpressionWithOperator(order->getText(), ctx->expression());
            }
            return visit(ctx->expression());
        }


#pragma mark - EXPRESSIONS:


        Any visitExpressionParenthesis(N1QLParser::ExpressionParenthesisContext *ctx) override {
            return visit(ctx->expression());
        }

        Any visitExpressionString(N1QLParser::ExpressionStringContext *ctx) override {
            return visitExpressionWithOperator(ctx->PIPE(), ctx->expression());
        }

        Any visitExpressionArithmetic(N1QLParser::ExpressionArithmeticContext *ctx) override {
            return visitExpressionWithOperator(ctx->op->getText(), ctx->expression());
        }

        Any visitExpressionRelation(N1QLParser::ExpressionRelationContext *ctx) override {
            return visitExpressionWithOperator(ctx->op->getText(), ctx->expression());
        }

        Any visitExpressionNot(N1QLParser::ExpressionNotContext *ctx) override {
            return visitExpressionWithOperator(ctx->NOT()->getText(), ctx->expression());
        }

        Any visitExpressionRelationIs(N1QLParser::ExpressionRelationIsContext *ctx) override {
            string is = ctx->IS()->getText();
            string op = ctx->NOT() != nullptr ? (is + " " + ctx->NOT()->getText()) : is;
            return visitExpressionWithOperator(op, ctx->expression());
        }

        Any visitExpressionRelationIn(N1QLParser::ExpressionRelationInContext *ctx) override {
            string in = ctx->IN()->getText();
            string op = ctx->NOT() != nullptr ? (ctx->NOT()->getText() + " " + in) : in;
            return visitExpressionWithOperator(op, ctx->expression());
        }

        Any visitExpressionRelationBetween(N1QLParser::ExpressionRelationBetweenContext *ctx) override {
            return visitExpressionWithOperator(ctx->BETWEEN(), ctx->expression());
        }

        Any visitExpressionAggregate(N1QLParser::ExpressionAggregateContext *ctx) override {
            string op = ctx->AND() != nullptr ? ctx->AND()->getText() : ctx->OR()->getText();
            return visitExpressionWithOperator(op, ctx->expression());
        }

        Any visitExpressionWithOperator(string op,
                                        N1QLParser::ExpressionContext *expression)
        {
            return visitExpressionWithOperator(op, vector<N1QLParser::ExpressionContext*>{expression});
        }

        Any visitExpressionWithOperator(string op,
                                        vector<N1QLParser::ExpressionContext*>expressions)
        {
            auto result = MutableArray::newArray();
            result.append(toUppercase(op).c_str());
            for (N1QLParser::ExpressionContext *expr : expressions) {
                appendVisit(result, expr);
            }
            return result;
        }

        Any visitExpressionWithOperator(TerminalNode *terminal,
                                        vector<N1QLParser::ExpressionContext*>expressions)
        {
            return visitExpressionWithOperator(terminal->getText(), expressions);
        }

        Any visitExpressionCollection(N1QLParser::ExpressionCollectionContext *ctx) override {
            auto result = MutableArray::newArray();
            result.append(ctx->anyEvery()->getText().c_str());
            result.append(ctx->variableName()->getText().c_str());
            appendVisit(result, ctx->expression(0));
            appendVisit(result, ctx->expression(1));
            return result;
        }

        Any visitMeta(N1QLParser::MetaContext *ctx) override {
            return visitProperty(ctx->dataSourceName(),
                                 vector<N1QLParser::PropertyNameContext*>{ctx->propertyName()},
                                 true);
        }

        Any visitProperty(N1QLParser::PropertyContext *ctx) override {
            return visitProperty(ctx->dataSourceName(), ctx->propertyName());
        }

        MutableArray visitProperty(N1QLParser::DataSourceNameContext *dataSourceName,
                                   vector<N1QLParser::PropertyNameContext*>propertyNames,
                                   bool isMeta =false)
        {
            stringstream sb;
            if (dataSourceName != nullptr) {
                sb << '.' << quoteDots(dataSourceName->getText());
            }

            for (N1QLParser::PropertyNameContext *propertyName : propertyNames) {
                string name = propertyName->getText();
                if (isMeta && !hasPrefix(name, "_")) {
                    sb << "_";
                }
                sb << '.' << quoteDots(name);
            }

            return arrayWith(sb.str().c_str());
        }

        Any visitFunction(N1QLParser::FunctionContext *ctx) override {
            string function = ctx->functionName()->getText() + "()";
            return visitExpressionWithOperator(function, ctx->expression());
        }

        Any visitLiteral(N1QLParser::LiteralContext *ctx) override {
            // This is the only visitor method that returns a type other than MutableArray or
            // MutableDict. Literal values are mostly scalars, so those are returned as C++
            // types (long long, double, bool, string.)

            TerminalNode *literal = ctx->STRING_LITERAL();
            if (literal != nullptr) {
                string value = literal->getText();
                return value.substr(1, value.size() - 1);
            }

            TerminalNode *number = ctx->NUMERIC_LITERAL();
            if (number != nullptr) {
                string nstr = number->getText();
                size_t pos;
                long long ll = stoll(nstr, &pos);
                if (pos == nstr.size()) {
                    return ll;
                } else {
                    double d = stod(nstr, &pos);
                    assert(pos == nstr.size());
                    return d;
                }
            }

            TerminalNode *boolean = ctx->BOOLEAN_LITERAL();
            if (boolean != nullptr) {
                bool value = compareIgnoringCase(boolean->getText(), "TRUE") == 0;
                return value;
            }

            TerminalNode *nil = ctx->Null();
            if (nil != nullptr) {
                return Value::null();
            }


            TerminalNode *missing = ctx->MISSING();
            if (missing != nullptr) {
                return arrayWith("MISSING");
            }

            return ctx->getText();
        }

    private:

#pragma mark - INTERNALS:

        // Adding scalar 'Any' values to Fleece collections:

        void addVisit(MutableDict dict, const char *keyStr, antlr4::ParserRuleContext *ctx) {
            if (!ctx)
                return;
            slice key(keyStr);
            Any value = visit(ctx);
            assert(!value.isNull());
            if (value.is<MutableArray>())
                dict.set(key, value.as<MutableArray>());
            else if (value.is<MutableDict>())
                dict.set(key, value.as<MutableDict>());
            else if (value.is<Value>())
                dict.set(key, value.as<Value>());
            else if (value.is<string>())
                dict.set(key, value.as<string>().c_str());
            else if (value.is<long long>())
                dict.set(key, value.as<long long>());
            else if (value.is<double>())
                dict.set(key, value.as<double>());
            else if (value.is<bool>())
                dict.set(key, value.as<bool>());
            else
                throw runtime_error("Unexpected Any type");
        }

        void appendVisit(MutableArray array, antlr4::ParserRuleContext *ctx) {
            if (!ctx)
                return;
            auto value = visit(ctx);
            assert(!value.isNull());
            if (value.is<MutableArray>())
                array.append(value.as<MutableArray>());
            else if (value.is<MutableDict>())
                array.append(value.as<MutableDict>());
            else if (value.is<Value>())
                array.append(value.as<Value>());
            else if (value.is<string>())
                array.append(value.as<string>().c_str());
            else if (value.is<long long>())
                array.append(value.as<long long>());
            else if (value.is<double>())
                array.append(value.as<double>());
            else if (value.is<bool>())
                array.append(value.as<bool>());
            else
                throw runtime_error("Unexpected Any type");
        }

        template <class CTX>
        MutableArray arrayOfVisits(const vector<CTX*> &contexts) {
            auto result = MutableArray::newArray();
            for (auto item : contexts) {
                appendVisit(result, item);
            }
            return result;
        }

        template <class T>
        MutableArray arrayWith(T item) {
            auto array = MutableArray::newArray();
            array.append(item);
            return array;
        }

        // String stuff:

        bool hasPrefix(const std::string &str, const std::string &prefix) noexcept {
            return str.size() >= prefix.size() && memcmp(str.data(), prefix.data(), prefix.size()) == 0;
        }

        int compareIgnoringCase(const std::string &a, const std::string &b) {
            return strcasecmp(a.c_str(), b.c_str());
        }

        string toUppercase(string str) {
            for (char &c : str)
                c = (char)toupper(c);
            return str;
        }

        string quoteDots(const string &str) {
            string quoted;
            for (char c : str) {
                if (c == '.' || c == '[' || c == '$')
                    quoted += '\\';
                quoted += c;
            }
            return quoted;
        }

    };

} }
