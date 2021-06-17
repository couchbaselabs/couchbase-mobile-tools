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

import java.util.Arrays;
import java.util.HashSet;

import com.puppycrawl.tools.checkstyle.api.AbstractCheck;
import com.puppycrawl.tools.checkstyle.api.DetailAST;
import com.puppycrawl.tools.checkstyle.api.FullIdent;
import com.puppycrawl.tools.checkstyle.api.TokenTypes;


/**
 * Based on sevntu.checkstyle JSR305AnnotationsCheck
 */
public abstract class BaseKotlinChecker extends AbstractCheck {
    public static final String MOD_PUBLIC = "public";
    public static final String MOD_PROTECTED = "protected";

    public interface DefinitionChecker {
        void check(DetailAST ast);
    }


    private final int[] acceptedTypes;
    private String[] packages = new String[0];
    private String[] excludedPackages = new String[0];
    private boolean ignore;

    protected BaseKotlinChecker(int... types) { acceptedTypes = types; }

    public void setPackages(final String... packageNames) { packages = unique(packageNames); }

    public void setExcludedPackages(final String... packageNames) { excludedPackages = unique(packageNames); }

    @Override
    public final int[] getDefaultTokens() { return getAcceptableTokens(); }

    @Override
    public final int[] getRequiredTokens() { return new int[0]; }

    @Override
    public final int[] getAcceptableTokens() { return acceptedTypes; }


    protected abstract DefinitionChecker visitDefinition(final DetailAST ast);

    @Override
    public void beginTree(DetailAST rootAST) { ignore = false; }

    @Override
    public final void visitToken(final DetailAST ast) {
        if (ast.getType() == TokenTypes.PACKAGE_DEF) {
            ignore = isPackageExcluded(FullIdent.createFullIdent(ast.getLastChild().getPreviousSibling()));
            return;
        }

        if (ignore) { return; }

        final DefinitionChecker checker = visitDefinition(ast);
        if (checker != null) { checker.check(ast); }
    }

    public final boolean isPackageExcluded(final FullIdent fullIdent) {
        final String packageName = fullIdent.getText();

        for (final String excludedPackageNames: excludedPackages) {
            if (packageName.startsWith(excludedPackageNames)) { return true; }
        }

        for (final String includePackageName: packages) {
            if (packageName.startsWith(includePackageName)) { return false; }
        }

        return true;
    }

    public final boolean isPrimitiveType(DetailAST ast) {
        final DetailAST parameterType = ast.findFirstToken(TokenTypes.TYPE);
        final DetailAST identToken = parameterType.getFirstChild();

        if (identToken != null) {
            switch (identToken.getType()) {
                case TokenTypes.LITERAL_BOOLEAN:
                case TokenTypes.LITERAL_INT:
                case TokenTypes.LITERAL_LONG:
                case TokenTypes.LITERAL_SHORT:
                case TokenTypes.LITERAL_BYTE:
                case TokenTypes.LITERAL_CHAR:
                case TokenTypes.LITERAL_VOID:
                case TokenTypes.LITERAL_DOUBLE:
                case TokenTypes.LITERAL_FLOAT:
                    return !isArrayOrElipsis(parameterType);
            }
        }

        return false;
    }

    protected final boolean isArrayOrElipsis(final DetailAST identToken) {
        switch (identToken.getNextSibling().getType()) {
            case TokenTypes.ARRAY_DECLARATOR:
            case TokenTypes.ELLIPSIS:
                return true;
        }
        return false;
    }

    protected boolean isVisible(final DetailAST ast) {
        final DetailAST modifiers = ast.findFirstToken(TokenTypes.MODIFIERS);
        if (modifiers == null) { return false; }
        for (DetailAST child = modifiers.getFirstChild(); child != null; child = child.getNextSibling()) {
            if (MOD_PUBLIC.equals(child.getText()) || MOD_PROTECTED.equals(child.getText())) { return true; }
        }
        return false;
    }

    protected boolean isVoid(DetailAST ast) {
        DetailAST type = ast.findFirstToken(TokenTypes.TYPE);
        return (type != null) && type.findFirstToken(TokenTypes.LITERAL_VOID) != null;
    }

    protected String[] unique(final String... input) {
        return new HashSet<>(Arrays.asList(input)).toArray(new String[0]);
    }
}
