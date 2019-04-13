//
//  n1ql_parser.h
//  Tools
//
//  Created by Jens Alfke on 4/10/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>

// Entry point of the N1QL parser (implementation at the bottom of n1ql.leg)
fleece::MutableDict n1ql_parse(const std::string &input, int *errPos);
