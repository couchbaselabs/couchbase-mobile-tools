using System;
using System.Collections.Generic;
using System.Threading.Tasks;

using LargeDatasetGenerator.Generator;

using sly.lexer;

namespace LargeDatasetGenerator.Core.Generator
{
    internal sealed class RandomGeneratorArgs : GeneratorArgumentParser<IReadOnlyList<object>>
    {
        #region Variables

        private readonly List<object> _choices = new List<object>();

        #endregion

        #region Constructors

        public RandomGeneratorArgs(string input) : base(input)
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
                    if (previous.TokenID == GeneratorTokenType.OpenParen) {
                        throw new InvalidOperationException("random() must have at least two items");
                    }

                    if (previous.TokenID == GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    break;
                case GeneratorTokenType.String:
                    if (previous.TokenID != GeneratorTokenType.OpenParen &&
                        previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    _choices.Add(token.StringWithoutQuotes.Trim('\''));
                    break;
                case GeneratorTokenType.Int:
                    if (previous.TokenID != GeneratorTokenType.OpenParen &&
                        previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    _choices.Add(token.IntValue);
                    break;
                case GeneratorTokenType.Double:
                    if (previous.TokenID != GeneratorTokenType.OpenParen &&
                        previous.TokenID != GeneratorTokenType.Comma) {
                        ThrowSyntaxError(token);
                    }

                    _choices.Add(token.DoubleValue);
                    break;
                default:
                    ThrowSyntaxError(token);
                    break;
            }
        }

        protected override IReadOnlyList<object> Finish()
        {
            return _choices;
        }

        #endregion
    }

    /// <summary>
    /// A generator that chooses a random item from a list of provided ones
    /// </summary>
    public sealed class RandomGenerator : IDataGenerator
    {
        #region Variables

        private readonly IReadOnlyList<object> _choices;

        #endregion

        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Choose a random value from a provided list of values";

        /// <inheritdoc />
        public string Signature { get; } = "{{random(<item>, <item>, ...)}}";

        #endregion

        #region Constructors

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="input">The raw arguments from the command line</param>
        public RandomGenerator(string input)
        {
            var parser = new RandomGeneratorArgs(input);
            _choices = parser.Parse();
        }

        /// <summary>
        /// Default constructor
        /// </summary>
        public RandomGenerator() : this("(\"foo\", \"bar\")")
        {

        }

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            var nextIndex = ThreadSafeRandom.Next(0, _choices.Count);
            return Task.FromResult(_choices[nextIndex]);
        }

        #endregion
    }
}