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

from datetime import timedelta
import json
from typing import Any, List, Dict

class ConstantType(object):
    BOOLEAN_TYPE_ID: str = "boolean"
    INT_TYPE_ID: str = "int"
    LONG_TYPE_ID: str = "long"
    TIMESPAN_TYPE_ID: str = "TimeSpan"
    UINT_TYPE_ID: str = "uint"
    SIZE_T_TYPE_ID: str = "size_t"
    USHORT_TYPE_ID: str = "ushort"

    def __init__(self, id: str, subset: str):
        self._id = id
        self.subset = subset            

    def __str__(self):
        return str(self.id)

    @property
    def id(self) -> str:
        return self._id

class ConstantValue(object):
    def __init__(self, type: str, scalar, unit: str = None):
        if type == ConstantType.TIMESPAN_TYPE_ID:
            if(unit == "seconds"):
                self._val = timedelta(seconds=scalar)
            else:
                raise Exception(f"unknown timespan unit type '{unit}'")
        else:
            self._val = scalar

        self._unit = unit
        self.type = type

    @property
    def val(self) -> Any:
        return self._val

    @property
    def unit(self) -> str:
        return self._unit

    def __str__(self):
        return str(self.val)

class Constant(object):
    # def __init__(self, name: str, value: Any, type: dict, description: str, 
    #     only_on: List[str] = [], references: str = None):
    def __init__(self, args):
        self._name = args['name']
        self._raw = args
        self._default_type = ConstantType(args['type']['id'], args['type']['subset'])
        if isinstance(args['value'], dict):
            self._default_value = ConstantValue(self._default_type.id, **args['value'])
        else:
            self._default_value = ConstantValue(self._default_type.id, args['value'])
        
        self._description = args['description']
        self._references = args['references'] if 'references' in args else None
        self._only_on = args['only_on'] if 'only_on' in args else []

    def __str__(self):
        s = f"name: {self.name}, type: {self._default_type}, value: {self._default_value}"
        if len(self.only_on) > 0:
            s += f" (only on {json.dumps(self.only_on)})"

        return s

    def type(self, platform: str) -> ConstantType:
        specific_key = f"type_{platform}"
        if specific_key in self._raw:
            t = self._raw[specific_key]
        else:
            t = self._raw['type']

        return ConstantType(t['id'], t['subset'])

    @property
    def name(self) -> str:
        return self._name

    def value(self, platform: str) -> ConstantValue:
        if isinstance(self._raw['value'], dict):
            return ConstantValue(self.type(platform).id, **self._raw['value'])
            
        return ConstantValue(self.type(platform).id, self._raw['value'])

    @property
    def description(self) -> str:
        return self._description

    @property
    def references(self) -> str:
        return self._references

    @property
    def only_on(self) -> List[str]:
        return self._only_on

class DefaultEntry(object):
    def __init__(self, name: str, constants, long_name: str, only_on: List[str] = []):
        self._constants = []
        self.name = name
        self.long_name = long_name
        self.only_on = only_on
        for c in constants:
            self.constants.append(Constant(c))

    def __str__(self):
        return f"({self.name}), constant count: {len(self.constants)}"

    @property
    def constants(self) -> List[Constant]:
        return self._constants

class DefaultGenerator(object):
    def generate(self, input: List[DefaultEntry]) -> Dict[str, str]:
        raise NotImplementedError()

def make_c_style_varname(prefix_name: str, var_name: str):
    return f"kCBLDefault{prefix_name}{var_name}"

if __name__ == "__main__":
    raise Exception("This script is not standalone, it is used with gen_defaults.py")