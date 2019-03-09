using System;
using sly.buildresult;
using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal enum GeneratorTokenType
    {
        None,

        [Lexeme(GenericToken.KeyWord, "new")] New,

        [Lexeme(GenericToken.SugarToken, ",")] Comma,

        [Lexeme(GenericToken.SugarToken, "(")] OpenParen,

        [Lexeme(GenericToken.SugarToken, ")")] CloseParen,

        [Lexeme(GenericToken.Identifier, IdentifierType.Alpha)]
        Identifier,

        [Lexeme(GenericToken.String, "'", "\\")] [Lexeme(GenericToken.String, "\"", "\\")]
        String,

        [Lexeme(GenericToken.Int)] Int,

        [Lexeme(GenericToken.Double)] Double
    }

    internal abstract class GeneratorArgumentParser<T>
    {
        #region Variables

        private readonly string _input;
        private readonly ILexer<GeneratorTokenType> _lexer;

        #endregion

        #region Constructors

        protected GeneratorArgumentParser(string input)
        {
            _input = input;
            var buildResult = LexerBuilder.BuildLexer(new BuildResult<ILexer<GeneratorTokenType>>());
            if (buildResult.IsError) {
                throw new ApplicationException("Lexer failed to build");
            }

            _lexer = buildResult.Result;
        }

        #endregion

        #region Public Methods

        public T Parse()
        {
            var parenthesisCount = 0;
            Token<GeneratorTokenType> previous = new Token<GeneratorTokenType> {TokenID = GeneratorTokenType.None};
            foreach (var token in _lexer.Tokenize(_input)) {
                switch (token.TokenID) {
                    case GeneratorTokenType.OpenParen:
                        parenthesisCount++;
                        break;
                    case GeneratorTokenType.CloseParen:
                        parenthesisCount--;
                        if (parenthesisCount < 0) {
                            throw new InvalidOperationException($"Syntax error ')' at {token.Position}");
                        }

                        break;
                    default:
                        if (!token.End && parenthesisCount == 0) {
                            throw new InvalidOperationException($"Syntax error '{token.StringWithoutQuotes} at {token.Position}");
                        }

                        break;
                }

                AcceptToken(token, previous);
                previous = token;
            }

            return Finish();
        }

        #endregion

        #region Protected Methods

        protected abstract void AcceptToken(Token<GeneratorTokenType> token, Token<GeneratorTokenType> previous);

        protected abstract T Finish();

        protected void ThrowSyntaxError(Token<GeneratorTokenType> token)
        {
            if (token.End) {
                return;
            }

            throw new InvalidOperationException($"Syntax error '{token.StringWithoutQuotes}' at {token.Position}");
        }

        #endregion
    }
}