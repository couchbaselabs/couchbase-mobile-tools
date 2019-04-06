grammar N1QL;

sql
  : ( selectStatement )
  ;

selectStatement
  : select ( from )? ( where )? ( groupBy )? ( orderBy )? ( limit )? ';' ?
  ;

select
  : SELECT ( DISTINCT )? selectResult ( COMMA selectResult )*
  ;

selectResult
  : ( dataSourceName DOT )? STAR ( AS columnAlias )?
  | expression ( AS columnAlias )?
  ;

columnAlias
  : IDENTIFIER
  ;

from
  : FROM ( dataSource | joins )
  ;

dataSource
  : databaseName ( AS databaseAlias )?
  ;

joins
  : dataSource ( joinOperator dataSource joinConstraint )*
  ;

joinOperator
  : ( LEFT OUTER? | INNER | CROSS )? JOIN
  ;

joinConstraint
  : ( ON expression )?
  ;

where
  : WHERE expression
  ;

groupBy
  : GROUP BY expression ( COMMA expression )* ( having )?
  ;

having
  : HAVING expression
  ;

orderBy
  : ORDER BY ordering ( COMMA ordering )*
  ;

ordering
  : expression ( order )?
  ;

order
  : ( ASC | DESC )
  ;

limit
  :  LIMIT expression ( offset )?
  ;

offset
  : ( OFFSET ) expression
  ;

expression
  : LPAR expression RPAR                                        #expressionParenthesis
  | expression PIPE expression                                  #expressionString
  | expression op=(STAR|DIV|MOD) expression                     #expressionArithmetic
  | expression op=(PLUS|MINUS) expression                       #expressionArithmetic
  | expression op=(LT|LT_EQ|GT|GT_EQ) expression                #expressionRelation
  | expression op=(EQ|NOT_EQ|LIKE|REGEX|MATCH) expression       #expressionRelation
  | expression IS NOT? expression                               #expressionRelationIs
  | expression NOT? IN expression                               #expressionRelationIn
  | expression BETWEEN expression AND expression                #expressionRelationBetween
  | anyEvery variableName IN expression SATISFIES expression    #expressionCollection
  | NOT expression                                              #expressionNot
  | expression AND expression                                   #expressionAggregate
  | expression OR expression                                    #expressionAggregate
  | function                                                    #expressionFunction
  | meta                                                        #expressionMeta
  | property                                                    #expressionProperty
  | literal                                                     #expressionLiteral
  ;

literal
  : NUMERIC_LITERAL
  | BOOLEAN_LITERAL
  | STRING_LITERAL
  | Null
  | MISSING
  ;

meta
  : ( dataSourceName DOT )? META DOT propertyName
  ;

property
  : ( dataSourceName DOT )? propertyName ( DOT propertyName )*
  ;

function
  : functionName LPAR ( expression ( COMMA expression )* )? RPAR
  ;

anyEvery
  : ( ANY | EVERY | ANY AND EVERY )
  ;

databaseName
  : IDENTIFIER
  ;

dataSourceName
  : IDENTIFIER
  ;

databaseAlias
  : IDENTIFIER
  ;

functionName
  : IDENTIFIER
  ;

propertyName
  : IDENTIFIER
  ;

variableName
  : IDENTIFIER
  ;

// Keywords:
AND       : A N D;
ANY       : A N Y;
AS        : A S;
ASC       : A S C;
BETWEEN   : B E T W E E N;
BY        : B Y;
CROSS     : C R O S S;
DESC      : D E S C;
DISTINCT  : D I S T I N C T;
EVERY     : E V E R Y;
FROM      : F R O M;
GROUP     : G R O U P;
HAVING    : H A V I N G;
IN        : I N;
INNER     : I N N E R;
IS        : I S;
JOIN      : J O I N;
LEFT      : L E F T;
LIKE      : L I K E;
LIMIT     : L I M I T;
MATCH     : M A T C H;
META      : M E T A;
NATURAL   : N A T U R A L;
NOT       : N O T;
Null      : N U L L;
MISSING   : M I S S I N G;
OFFSET    : O F F S E T;
ON        : O N;
OR        : O R;
ORDER     : O R D E R;
OUTER     : O U T E R;
REGEX     : R E G E X;
RIGHT     : R I G H T;
SATISFIES : S A T I S F I E S;
SELECT    : S E L E C T;
USING     : U S I N G;
WHERE     : W H E R E;

// Symbols:
COMMA     : ',';
DOT       : '.';
STAR      : '*';
PLUS      : '+';
MINUS     : '-';
PIPE      : '||';
DIV       : '/';
MOD       : '%';
LT        : '<';
LT_EQ     : '<=';
GT        : '>';
GT_EQ     : '>=';
EQ        : '=';
NOT_EQ    : '!=';
LPAR      : '(';
RPAR      : ')';

BOOLEAN_LITERAL
  : (T R U E | F A L S E)
  ;

NUMERIC_LITERAL
  : DIGIT+ ( '.' DIGIT* )? ( E [-+]? DIGIT+ )?
  | '.' DIGIT+ ( E [-+]? DIGIT+ )?
  ;

STRING_LITERAL
  : '\'' ( ~'\'' | '\'\'' )* '\''
  | '"' (~'"' | '""')* '"'
  ;

IDENTIFIER
  : [a-zA-Z_] [a-zA-Z_0-9]*
  ;

SPACES
  : [ \u000B\t\r\n] -> channel(HIDDEN)
  ;

fragment DIGIT : [0-9];

fragment A : [aA];
fragment B : [bB];
fragment C : [cC];
fragment D : [dD];
fragment E : [eE];
fragment F : [fF];
fragment G : [gG];
fragment H : [hH];
fragment I : [iI];
fragment J : [jJ];
fragment K : [kK];
fragment L : [lL];
fragment M : [mM];
fragment N : [nN];
fragment O : [oO];
fragment P : [pP];
fragment Q : [qQ];
fragment R : [rR];
fragment S : [sS];
fragment T : [tT];
fragment U : [uU];
fragment V : [vV];
fragment W : [wW];
fragment X : [xX];
fragment Y : [yY];
fragment Z : [zZ];
