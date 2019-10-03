#
# Copyright (c) 2019 Couchbase, Inc All rights reserved.
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
import os


class CSharpFormatter:
    """Formatter for C# symbol definitions
       Definitions are formatted as string constants"""
    name = "C#"

    msg_file_name = "CouchbaseLiteErrorMessage.cs"

    message_def = "        internal const string {0} = \"{1}\";\n"

    trailer = """    }
}

"""

    header = """//  CouchbaseLiteErrorMessage.cs
//
//  Copyright (c) 2019 Couchbase, Inc All rights reserved.
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
using System;
using System.Collections.Generic;
using System.Text;

namespace Couchbase.Lite
{
    internal static partial class CouchbaseLiteErrorMessage
    {
"""

    def __init__(self, out_dir):
        self.out_file = os.path.join(out_dir, CSharpFormatter.msg_file_name)

    def format(self, errors_json):
        with open(self.out_file, "w") as message_file:
            message_file.write(CSharpFormatter.header)
            for error in errors_json:
                message_file.write(CSharpFormatter.message_def.format(error["name"], error["message"]))
            message_file.write(CSharpFormatter.trailer)
