using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

using Microsoft.Extensions.Configuration;

namespace LogMergeTool
{
    public static class ConfigurationKeys
    {
        public static class SingleFile
        {
            public const string MaxSize = "single_file:max_size";
        }

        public static class Format
        {
            public const string Utc = "format:use_utc";
        }
    }
    

    public static class Advanced
    {
        public static readonly IConfiguration Configuration;

        public static bool AsBoolean(string key)
        {
            if (Configuration[key] == null) {
                return false;
            }

            return Boolean.TryParse(Configuration[key], out var retVal) && retVal;
        }

        static Advanced()
        {
            var config = new ConfigurationBuilder()
                .SetBasePath(Directory.GetCurrentDirectory())
                .AddIniFile("config.ini", true)
                .Build();

            Configuration = config;
        }
    }
}
