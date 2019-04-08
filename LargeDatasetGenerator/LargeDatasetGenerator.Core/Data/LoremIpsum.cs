using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using LargeDatasetGenerator.Generator;

namespace LargeDatasetGenerator.Data
{
    public enum LoremUnit
    {
        Word,
        Sentence,
        Paragraph
    }

    internal static class LoremIpsum
    {
        public static readonly IReadOnlyList<string> Words = new[]{"lorem", "ipsum", "dolor", "sit", "amet", "consectetuer",
            "adipiscing", "elit", "sed", "diam", "nonummy", "nibh", "euismod",
            "tincidunt", "ut", "laoreet", "dolore", "magna", "aliquam", "erat"};

        private const int MinWordCount = 10;
        private const int MaxWordCount = 25;
        private const int MinSentenceCount = 3;
        private const int MaxSentenceCount = 8;

        public static string Generate(int count, LoremUnit unit)
        {
            var result = new StringBuilder();
            var paragraphs = new List<Paragraph>();

            if (unit == LoremUnit.Word) {
                paragraphs.Add(new Paragraph(1, count));
            } else if (unit == LoremUnit.Sentence) {
                paragraphs.Add(new Paragraph(count, 0));
            } else {
                for (int i = 0; i < count; i++) {
                    var sc = ThreadSafeRandom.Next(MinSentenceCount, MaxSentenceCount);
                    paragraphs.Add(new Paragraph(sc, 0));
                }
            }

            var first = true;
            foreach (var p in paragraphs) {
                if (!first) {
                    result.AppendLine();
                }

                foreach (var sentence in p) {
                    if (!first) {
                        result.Append("  ");
                    }

                    first = false;
                    for (int i = 0; i < sentence.WordCount; i++) {
                        var nextWord = Words[ThreadSafeRandom.Next(0, Words.Count)];
                        if (i == 0) {
                            nextWord = nextWord.Substring(0, 1).ToUpperInvariant() + nextWord.Substring(1);
                        }

                        result.Append(nextWord);
                        if (i != sentence.WordCount - 1) {
                            result.Append(" ");
                        }
                    }

                    if (unit != LoremUnit.Word) {
                        result.Append(".");
                    }
                }
            }

            return result.ToString();
        }

        private sealed class Paragraph : IEnumerable<Sentence>
        {
            private readonly int _sentenceCount;
            private readonly int _wordCount;

            public Paragraph(int sentenceCount, int wordCount)
            {
                _sentenceCount = sentenceCount;
                _wordCount = wordCount;
            }

            private sealed class Enumerator : IEnumerator<Sentence>
            {
                private readonly int _length;
                private readonly int _wordCount;
                private int _current;

                public Enumerator(int length, int wordCount)
                {
                    _length = length;
                    _wordCount = wordCount;
                }
                
                public bool MoveNext()
                {
                    return ++_current <= _length;
                }

                public void Reset()
                {
                    throw new NotSupportedException();
                }

                public Sentence Current
                {
                    get
                    {
                        var wordCount = _wordCount == 0 ? ThreadSafeRandom.Next(MinWordCount, MaxWordCount) : _wordCount;
                        return new Sentence(wordCount);
                    }
                }

                object IEnumerator.Current => Current;

                public void Dispose()
                {
                    // No-op
                }
            }

            public IEnumerator<Sentence> GetEnumerator()
            {
                return new Enumerator(_sentenceCount, _wordCount);
            }

            IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
        }

        private sealed class Sentence
        {
            public int WordCount { get; }

            public Sentence(int wordCount)
            {
                WordCount = wordCount;
            }
        }
    }
}