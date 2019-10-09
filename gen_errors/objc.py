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
import os
from datetime import datetime


class ObjCFormatter:
    """Formatter for Obj-C symbol definitions"""
    name = "Obj-C"
    year = datetime.today().year

    m_msg_file_name = "CBLMessage.m"
    h_msg_file_name = "CBLMessage.h"

    m_message_def = "NSString* const kCBLMessage{0} = @\"{1}\";\n"
    h_message_def = "extern NSString* const kCBLMessage{0};\n"

    m_trailer = """
@end

"""

    h_trailer = """
@end

NS_ASSUME_NONNULL_END

"""

    m_header = """//
//  CBLMessage.m
//  CouchbaseLite
//
//  Copyright (c) {0} Couchbase, Inc All rights reserved.
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
 
#import "CBLMessage.h"
 
@implementation CBLMessage
 
""".format(year)

    h_header = """//
//  CBLMessage.h
//  CouchbaseLite
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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CBLMessage : NSObject

"""

    def __init__(self, out_dir):
        self.m_out_file = os.path.join(out_dir, ObjCFormatter.m_msg_file_name)
        self.h_out_file = os.path.join(out_dir, ObjCFormatter.h_msg_file_name)

    def format(self, errors):
        with open(self.m_out_file, "w") as message_file:
            message_file.write(ObjCFormatter.m_header)
            for error in errors:
                message = error["message"].format("%1$@", "%2$@", "%3$@", "%4$@", "%4$@")
                message_file.write(ObjCFormatter.m_message_def.format(error["name"], message))
            message_file.write(ObjCFormatter.m_trailer)

        with open(self.h_out_file, "w") as message_file:
            message_file.write(ObjCFormatter.h_header)
            for error in errors:
                message_file.write(ObjCFormatter.h_message_def.format(error["name"]))
            message_file.write(ObjCFormatter.h_trailer)
