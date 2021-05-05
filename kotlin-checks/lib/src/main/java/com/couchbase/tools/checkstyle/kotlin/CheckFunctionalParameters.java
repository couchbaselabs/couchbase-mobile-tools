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


public class CheckFunctionalParameters extends BaseKotlinChecker {
    public static final String MSG_MISPLACED_FUNCTIONAL_PARAM
        = "com.couchbase.kotlin.param.functional";

    private Set<String> functionalTypes = new HashSet<>();

    public CheckFunctionalParameters() { super(TokenTypes.METHOD_DEF); }

    public void setFunctionalTypes(final String... functionalTypes) {
        this.functionalTypes = new HashSet<>();
        if (functionalTypes != null) { Collections.addAll(this.functionalTypes, functionalTypes); }
    }

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

        final DetailAST params = ast.findFirstToken(TokenTypes.PARAMETERS);
        if (params == null) { return; }

        DetailAST param = params.getLastChild();
        if (param == null) { return; }

        for (param = param.getPreviousSibling(); param != null; param = param.getPreviousSibling()) {
            if (param.getType() != TokenTypes.PARAMETER_DEF) { continue; }

            final DetailAST paramType = param.findFirstToken(TokenTypes.TYPE);
            if (paramType == null) { continue; }

            final DetailAST paramTypeIdentifier = paramType.findFirstToken(TokenTypes.IDENT);
            if (paramTypeIdentifier == null) { continue; }

            final String paramTypeName = paramTypeIdentifier.getText();
            if (functionalTypes.contains(paramTypeName)) {
                final DetailAST methodIdentifier = ast.findFirstToken(TokenTypes.IDENT);
                final String methodName = (methodIdentifier == null) ? "???" : methodIdentifier.getText();

                final DetailAST paramIdentifier = param.findFirstToken(TokenTypes.IDENT);
                final String paramName = (paramIdentifier == null) ? "???" : paramIdentifier.getText();

                log(ast, MSG_MISPLACED_FUNCTIONAL_PARAM, paramName, methodName, paramTypeName);
            }
        }
    }
}
