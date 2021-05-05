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

import com.puppycrawl.tools.checkstyle.api.DetailAST;
import com.puppycrawl.tools.checkstyle.api.TokenTypes;
import com.puppycrawl.tools.checkstyle.utils.TokenUtil;


public class CheckGettersAndSetters extends BaseKotlinChecker {
    public static final String MOD_PUBLIC = "public";
    public static final String MOD_PROTECTED = "protected";

    public static final String MSG_SETTER_SHOULD_BE_VOID
        = "com.couchbase.kotlin.setter.void";
    public static final String MSG_SETTER_SHOULD_HAVE_ONE_PARAMETER
        = "com.couchbase.kotlin.setter.parameter";
    public static final String MSG_GETTER_SHOULD_NOT_BE_VOID
        = "com.couchbase.kotlin.getter.void";
    public static final String MSG_SETTER_SHOULD_HAVE_NO_PARAMETERS
        = "com.couchbase.kotlin.getter.parameter";


    public CheckGettersAndSetters() { super(TokenTypes.METHOD_DEF); }

    protected final DefinitionChecker visitDefinition(final DetailAST ast) {
        final int type = ast.getType();
        switch (type) {
            case TokenTypes.METHOD_DEF:
                return this::checkMethod;
        }

        throw new IllegalArgumentException("Visiting unsupported token: " + TokenUtil.getTokenName(type));
    }

    private void checkMethod(final DetailAST ast) {
        if (!isVisible(ast)) { return; }

        final DetailAST methodIdentifier = ast.findFirstToken(TokenTypes.IDENT);
        if (methodIdentifier == null) { return; }

        final String methodName = methodIdentifier.getText();
        if (methodName.startsWith("get")) { checkGetter(ast, methodName); }
        else if (methodName.startsWith("set")) { checkSetter(ast, methodName); }
    }

    private void checkSetter(DetailAST ast, String methodName) {
        if (!isVoid(ast)) { log(ast, MSG_SETTER_SHOULD_BE_VOID, methodName); }

        final DetailAST params = ast.findFirstToken(TokenTypes.PARAMETERS);
        if ((params != null) && (params.getChildCount() != 1)) {
            log(ast, MSG_SETTER_SHOULD_HAVE_ONE_PARAMETER, methodName);
        }
    }

    private void checkGetter(DetailAST ast, String methodName) {
        if (isVoid(ast)) { log(ast, MSG_GETTER_SHOULD_NOT_BE_VOID, methodName); }

        final DetailAST params = ast.findFirstToken(TokenTypes.PARAMETERS);
        if ((params != null) && (params.getChildCount() > 0)) {
            log(ast, MSG_SETTER_SHOULD_HAVE_NO_PARAMETERS, methodName);
        }
    }
}
