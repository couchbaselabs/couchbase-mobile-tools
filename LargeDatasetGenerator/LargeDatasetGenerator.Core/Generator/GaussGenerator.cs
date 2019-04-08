using System;
using System.Threading.Tasks;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class GaussGeneratorArgs : GeneratorArgumentParser<(double, double)>
    {
        #region Variables

        private int _index;
        private double? _mean;
        private double? _stddev;

        #endregion

        #region Constructors

        public GaussGeneratorArgs(string input) : base(input)
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
                    if (previous.TokenID != GeneratorTokenType.Int) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Int:
                    if (previous != null && previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    if (_index == 0) {
                        _mean = token.IntValue;
                    } else {
                        _stddev = token.IntValue;
                    }

                    break;
                case GeneratorTokenType.Double:
                    if (previous != null && previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    if (_index == 0) {
                        _mean = token.DoubleValue;
                    } else {
                        _stddev = token.DoubleValue;
                    }

                    break;
                case GeneratorTokenType.Comma:
                    _index++;
                    if (_index > 1) {
                        ThrowSyntaxError(token);
                    }

                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (double, double) Finish()
        {
            return (_mean ?? 0.0, _stddev ?? 1.0);
        }

        #endregion
    }

    public sealed class GaussGenerator : IDataGenerator
    {
        #region Variables

        private readonly double _mean;
        private readonly double _stddev;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Generates normally distributed random floating point values";

        /// <inheritdoc />
        public string Signature { get; } = "{{gauss(mu: number = 0.0, sigma: number = 1.0)}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="input">The arguments to the gauss function</param>
        public GaussGenerator(string input)
        {
            var parser = new GaussGeneratorArgs(input);
            (double mean, double stddev) result = parser.Parse();
            _mean = result.mean;
            _stddev = result.stddev;
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public GaussGenerator() : this("()")
        {
        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            var x1 = 1 - ThreadSafeRandom.NextDouble();
            var x2 = 1 - ThreadSafeRandom.NextDouble();

            var y1 = Math.Sqrt(-2.0 * Math.Log(x1)) * Math.Cos(2.0 * Math.PI * x2);
            return Task.FromResult<object>(y1 * _stddev + _mean);
        }

        #endregion
    }
}