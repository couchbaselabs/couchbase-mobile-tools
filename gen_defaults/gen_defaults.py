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

import json
from os import makedirs, path
from typing import Callable

from defs import DefaultEntry, DefaultGenerator
from swift import SwiftDefaultGenerator
from java import JavaDefaultGenerator
from objc import ObjCDefaultGenerator
from c import CDefaultGenerator

generators: dict[str, Callable[[dict[str, DefaultEntry]], DefaultGenerator]] = {
    "java": lambda d: JavaDefaultGenerator(d),
    "objc": lambda d: ObjCDefaultGenerator(d),
    "swift": lambda d: SwiftDefaultGenerator(d),
    "c": lambda d: CDefaultGenerator(d)
}

def read_defaults() -> list[DefaultEntry]:
    default = []
    with open(path.join(path.dirname(__file__), "cbl-defaults.json"), "r") as fin:
        content = json.load(fin)
        for k in content:
            default.append(DefaultEntry(k, **content[k]))

    return default

def main():
    defaults = read_defaults()
    print("Found the following information:")
    for d in defaults:
        print(d)
        for c in d.constants:
            print(f"\t\t{c}")

    for g in generators:
        print(f"Starting {g}")
        generator = generators[g](defaults)
        generated = generator.generate()
        if not path.isdir(g):
            makedirs(g, True)
        
        for f in generated:
            print(f"\t...writing {f}")
            with open(path.join(g, f), "w") as fout:
                fout.write(generated[f])

        print("\t...done!")


if __name__ == "__main__":
    main()