using System;
using System.Threading.Tasks;

namespace LargeDatasetGenerator.Generator
{
    /// <summary>
    /// A generator that creates GUID objects and returns their string representation
    /// </summary>
    public sealed class GuidGenerator : IDataGenerator
    {
        #region Properties

        /// <inheritdoc />
        public string Description { get; } = "Creates a GUID object as a string";

        /// <inheritdoc />
        public string Signature { get; } = "{{guid()}}";

        #endregion

        #region IDataGenerator

        /// <inheritdoc />
        public Task<object> GenerateValueAsync()
        {
            return Task.FromResult<object>(Guid.NewGuid().ToString());
        }

        #endregion
    }
}