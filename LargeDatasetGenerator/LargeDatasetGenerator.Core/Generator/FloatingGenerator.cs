using System;
using System.Threading.Tasks;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class FloatingGeneratorParser : GeneratorArgumentParser<(double, double, int?)>
    {
        #region Variables

        private int _currentArgument;
        private int? _digits;
        private double? _max;
        private double? _min;

        #endregion

        #region Constructors

        public FloatingGeneratorParser(string input) : base(input)
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
                    if (previous.TokenID != GeneratorTokenType.Double &&
                        previous.TokenID != GeneratorTokenType.Int) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Double:
                    if (!_min.HasValue) {
                        _min = token.DoubleValue;
                    } else {
                        if (_currentArgument > 1) {
                            ThrowSyntaxError(token);
                        }

                        _max = token.DoubleValue;
                    }

                    break;
                case GeneratorTokenType.Int:
                    if (!_min.HasValue) {
                        _min = token.IntValue;
                    } else if (!_max.HasValue) {
                        _max = token.IntValue;
                    } else {
                        if (_currentArgument > 2) {
                            ThrowSyntaxError(token);
                        }

                        _digits = token.IntValue;
                    }

                    break;
                case GeneratorTokenType.Comma:
                    _currentArgument++;
                    if (_currentArgument >= 3) {
                        ThrowSyntaxError(token);
                    }

                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (double, double, int?) Finish()
        {
            return (_min ?? Double.MinValue / 2, _max ?? Double.MaxValue / 2, _digits);
        }

        #endregion
    }

    public sealed class FloatingGenerator : IDataGenerator
    {
        #region Variables

        private readonly int? _digits;
        private readonly double _max;
        private readonly double _min;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } =
            "Randomly generates a number between min and max, rounded to X digits after the decimal point";

        /// <inheritdoc />
        public string Signature { get; } = "{{floating(min: number = (double.min / 2.0), max: number = (double.max / 2.0), digits: number = (no value))}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="parameters">The arguments to the floating method</param>
        public FloatingGenerator(string parameters)
        {
            var parser = new FloatingGeneratorParser(parameters);
            (double min, double max, int? digits) result = parser.Parse();
            _min = result.min;
            _max = result.max;
            _digits = result.digits;
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public FloatingGenerator() : this("()")
        {

        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            var value = ThreadSafeRandom.NextDouble() * (_max - _min) + _min;
            if (_digits.HasValue) {
                value = Math.Round(value, _digits.Value);
            }

            return Task.FromResult<object>(value);
        }

        #endregion
    }
}