using System.Diagnostics.CodeAnalysis;
using System.Text.Json.Serialization;

namespace RetentionUpdater.JsonModel
{
    [JsonSerializable(typeof(RetentionRule))]
    internal partial class RetentionRuleContext : JsonSerializerContext;

    [SuppressMessage("ReSharper", "InconsistentNaming")]
    internal readonly record struct RetentionRule(
        int? keepVersionsCount = null,
        IReadOnlyList<string>? keepVersions = null,
        IReadOnlyList<string>? deleteVersions = null
    );
}
