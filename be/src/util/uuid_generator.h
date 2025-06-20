#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <string>

namespace starrocks {

class ThreadLocalUUIDGenerator {
public:
    static boost::uuids::uuid next_uuid() { return s_tls_gen(); }

    static std::string next_uuid_string() { return boost::uuids::to_string(next_uuid()); }

private:
    static inline thread_local boost::uuids::basic_random_generator<boost::mt19937> s_tls_gen;
};

} // namespace starrocks
