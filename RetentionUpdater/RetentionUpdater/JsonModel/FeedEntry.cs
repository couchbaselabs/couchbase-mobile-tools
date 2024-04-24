using System.Diagnostics.CodeAnalysis;
using System.Text.Json.Serialization;

namespace RetentionUpdater.JsonModel
{
    [JsonSerializable(typeof(FeedEntry))]
    internal partial class FeedEntryContext : JsonSerializerContext;

    [SuppressMessage("ReSharper", "InconsistentNaming")]
    internal readonly record struct FeedEntry(string name, string feedType, IReadOnlyList<RetentionRule> retentionRules, bool retentionRulesEnabled = true);
}
