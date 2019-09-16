#
# Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
"""Generate platform specific implementations
   of cross-platform error messages for Couchbase Lite.
"""

import json
import os
import sys

from android import AndroidFormatter
from csharp import CSharpFormatter
from java import JavaFormatter
from objc import ObjCFormatter


def read_errors(file_name):
    with open(file_name, "r") as json_file:
        return json.load(json_file)


def format_errors(error_defs, out_dir):
    formatters = [
        AndroidFormatter(out_dir),
        CSharpFormatter(out_dir),
        JavaFormatter(out_dir),
        ObjCFormatter(out_dir)]

    json_errors = read_errors(error_defs)
    for formatter in formatters:
        print("== Formatting for {0}".format(formatter.name))
        formatter.format(json_errors)


def usage():
    print("Usage: python gen_errors.py <errors.json> <output directory>")
    print("    errors.json - the error message definition file")
    print("    output directory - an existing, writable directory")
    exit(1)


def main():
    if len(sys.argv) != 3:
        usage()
    error_defs = sys.argv[1]
    if not os.path.exists(error_defs) or not os.path.isfile(error_defs) or (not os.access(error_defs, os.R_OK)):
        usage()
    out_dir = sys.argv[2]
    if not os.path.exists(out_dir) or os.path.isfile(out_dir) or (not os.access(out_dir, os.W_OK)):
        usage()

    format_errors(error_defs, out_dir)


if __name__ == "__main__":
    main()
