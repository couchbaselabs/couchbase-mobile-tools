using System;

using LargeDatasetGenerator.Generator;

using sly.lexer;

namespace LargeDatasetGenerator.Core.Generator
{
    internal sealed class DateObjectArgs
    {
        #region Variables

        private DateTimeComponent _currentComponent;

        #endregion

        #region Properties

        public DateTimeOffset DateTime { get; private set; }

        #endregion

        #region Constructors

        public DateObjectArgs(DateTimeOffset defaultValue)
        {
            DateTime = defaultValue;
        }

        #endregion

        #region Public Methods

        public bool Accept(Token<GeneratorTokenType> token, Token<GeneratorTokenType> previous)
        {
            if (token.TokenID == GeneratorTokenType.Int) {
                DateTime = SetDateTimeComponent(token.IntValue);
                return previous.TokenID == GeneratorTokenType.OpenParen || previous.TokenID == GeneratorTokenType.Comma;
            }

            if (token.TokenID == GeneratorTokenType.Comma) {
                _currentComponent++;
                return (previous.TokenID == GeneratorTokenType.Int || previous.TokenID == GeneratorTokenType.OpenParen)
                       && _currentComponent <= DateTimeComponent.Second;
            }

            if (token.TokenID == GeneratorTokenType.CloseParen) {
                return previous.TokenID == GeneratorTokenType.OpenParen || previous.TokenID == GeneratorTokenType.Int;
            }

            return false;
        }

        #endregion

        #region Private Methods

        private DateTimeOffset SetDateTimeComponent(int value)
        {
            switch (_currentComponent) {
                case DateTimeComponent.Year:
                    return new DateTimeOffset(value, DateTime.Month, DateTime.Day, DateTime.Hour, DateTime.Minute,
                        DateTime.Second,
                        DateTime.Offset);
                case DateTimeComponent.Month:
                    return new DateTimeOffset(DateTime.Year, value, DateTime.Day, DateTime.Hour, DateTime.Minute,
                        DateTime.Second,
                        DateTime.Offset);
                case DateTimeComponent.Day:
                    return new DateTimeOffset(DateTime.Year, DateTime.Month, value, DateTime.Hour, DateTime.Minute,
                        DateTime.Second,
                        DateTime.Offset);
                case DateTimeComponent.Hour:
                    return new DateTimeOffset(DateTime.Year, DateTime.Month, DateTime.Day, value, DateTime.Minute,
                        DateTime.Second,
                        DateTime.Offset);
                case DateTimeComponent.Minute:
                    return new DateTimeOffset(DateTime.Year, DateTime.Month, DateTime.Day, DateTime.Hour, value,
                        DateTime.Second,
                        DateTime.Offset);
                case DateTimeComponent.Second:
                    return new DateTimeOffset(DateTime.Year, DateTime.Month, DateTime.Day, DateTime.Hour,
                        DateTime.Minute, value,
                        DateTime.Offset);
                default:
                    throw new ArgumentException("Invalid date time received");
            }
        }

        #endregion

        private enum DateTimeComponent
        {
            Year,
            Month,
            Day,
            Hour,
            Minute,
            Second
        }
    }
}