/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _MYSQL_SQL_IMPLEMENTATION_H_
#define _MYSQL_SQL_IMPLEMENTATION_H_

#include <mysql.h>
#include <oasys/debug/Log.h>
#include <oasys/serialize/SQLImplementation.h>

namespace dtn {

/**
 * Mysql based implementation of SQL database.
 */
class MysqlSQLImplementation : public oasys::SQLImplementation, public oasys::Logger {
public:
    MysqlSQLImplementation();

    ///@{
    /// Virtual functions inherited from SQLImplementation
    int connect(const char* dbname);
    int close();
    bool has_table(const char* tablename);
    int exec_query(const char* query);
    int num_tuples();
    const char* get_value(int tuple_no, int field_no);
    //   size_t get_value_length(int tuple_no, int field_no);
    
    const char* binary_datatype();
    const char* escape_string(const char* from);
    const u_char* escape_binary(const u_char* from, int from_length);
    const u_char* unescape_binary(const u_char* from);
  
    ///@}


private:
    MYSQL* db_;  		///< the db connection
    MYSQL_RES* query_result_;	/// result of the last query performed
};
} // namespace dtn

#endif /* _MYSQL_SQL_IMPLEMENTATION_H_ */
