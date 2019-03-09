using System;
using System.Threading.Tasks;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class IntegerGeneratorArgs : GeneratorArgumentParser<(long, long)>
    {
        #region Variables

        private int _currentArgument;
        private int? _max;
        private int? _min;

        #endregion

        #region Constructors

        public IntegerGeneratorArgs(string input) : base(input)
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

                    break;
                case GeneratorTokenType.CloseParen:
                    if (previous.TokenID != GeneratorTokenType.Int &&
                        previous.TokenID != GeneratorTokenType.OpenParen) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Int:
                    if (previous.TokenID != GeneratorTokenType.OpenParen &&
                        previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    if (!_min.HasValue) {
                        _min = token.IntValue;
                    } else if (!_max.HasValue) {
                        _max = token.IntValue;
                    }

                    break;
                case GeneratorTokenType.Comma:
                    if (previous.TokenID != GeneratorTokenType.Int &&
                        previous.TokenID != GeneratorTokenType.OpenParen) {
                        ThrowSyntaxError(token);
                    }

                    _currentArgument++;
                    if (_currentArgument >= 2) {
                        ThrowSyntaxError(token);
                    }

                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (long, long) Finish()
        {
            return (_min ?? Int64.MinValue / 2, _max ?? Int64.MaxValue / 2);
        }

        #endregion
    }

    /// <summary>
    /// A generator that generates random 64-bit integers
    /// </summary>
    public sealed class IntegerGenerator : IDataGenerator
    {
        #region Variables

        private readonly long _max;
        private readonly long _min;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Generates random 64-bit integers between min and max";

        /// <inheritdoc />
        public string Signature { get; } = "{{integer(min: int64 = int64.min / 2, max: int64 = int64.max / 2)}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="input">The arguments to the integer function</param>
        public IntegerGenerator(string input)
        {
            var parser = new IntegerGeneratorArgs(input);
            var (min, max) = parser.Parse();
            _min = min;
            _max = max;
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public IntegerGenerator() : this("()")
        {
        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            return Task.FromResult<object>(ThreadSafeRandom.NextInt64(_min, _max));
        }

        #endregion
    }
}