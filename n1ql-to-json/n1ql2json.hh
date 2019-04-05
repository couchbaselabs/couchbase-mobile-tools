//
//  n1ql2json.hh
//  N1QL to JSON
//
//  Created by Jens Alfke on 4/4/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include <string>

namespace litecore { namespace n1ql {

    /** Converts N1QL 'SELECT' query to LiteCore JSON query.
        Throws exception on error. */
    std::string N1QL_to_JSON(const std::string &n1ql,
                             std::string &errorMessage);

} }
