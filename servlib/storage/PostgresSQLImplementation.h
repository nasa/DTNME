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

#ifndef _POSTGRES_SQL_IMPLEMENTATION_H_
#define _POSTGRES_SQL_IMPLEMENTATION_H_

#include <libpq-fe.h>
#include <oasys/debug/Log.h>
#include <oasys/serialize/SQLImplementation.h>

namespace dtn {

/**
 * Postgres based implementation of SQL database.
 */
class PostgresSQLImplementation : public oasys::SQLImplementation, public oasys::Logger {
public:
    PostgresSQLImplementation();
    
    ///@{
    /// Virtual functions inherited from SQLImplementation
    int connect(const char* dbname);
    int close();
    bool has_table(const char* tablename);
    int exec_query(const char* query);
    int num_tuples();
    const char* get_value(int tuple_no, int field_no);
//    size_t get_value_length(int tuple_no, int field_no);
    

    const char* escape_string(const char* from);
    const u_char* escape_binary(const u_char* from, int from_length);
    const u_char* unescape_binary(const u_char* from);
  
    ///@}

private:
    PGconn* db_;  		///< the db connection
    PGresult* query_result_;	/// result of the last query performed
};
} // namespace dtn

#endif /* _POSTGRES_SQL_IMPLEMENTATION_H_ */
