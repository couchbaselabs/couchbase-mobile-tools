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
from typing import Any

class ConstantType(object):
    BOOLEAN_TYPE_ID: str = "boolean"
    INT_TYPE_ID: str = "int"
    LONG_TYPE_ID: str = "long"
    TIMESPAN_TYPE_ID: str = "TimeSpan"
    UINT_TYPE_ID: str = "uint"

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

    @property
    def val(self) -> Any:
        return self._val

    @property
    def unit(self) -> str:
        return self._unit

    def __str__(self):
        return str(self.val)

class Constant(object):
    def __init__(self, name: str, value: Any, type: dict, description: str, override_short: str = "", only_on: list[str] = []):
        self.name = name
        self._type = ConstantType(**type)
        if isinstance(value, dict):
            self._value = ConstantValue(self.type.id, **value)
        else:
            self._value = ConstantValue(self.type.id, value)
        
        self.description = description
        self.override_short = override_short
        self.only_on = only_on

    def __str__(self):
        s = f"name: {self.name}, type: {self.type}, value: {self.value}"
        if len(self.only_on) > 0:
            s += f" (only on {json.dumps(self.only_on)})"

        return s

    @property
    def type(self) -> ConstantType:
        return self._type

    @property
    def value(self) -> ConstantValue:
        return self._value

class DefaultEntry(object):
    def __init__(self, name: str, constants):
        self._constants = []
        self.name = name
        for c in constants:
            self.constants.append(Constant(**c))

    def __str__(self):
        return f"({self.name}), constant count: {len(self.constants)}"

    @property
    def constants(self) -> list[Constant]:
        return self._constants

class DefaultGenerator(object):
    def __init__(self, input: list[DefaultEntry]):
        self._input = input

    def generate() -> dict[str, str]:
        raise NotImplementedError()

if __name__ == "__main__":
    raise Exception("This script is not standalone, it is used with gen_defaults.py")