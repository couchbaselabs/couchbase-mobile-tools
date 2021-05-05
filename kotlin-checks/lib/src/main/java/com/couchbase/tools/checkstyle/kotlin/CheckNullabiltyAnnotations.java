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
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;

import com.puppycrawl.tools.checkstyle.api.DetailAST;
import com.puppycrawl.tools.checkstyle.api.TokenTypes;
import com.puppycrawl.tools.checkstyle.utils.TokenUtil;


public class CheckNullabiltyAnnotations extends BaseKotlinChecker {
    public static final String MSG_FIELD_NOT_ANNOTATED
        = "com.couchbase.kotlin.nullability.field.annotation";
    public static final String MSG_PRIMITIVE_FIELD_ANNOTATED
        = "com.couchbase.kotlin.nullability.field.primitive";

    public static final String MSG_CONSTRUCTOR_WITH_RETURN_ANNOTATION
        = "com.couchbase.kotlin.nullability.constructor.annotation";

    public static final String MSG_METHOD_RETURN_VALUE_NOT_ANNOTATED
        = "com.couchbase.kotlin.nullability.method.annotation";
    public static final String MSG_VOID_METHOD_RETURN_VALUE_ANNOTATED
        = "com.couchbase.kotlin.nullability.method.void";
    public static final String MSG_PRIMITIVE_METHOD_RETURN_VALUE_ANNOTATED
        = "com.couchbase.kotlin.nullability.method.primitive";

    public static final String MSG_METHOD_PARAMETER_NOT_ANNOTATED
        = "com.couchbase.kotlin.nullability.parameter.annotation";
    public static final String MSG_PRIMITIVE_METHOD_PARAMETER_ANNOTATED
        = "com.couchbase.kotlin.nullability.parameter.primitive";

    public static final String MSG_CONFLICTING_ANNOTATIONS
        = "com.couchbase.kotlin.annotations.conflict";

    public enum NullabilityAnnotation {
        NULLABLE("Nullable", "android.support.annotation"),
        NONNULL("NonNull", "android.support.annotation");

        private final String annotationName;
        private final String fullyQualifiedClassName;

        NullabilityAnnotation(final String annotationName, final String packageName) {
            this.annotationName = annotationName;
            fullyQualifiedClassName = packageName + "." + annotationName;
        }
    }

    private static final Set<NullabilityAnnotation> ANNOTATIONS;
    private static final Map<String, NullabilityAnnotation> STRING_TO_ANNOTATION;

    static {
        final Map<String, NullabilityAnnotation> m = new HashMap<>();
        for (final NullabilityAnnotation annotation: NullabilityAnnotation.values()) {
            m.put(annotation.annotationName, annotation);
            m.put(annotation.fullyQualifiedClassName, annotation);
        }
        STRING_TO_ANNOTATION = Collections.unmodifiableMap(m);
        ANNOTATIONS = Collections.unmodifiableSet(new HashSet<>(m.values()));
    }

    public CheckNullabiltyAnnotations() {
        super(
            TokenTypes.PACKAGE_DEF,
            TokenTypes.CTOR_DEF,
            TokenTypes.METHOD_DEF,
            TokenTypes.PARAMETER_DEF,
            TokenTypes.VARIABLE_DEF);
    }

    protected final DefinitionChecker visitDefinition(final DetailAST ast) {
        final int type = ast.getType();
        switch (type) {
            case TokenTypes.VARIABLE_DEF:
                return this::checkField;
            case TokenTypes.CTOR_DEF:
                return this::checkConstructor;
            case TokenTypes.METHOD_DEF:
                return this::checkMethod;
            case TokenTypes.PARAMETER_DEF:
                return this::checkMethodParameter;
        }

        throw new IllegalArgumentException("Visiting unsupported token: " + TokenUtil.getTokenName(type));
    }

