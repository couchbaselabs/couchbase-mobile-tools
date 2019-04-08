using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace LargeDatasetGenerator.Generator
{
    public static class DataGenerator
    {
        #region Constants

        public static readonly IReadOnlyDictionary<string, Type> TypeMap;

        #endregion

        #region Constructors

        /// <summary>
        /// Static Constructor
        /// </summary>
        static DataGenerator()
        {
            TypeMap = PluginLoader.GeneratorTypes.ToDictionary(x => x.Name.Replace("Generator", "").ToLowerInvariant(),
                v => v);
        }

        #endregion

        #region Public Methods

        /// <summary>
        /// Gets the generator registered for the given identifier, if it exists.  A new object
        /// is created for every call.
        /// </summary>
        /// <param name="identifier">The identifier of the registered generator</param>
        /// <param name="input">The input to create it with (not always used)</param>
        /// <returns>The instantiated generator object</returns>
        /// <exception cref="KeyNotFoundException">Thrown if the given key has no registration</exception>
        public static IDataGenerator Generator(string identifier, string input)
        {
            if (!TypeMap.ContainsKey(identifier)) {
                throw new KeyNotFoundException($"Generator not found for {identifier}");
            }

            var type = TypeMap[identifier];
            if (type.GetConstructor(new[] {typeof(string)}) != null) {
                return Activator.CreateInstance(type, input) as IDataGenerator;
            }

            return Activator.CreateInstance(type) as IDataGenerator;
        }

        #endregion
    }
}