using System.Threading.Tasks;

namespace LargeDatasetGenerator.Generator
{
    /// <summary>
    /// A generator that will randomly generate true or false
    /// </summary>
    public sealed class BoolGenerator : IDataGenerator
    {
        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Randomly generates true or false values";

        /// <inheritdoc />
        public string Signature { get; } = "{{bool()}}";

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            return Task.FromResult<object>(ThreadSafeRandom.NextDouble() >= 0.5);
        }

        #endregion
    }
}