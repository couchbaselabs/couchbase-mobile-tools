//
// Copyright (c) 2021 Couchbase, Inc All rights reserved.
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
package com.couchbase.tools.checkstyle.kotlin;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

import com.puppycrawl.tools.checkstyle.api.DetailAST;
import com.puppycrawl.tools.checkstyle.api.TokenTypes;
import com.puppycrawl.tools.checkstyle.utils.TokenUtil;


public class CheckKotlinReserveWords extends BaseKotlinChecker {
    public static final String MSG_METHOD_NAME_IS_RESERVED
        = "com.couchbase.kotlin.reserved.method";
    public static final String MSG_IDENTIFIER_IS_RESERVED
        = "com.couchbase.kotlin.reserved.identifier";

    private static final Set<String> KEYWORDS;
    static {
        final HashSet<String> s = new HashSet<>();
        // hard keywords
        s.add("as");
        s.add("fun");
        s.add("in");
        s.add("is");
        s.add("object");
        s.add("typealias");
        s.add("typeof");
        s.add("val");
        s.add("var");
        s.add("when");

        // soft keywords
        s.add("by");
        s.add("constructor");
        s.add("delegate");
        s.add("operator");
        s.add("init");
        s.add("value");
        s.add("where");

        // special identifiers
        s.add("field");
        s.add("it");

        KEYWORDS = Collections.unmodifiableSet(s);
    }

    private static final Set<String> SPECIAL_IDENTIFIERS;
    static {
        final HashSet<String> s = new HashSet<>();

        // operators
        s.add("unaryPlus");
        s.add("unaryMinus");
        s.add("not");
        s.add("inc");
        s.add("dec");
        s.add("plus");
        s.add("minus");
        s.add("times");
        s.add("div");
        s.add("rem");
        s.add("mod");
        s.add("rangeTo");
        s.add("contains");
        s.add("get");
        s.add("set");
        s.add("invoke");
        s.add("plusAssign");
        s.add("minusAssign");
        s.add("timesAssign");
        s.add("divAssign");
        s.add("remAssign");
        s.add("modAssign");
        s.add("compareTo");
        s.add("provideDelegate");
        s.add("getValue");
        s.add("setValue");

        // scope functions
        s.add("let");
        s.add("run");
        s.add("with");
        s.add("apply");
        s.add("also");
        s.add("takeIf");
        s.add("takeUnless");

        // common infix operators
        s.add("to");
        s.add("and");
        s.add("or");
        s.add("xor");
        s.add("matches");

        SPECIAL_IDENTIFIERS = Collections.unmodifiableSet(s);
    }

    public CheckKotlinReserveWords() {
        super(
            TokenTypes.PACKAGE_DEF,
            TokenTypes.INTERFACE_DEF,
            TokenTypes.ENUM_DEF,
            TokenTypes.CLASS_DEF,
            TokenTypes.VARIABLE_DEF);
    }

    protected final DefinitionChecker visitDefinition(final DetailAST ast) {
        final int type = ast.getType();
        switch (type) {
            case TokenTypes.INTERFACE_DEF:
            case TokenTypes.ENUM_DEF:
            case TokenTypes.CLASS_DEF:
                return this::checkIdentifier;
            case TokenTypes.METHOD_DEF:
                return this::checkMethod;
            case TokenTypes.VARIABLE_DEF:
                return this::checkField;
        }

        throw new IllegalArgumentException("Visiting unsupported token: " + TokenUtil.getTokenName(type));
    }

    private void checkMethod(final DetailAST ast) {
        if (!checkIdentifier(ast)) { return; }

        final String identifier = ast.findFirstToken(TokenTypes.IDENT).getText();
        if (SPECIAL_IDENTIFIERS.contains(identifier)) {
            log(ast, MSG_METHOD_NAME_IS_RESERVED, identifier);
            return;
        }
    }

    private void checkField(final DetailAST ast) {
        if (isField(ast)) { checkIdentifier(ast); }
    }

    private boolean checkIdentifier(final DetailAST ast) {
        if (!isVisible(ast)) { return true; }

        final String identifier = ast.findFirstToken(TokenTypes.IDENT).getText();
        if (KEYWORDS.contains(identifier)) {
            log(ast, MSG_IDENTIFIER_IS_RESERVED, identifier);
            return false;
        }

        return true;
    }

    private boolean isField(DetailAST ast) {
        DetailAST parent = ast.getParent();
        if (parent == null) { return false; }
        parent = parent.getParent();
        return (parent != null) && (parent.getType() == TokenTypes.CLASS_DEF);
    }
}