    private void checkField(final DetailAST ast) {
        if (!isField(ast)) { return; }

        if (isConst(ast)) { return; }

        if (isPrimitiveType(ast)) {
            checkAndNotify(ast, this::containsAny, MSG_PRIMITIVE_FIELD_ANNOTATED);
            return;
        }

        checkAndNotify(ast, this::containsNone, MSG_FIELD_NOT_ANNOTATED);
        checkAndNotify(ast, this::containsAll, CheckNullabiltyAnnotations.MSG_CONFLICTING_ANNOTATIONS);
    }

    private void checkConstructor(final DetailAST ast) {
        checkAndNotify(ast, this::containsAny, MSG_CONSTRUCTOR_WITH_RETURN_ANNOTATION);
    }

    private void checkMethod(final DetailAST ast) {
        if (isVoid(ast)) {
            checkAndNotify(ast, this::containsAny, MSG_VOID_METHOD_RETURN_VALUE_ANNOTATED);
            return;
        }

        if (isPrimitiveType(ast)) {
            checkAndNotify(ast, this::containsAny, MSG_PRIMITIVE_METHOD_RETURN_VALUE_ANNOTATED);
            return;
        }

        checkAndNotify(ast, this::containsNone, MSG_METHOD_RETURN_VALUE_NOT_ANNOTATED);
        checkAndNotify(ast, this::containsAll, MSG_CONFLICTING_ANNOTATIONS);
    }

    private void checkMethodParameter(final DetailAST ast) {
        if (isPrimitiveType(ast)) {
            checkAndNotify(ast, this::containsAny, MSG_PRIMITIVE_METHOD_PARAMETER_ANNOTATED);
            return;
        }

        checkAndNotify(ast, this::containsNone, MSG_METHOD_PARAMETER_NOT_ANNOTATED);
        checkAndNotify(ast, this::containsAll, MSG_CONFLICTING_ANNOTATIONS);
    }

    private boolean isField(DetailAST ast) {
        DetailAST parent = ast.getParent();
        if (parent == null) { return false; }

        parent = parent.getParent();
        return (parent != null) && (parent.getType() == TokenTypes.CLASS_DEF);
    }

    private boolean isConst(DetailAST ast) {
        DetailAST modifiers = ast.findFirstToken(TokenTypes.MODIFIERS);
        if (modifiers == null) { return false; }

        return modifiers.findFirstToken(TokenTypes.FINAL) != null;
    }


    private void checkAndNotify(DetailAST ast, Predicate<Set<NullabilityAnnotation>> validator, String msg) {
        if (validator.test(findAnnotations(ast))) { log(ast, msg, ast.findFirstToken(TokenTypes.IDENT).getText()); }
    }

    private boolean containsNone(final Set<NullabilityAnnotation> annotations) { return !containsAny(annotations); }

    private boolean containsAll(final Set<NullabilityAnnotation> annotations) {
        if (annotations.isEmpty()) { return false; }
        for (NullabilityAnnotation annotation: ANNOTATIONS) {
            if (!annotations.contains(annotation)) { return false; }
        }
        return true;
    }

    private boolean containsAny(final Set<NullabilityAnnotation> annotations) {
        if (annotations.isEmpty()) { return false; }
        for (NullabilityAnnotation annotation: ANNOTATIONS) {
            if (annotations.contains(annotation)) { return true; }
        }
        return false;
    }

    private Set<NullabilityAnnotation> findAnnotations(final DetailAST ast) {
        final Set<NullabilityAnnotation> annotations = new HashSet<>();
        final DetailAST modifiers = ast.findFirstToken(TokenTypes.MODIFIERS);
        for (DetailAST child = modifiers.getFirstChild(); child != null; child = child.getNextSibling()) {
            if (child.getType() != TokenTypes.ANNOTATION) { continue; }

            final DetailAST identifier = child.findFirstToken(TokenTypes.IDENT);
            if (identifier == null) { continue; }

            final NullabilityAnnotation annotation = STRING_TO_ANNOTATION.get(identifier.getText());
            if (annotation == null) { continue; }

            annotations.add(annotation);
        }

        return annotations;
    }
}
