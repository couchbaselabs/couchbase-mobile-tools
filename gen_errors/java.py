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


class JavaFormatter:
    """Formatter for pure Java properties
       Errors are formatted as a properties file to be read into a properties map"""
    name = "Java"

    msg_file_name = "errors.properties"
    message_def = "{0} = {1}\n"

    def __init__(self, out_dir):
        self.out_file = os.path.join(out_dir, JavaFormatter.msg_file_name)

    def format(self, errors):
        with open(self.out_file, "w") as message_file:
            for error in errors:
                message = error["message"]
                message = message.format("%1$s", "%2$s", "%3$s", "%4$s", "%5$s")
                message_file.write(JavaFormatter.message_def.format(error["name"], message))
