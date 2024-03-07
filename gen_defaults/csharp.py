#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# Copyright (c) 2022 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from datetime import datetime, timedelta
from typing import List, Dict, cast
from defs import DefaultGenerator, DefaultEntry, Constant, ConstantValue, ConstantType

OUTPUT_ID = "csharp"

top_level_format = """//
// Copyright (c) {year}-present Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// --------------------------------------------------
// <auto-generated>
// This file was generated by gen_defaults.py
// </auto-generated>
// --------------------------------------------------

using Couchbase.Lite.Sync;
using Couchbase.Lite.Query;
using Couchbase.Lite.Logging;
#if COUCHBASE_ENTERPRISE
using Couchbase.Lite.Enterprise.Query;
using Couchbase.Lite.P2P;
#endif

using System;

namespace Couchbase.Lite.Info;

/// <summary>
/// A class holding predefined constant default values for various classes<br /><br />
///
/// > [!INFO]
/// > This file was autogenerated by a Couchbase script
/// </summary>
public static class Constants {{

{generated}}}
"""

class CSharpDefaultGenerator(DefaultGenerator):
    _type_mapping: Dict[str, str] = {
        ConstantType.BOOLEAN_TYPE_ID: "bool",
        ConstantType.TIMESPAN_TYPE_ID: "TimeSpan",
        ConstantType.SIZE_T_TYPE_ID: "long"
    }

    def transform_var_value(self, type: ConstantType, value: ConstantValue) -> str:
        if type.subset == "enum":
            return f"{type.id}.{value.val}"

        if type.id == ConstantType.BOOLEAN_TYPE_ID:
            return str(value.val).lower()

        if type.id == ConstantType.TIMESPAN_TYPE_ID:
            if value.unit == "seconds":
                numeric = cast(timedelta, value.val).seconds
                return f"TimeSpan.FromSeconds({numeric})"
            else:
                raise Exception(f"Unknown unit '{value.unit}'")

        if type.id == ConstantType.UINT_TYPE_ID:
            if value.val == "max":
                return "UInt32.MaxValue"
            
        if type.id == ConstantType.INT_TYPE_ID:
            if value.val == "max":
                return "Int32.MaxValue"

        return str(value)

    def compute_value(self, entry: DefaultEntry, constant: Constant) -> str:
        platform_type = constant.type(OUTPUT_ID)
        platform_value = constant.value(OUTPUT_ID)
        references = constant.references if constant.references is not None else constant.name
        type = self._type_mapping[platform_type.id] if platform_type.id in self._type_mapping else platform_type
        value = self.transform_var_value(platform_type, platform_value)
        ret_val = f"\t/// <summary>\n\t/// Default value for " + \
            f"<see cref=\"{entry.long_name}.{references}\" /> ({value})\n" + \
            f"\t/// {constant.description}\n\t/// </summary>\n"
        
        ret_val += f"\tpublic static readonly {type} Default{entry.name}{constant.name} = {value};\n\n"
        return ret_val

    def generate(self, input: List[DefaultEntry]) -> Dict[str, str]:
        output: str = ""
        generated: Dict[str, str] = {}
        input = sorted(input, key=lambda x: x.ee)
        writing_ee = False
        for entry in input:
            if len(entry.only_on) > 0 and not OUTPUT_ID in entry.only_on:
                continue

            if not writing_ee and entry.ee:
                output += "#if COUCHBASE_ENTERPRISE\n\n"
                writing_ee = True

            for c in entry.constants:
                if len(c.only_on) > 0 and not OUTPUT_ID in c.only_on:
                    continue
                
                output += self.compute_value(entry, c)

        if writing_ee:
            output += "#endif\n"
        
        output = output.rstrip() + "\n"
        generated["Defaults.cs"] = top_level_format.format(year = datetime.now().year, generated = output)
        return generated

if __name__ == "__main__":
    raise Exception("This script is not standalone, it is used with gen_defaults.py")