using System;
using System.Threading.Tasks;

using LargeDatasetGenerator.Data;

using sly.lexer;

namespace LargeDatasetGenerator.Generator
{
    internal sealed class LoremGeneratorArgs : GeneratorArgumentParser<(int, LoremUnit)>
    {
        #region Variables

        private int? _count;
        private int _currentArgument;
        private LoremUnit? _unit;

        #endregion

        #region Constructors

        public LoremGeneratorArgs(string input) : base(input)
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
                        previous.TokenID != GeneratorTokenType.Identifier) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Comma:
                    _currentArgument++;
                    if (_currentArgument > 1) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.Int:
                    if (_currentArgument != 0 || previous.TokenID != GeneratorTokenType.OpenParen) {
                        ThrowSyntaxError(token);
                    }

                    _count = token.IntValue;
                    break;
                case GeneratorTokenType.Identifier:
                    if (_currentArgument != 1) {
                        ThrowSyntaxError(token);
                    }

                    if (!Enum.TryParse<LoremUnit>(token.StringWithoutQuotes, true, out var unit) &&
                        !Enum.TryParse<LoremUnit>(token.StringWithoutQuotes.TrimEnd('s'), true, out unit)) {
                        ThrowSyntaxError(token);
                    }

                    _unit = unit;
                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override (int, LoremUnit) Finish()
        {
            return (_count ?? 1, _unit ?? LoremUnit.Sentence);
        }

        #endregion
    }

    public sealed class LoremGenerator : IDataGenerator
    {
        #region Variables

        private readonly int _count;
        private readonly LoremUnit _unit;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Generates lorem ipsum text of the specified amount";

        /// <inheritdoc />
        public string Signature { get; } = "{{lorem(count: int32, unit: word(s)|sentence(s)|paragraph(s))}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="input">The arguments to the lorem function</param>
        public LoremGenerator(string input)
        {
            var parser = new LoremGeneratorArgs(input);
            (int count, LoremUnit unit) result = parser.Parse();
            _count = result.count;
            _unit = result.unit;
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public LoremGenerator() : this("(1, word)")
        {
        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            return Task.FromResult<object>(LoremIpsum.Generate(_count, _unit));
        }

        #endregion
    }
}