using System;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class RepeatGeneratorArgs : GeneratorArgumentParser<(int, int)>
    {
        #region Variables

        private int _currentArg;
        private int _max;
        private int _min;

        #endregion

        #region Constructors

        public RepeatGeneratorArgs(string input) : base(input)
        {
        }

        #endregion

        #region Overrides

        protected override void AcceptToken(Token<GeneratorTokenType> token, Token<GeneratorTokenType> previous)
        {
            switch (token.TokenID) {
                case GeneratorTokenType.OpenParen:
                    if (previous.TokenID != GeneratorTokenType.None) {
                        ThrowSyntaxError(token);
                    }

                    _currentArg++;
                    break;
                case GeneratorTokenType.CloseParen:
                    if (_currentArg < 2) {
                        throw new InvalidOperationException("Missing argument 'min'");
                    }

                    if (previous.TokenID != GeneratorTokenType.Int) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Int:
                    if (previous.TokenID != GeneratorTokenType.OpenParen &&
                        previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    if (_currentArg == 1) {
                        _min = _max = token.IntValue;
                    } else {
                        _max = token.IntValue;
                    }

                    break;
                case GeneratorTokenType.Comma:
                    if (previous.TokenID != GeneratorTokenType.Int) {
                        ThrowSyntaxError(token);
                    }

                    _currentArg++;
                    if (_currentArg > 2) {
                        ThrowSyntaxError(token);
                    }

                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (int, int) Finish()
        {
            return (_min, _max);
        }

        #endregion
    }
}