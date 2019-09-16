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
import json
import os


class AndroidFormatter:
    """Formatter for Android resources
       Android will read a map from a JSON document
       This formatter will handle strings with up to 5 arguments"""
    name = "Android"

    msg_file_name = "errors.json"

    def __init__(self, out_dir):
        self.out_file = os.path.join(out_dir, AndroidFormatter.msg_file_name)

    def format(self, errors):
        errors_dict = dict()
        for error in errors:
            message = error["message"]
            errors_dict[error["name"]] = message.format("%1$s", "%2$s", "%3$s", "%4$s", "%4$s")
        with open(self.out_file, "w") as message_file:
            message_file.write(json.dumps(errors))
