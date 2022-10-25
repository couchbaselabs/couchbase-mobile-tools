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

from glob import glob
from inspect import getmembers, isclass
import json
from os import makedirs, path
from typing import Dict, List

from defs import DefaultEntry, DefaultGenerator

generators: Dict[str, DefaultGenerator] = { }

for f in glob("*.py"):
    if f == "gen_defaults.py" or f == "defs.py":
        continue

    module_name = f.replace(".py", "")
    module = __import__(module_name)
    classes = getmembers(module, predicate=isclass)
    plugin_data = next((c for c in classes if c[0] != "DefaultGenerator" and c[0].endswith("DefaultGenerator")), None)
    if plugin_data is None:
        continue

    print(f"Found {plugin_data[0]}")
    generators[module_name] = plugin_data[1]()



def read_defaults() -> List[DefaultEntry]:
    default = []
    with open(path.join(path.dirname(__file__), "cbl-defaults.json"), "r") as fin:
        content = json.load(fin)
        for k in content:
            default.append(DefaultEntry(k, **content[k]))

    return default

def main():
    if len(generators) == 0:
        print("No export logic found, exiting...")
        return

    defaults = read_defaults()
    print("Found the following information:")
    for d in defaults:
        print(d)
        for c in d.constants:
            print(f"\t\t{c}")

    for g in generators:
        print(f"Starting {g}")
        generator = generators[g]
        generated = generator.generate(defaults)
        if not path.isdir(g):
            makedirs(g, 0o777, True)
        
        for f in generated:
            print(f"\t...writing {f}")
            with open(path.join(g, f), "w") as fout:
                fout.write(generated[f])

        print("\t...done!")


if __name__ == "__main__":
    main()