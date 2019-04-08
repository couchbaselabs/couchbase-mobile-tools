using System;
using System.Collections.Generic;
using System.Threading.Tasks;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class RangeGeneratorArgs : GeneratorArgumentParser<(int, int, int)>
    {
        #region Variables

        private int _currentArgument;
        private int _start;
        private int _step = 1;
        private int _stop;

        #endregion

        #region Constructors

        public RangeGeneratorArgs(string input) : base(input)
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

                    _currentArgument++;
                    break;
                case GeneratorTokenType.CloseParen:
                    if (previous.TokenID != GeneratorTokenType.Int) {
                        if (previous.TokenID == GeneratorTokenType.OpenParen) {
                            throw new InvalidOperationException("Missing argument 'stop'");
                        }

                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Int:
                    if (previous.TokenID != GeneratorTokenType.Comma &&
                        previous.TokenID != GeneratorTokenType.OpenParen) {
                        ThrowSyntaxError(token);
                    }

                    if (_currentArgument == 1) {
                        _stop = token.IntValue;
                    } else if (_currentArgument == 2) {
                        _start = _stop;
                        _stop = token.IntValue;
                    } else {
                        _step = token.IntValue;
                    }

                    break;
                case GeneratorTokenType.Comma:
                    if (previous.TokenID != GeneratorTokenType.Int) {
                        ThrowSyntaxError(token);
                    }

                    _currentArgument++;
                    if (_currentArgument > 3) {
                        ThrowSyntaxError(token);
                    }

                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (int, int, int) Finish()
        {
            return (_start, _stop, _step);
        }

        #endregion
    }

    /// <summary>
    /// A generator that creates an array of numbers in a range
    /// </summary>
    public sealed class RangeGenerator : IDataGenerator
    {
        #region Variables

        private readonly int _start, _stop, _step;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Generates an array of numbers between min and max, increasing by step.";

        /// <inheritdoc />
        public string Signature { get; } = "{{range(start: int32 = 0, stop: int32, step: int32 = 1)}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="input">The arguments to the range function</param>
        public RangeGenerator(string input)
        {
            var parser = new RangeGeneratorArgs(input);
            (int start, int stop, int step) result = parser.Parse();
            _start = result.start;
            _stop = result.stop;
            _step = result.step;
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public RangeGenerator() : this("(0)")
        {
        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            var retVal = new List<int>();
            for (int i = _start; i < _stop; i += _step) {
                retVal.Add(i);
            }

            return Task.FromResult<object>(retVal);
        }

        #endregion
    }
}