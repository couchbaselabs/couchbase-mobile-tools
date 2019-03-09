using System;
using System.Threading.Tasks;

using LargeDatasetGenerator.Core.Generator;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class DateTimeGeneratorParser : GeneratorArgumentParser<(DateTimeOffset, DateTimeOffset)>
    {
        #region Variables

        private DateObjectArgs _currentDate;
        private int _currentEntry;
        private DateTimeOffset _start = new DateTimeOffset(1970, 1, 1, 0, 0, 0, TimeSpan.Zero);
        private DateTimeOffset _end = DateTimeOffset.UtcNow;

        #endregion

        #region Constructors

        public DateTimeGeneratorParser(string input) : base(input)
        {
        }

        #endregion

        #region Overrides

        protected override void AcceptToken(Token<GeneratorTokenType> token, Token<GeneratorTokenType> previous)
        {
            switch (token.TokenID) {
                case GeneratorTokenType.New:
                    if (_currentDate != null) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Identifier:
                    if (previous.TokenID != GeneratorTokenType.New) {
                        ThrowSyntaxError(token);
                    }

                    if (token.StringWithoutQuotes != "Date") {
                        ThrowSyntaxError(token);
                    }

                    _currentEntry += 1;
                    break;
                case GeneratorTokenType.OpenParen:
                    if (previous.TokenID != GeneratorTokenType.Identifier &&
                        previous.TokenID != GeneratorTokenType.None) {
                        ThrowSyntaxError(token);
                    }

                    if (previous.TokenID != GeneratorTokenType.None) {
                        _currentDate = new DateObjectArgs(_currentEntry == 1 ? _start : _end);
                    }
                    break;
                case GeneratorTokenType.Comma:
                    if (previous.TokenID != GeneratorTokenType.CloseParen &&
                        previous.TokenID != GeneratorTokenType.Int) {
                        ThrowSyntaxError(token);
                    }

                    if (_currentDate != null && !_currentDate.Accept(token, previous)) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.CloseParen:
                    if (_currentDate != null) {
                        if (_currentEntry == 1) {
                            _start = _currentDate.DateTime;
                        } else {
                            _end = _currentDate.DateTime;
                        }
                    } else if (previous.TokenID != GeneratorTokenType.CloseParen) {
                        ThrowSyntaxError(token);
                    }

                    _currentDate = null;
                    break;
                case GeneratorTokenType.Int:
                    if (_currentDate?.Accept(token, previous) != true) {
                        ThrowSyntaxError(token);
                    }

                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (DateTimeOffset, DateTimeOffset) Finish()
        {
            return (_start, _end);
        }

        #endregion

        
    }

    /// <summary>
    /// A generator that randomly generates a date between two given dates
    /// </summary>
    public sealed class DateGenerator : IDataGenerator
    {
        #region Variables

        private readonly DateTimeOffset _max;

        private readonly DateTimeOffset _min;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Randomly generates a date between the given min and max";

        /// <inheritdoc />
        public string Signature { get; } = "{{date(min: Date = new Date(1970, 1, 1, 0, 0, 0), max: Date = new Date())}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="parameters">The parameters of the date function call</param>
        public DateGenerator(string parameters)
        {
            var parser = new DateTimeGeneratorParser(parameters);
            (DateTimeOffset min, DateTimeOffset max) result = parser.Parse();
            _min = result.min;
            _max = result.max;
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public DateGenerator() : this("()")
        {
            
        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            var nextTicks = ThreadSafeRandom.NextInt64(_min.Ticks, _max.Ticks);
            return Task.FromResult<object>(new DateTimeOffset(nextTicks, TimeSpan.Zero));
        }

        #endregion
    }
}