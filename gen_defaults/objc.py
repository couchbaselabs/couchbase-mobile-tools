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
from defs import DefaultGenerator, DefaultEntry, Constant, ConstantValue, ConstantType, make_c_style_varname

top_level_format = """//
//  CBLDefaults.{extension}
//  CouchbaseLite
//
//  Copyright (c) {year}-present Couchbase, Inc All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

// THIS IS AN AUTOGENERATED FILE, MANUAL CHANGES SHOULD BE EXPECTED TO
// BE OVERWRITTEN

#import "{import_header}"

{generated}
"""

class ObjCDefaultGenerator(DefaultGenerator):
    _type_mapping: Dict[str, str] = {
        ConstantType.BOOLEAN_TYPE_ID: "BOOL",
        ConstantType.TIMESPAN_TYPE_ID: "NSTimeInterval",
        ConstantType.SIZE_T_TYPE_ID: "uint64_t",
        ConstantType.USHORT_TYPE_ID: "unsigned short",
        "ReplicatorType": "CBLReplicatorType"
    }

    def transform_var_value(self, type: ConstantType, value: ConstantValue) -> str:
        if type.subset == "enum":
            return f"kCBL{type.id}{value}"

        if type.id == ConstantType.BOOLEAN_TYPE_ID:
            return "YES" if bool(value.val) else "NO"

        if type.id == ConstantType.TIMESPAN_TYPE_ID:
            if value.unit == "seconds":
                return str(cast(timedelta, value.val).seconds)
            else:
                raise Exception(f"Unknown unit '{value.unit}'")

        if type.id == ConstantType.UINT_TYPE_ID:
            if value.val == -1:
                return "UINT_MAX"

        return str(value)

    def compute_header_line(self, prefix_name: str, constant: Constant) -> str:
        value = self.transform_var_value(constant.type, constant.value)
        ret_val = f"/** [{value}] {constant.description} */\n"
        type = self._type_mapping[constant.type.id] if constant.type.id in self._type_mapping else constant.type
        var_name = make_c_style_varname(prefix_name, constant.name)
        ret_val += f"extern const {type} {var_name};\n\n"
        return ret_val

    def compute_impl_line(self, prefix_name: str, constant: Constant) -> str:
        type = self._type_mapping[constant.type.id] if constant.type.id in self._type_mapping else constant.type
        value = self.transform_var_value(constant.type, constant.value)
        var_name = make_c_style_varname(prefix_name, constant.name)
        return f"const {type} {var_name} = {value};\n"

    def generate(self, input: List[DefaultEntry]) -> Dict[str, str]:
        generated: Dict[str, str] = {}
        generated_header = ""
        generated_impl = ""
        for entry in input:
            for c in entry.constants:
                if len(c.only_on) > 0 and not "objc" in c.only_on:
                    continue
                
                generated_header += self.compute_header_line(entry.name, c)
                generated_impl += self.compute_impl_line(entry.name, c)

        generated["CBLDefaults.h"] = top_level_format.format(year = datetime.now().year, extension = "h", generated = generated_header,
            import_header = "CBLReplicatorTypes.h")
        generated["CBLDefaults.m"] = top_level_format.format(year = datetime.now().year, extension = "m", generated = generated_impl,
            import_header = "CBLDefaults.h")
        return generated
                

if __name__ == "__main__":
    raise Exception("This script is not standalone, it is used with gen_defaults.py")