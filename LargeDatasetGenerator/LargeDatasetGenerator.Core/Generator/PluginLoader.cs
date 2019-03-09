using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;

using LargeDatasetGenerator.Output;

namespace LargeDatasetGenerator.Generator
{
    internal static class PluginLoader
    {
        #region Constants

        public static readonly IReadOnlyList<Type> GeneratorTypes;
        public static readonly IReadOnlyList<Type> OutputTypes;

        #endregion

        #region Constructors

        static PluginLoader()
        {
            var generatorTypes = new List<Type>();
            var outputTypes = new List<Type>();
            var totalCount = 0;
            var invalidCount = 0;
            var assemblyDir = Path.GetDirectoryName(typeof(PluginLoader).Assembly.Location);
            var alreadyLoaded =
                new HashSet<string>(AppDomain.CurrentDomain.GetAssemblies().Select(x => x.GetName().Name));

            outputTypes.AddRange(LoadOutputTypes(Assembly.GetExecutingAssembly()));
            generatorTypes.AddRange(LoadGeneratorTypes(Assembly.GetExecutingAssembly()));
            foreach (var dll in Directory.EnumerateFiles(assemblyDir, "*.dll")
                .Where(x => !alreadyLoaded.Contains(Path.GetFileNameWithoutExtension(x)))) {
                try {
                    var assembly = Assembly.LoadFile(Path.GetFullPath(dll));
                    outputTypes.AddRange(LoadOutputTypes(assembly));
                    generatorTypes.AddRange(LoadGeneratorTypes(assembly));
                    totalCount++;
                } catch (BadImageFormatException) {
                    invalidCount++;
                } catch (Exception e) {
                    Console.ForegroundColor = ConsoleColor.DarkYellow;
                    PositionalConsole.WriteLine($"Skipping {dll} because it was not loadable: {e}");
                    Console.ResetColor();
                }
            }

            if (invalidCount > 0) {
                PositionalConsole.WriteLine($"Skipped {invalidCount} of {totalCount} assemblies because they were not valid .NET",
                    ConsoleColor.DarkYellow);
            } else {
                PositionalConsole.WriteLine($"Processed {totalCount} assemblies for plugin types");
            }

            GeneratorTypes = generatorTypes;
            OutputTypes = outputTypes;
        }

        #endregion

        #region Private Methods

        private static IEnumerable<Type> LoadOutputTypes(Assembly assembly)
        {
            return assembly.GetExportedTypes()
                .Select(x => x.GetTypeInfo())
                .Where(x => !x.IsAbstract && x.ImplementedInterfaces.Contains(typeof(IJsonOutput)));
        }

        private static IEnumerable<Type> LoadGeneratorTypes(Assembly assembly)
        {
            return assembly.GetExportedTypes()
                .Select(x => x.GetTypeInfo())
                .Where(x => !x.IsAbstract && x.ImplementedInterfaces.Contains(typeof(IDataGenerator)));
        }

        #endregion
    }
}