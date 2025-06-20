#pragma once

#include <cstdlib>
#include <list>
#include <string>
#include <vector>

#include "common/status.h"
#include "util/slice.h"

#ifndef __StarRocksMysql
#define __StarRocksMysql void
#endif

#ifndef __StarRocksMysqlRes
#define __StarRocksMysqlRes void
#endif

namespace starrocks {
struct MysqlScannerParam {
    std::string host;
    std::string port;
    std::string user;
    std::string passwd;
    std::string db;
    unsigned long client_flag{0};
    MysqlScannerParam() = default;
};

// Mysql Scanner for scan data from mysql
class MysqlScanner {
public:
    MysqlScanner(const MysqlScannerParam& param);
    ~MysqlScanner();

    Status open();
    Status query(const std::string& query);

    // query for STARROCKS
    Status query(const std::string& table, const std::vector<std::string>& fields,
                 const std::vector<std::string>& filters,
                 const std::unordered_map<std::string, std::vector<std::string>>& filters_in,
                 std::unordered_map<std::string, bool>& filters_null_in_set, int64_t limit,
                 const std::string& temporal_clause);
    Status get_next_row(char*** buf, unsigned long** lengths, bool* eos);

    int field_num() const { return _field_num; }

    Slice escape(const std::string& value);

private:
    Status _error_status(const std::string& prefix);
    std::string _escape_buffer;
    const MysqlScannerParam& _my_param;
    __StarRocksMysql* _my_conn;
    __StarRocksMysqlRes* _my_result;
    std::string _sql_str;
    bool _opened;
    int _field_num;
};

} // namespace starrocks
