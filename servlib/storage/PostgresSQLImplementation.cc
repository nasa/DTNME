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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#if POSTGRES_ENABLED

#include <string.h>
#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringBuffer.h>
#include "PostgresSQLImplementation.h"

namespace dtn {

PostgresSQLImplementation::PostgresSQLImplementation()
    : SQLImplementation("BYTEA", "BOOLEAN"),
      Logger("/storage/postgresql")
{
    query_result_ = NULL;
}

int
PostgresSQLImplementation::connect(const char* dbName)
{
    char *pghost;
    char *pgport;
    char *pgoptions;
    char *pgtty;
 
    log_debug("connecting to database %s", dbName);

    /*
     * begin, by setting the parameters for a backend connection if
     * the parameters are null, then the system will try to use
     * reasonable defaults by looking up environment variables or,
     * failing that, using hardwired constants
     */

    pghost = NULL;   	/* host name of the backend server */
    pgport = NULL;   	/* port of the backend server */
    pgoptions = NULL;   /* special options to start up the backend
                         * server */
    pgtty = NULL;   	/* debugging tty for the backend server */
    
    /**
     *  make a connection to the database 
     *
     */
    
    db_ = PQsetdb(pghost, pgport, pgoptions, pgtty, dbName);
        
    /**
     * check to see that the backend connection was successfully made
     */
    if (PQstatus(db_) == CONNECTION_BAD)
    {
        log_err("connection to database '%s' failed: %s",
                dbName, PQerrorMessage(db_));
        return -1;
    }
    
    return 0;
}

int
PostgresSQLImplementation::close()
{
    PQfinish(db_);
    return 0;
}

/*
size_t
PostgresSQLImplementation::get_value_length(int tuple_no, int field_no)
{
    size_t retval;
    const char* val = get_value(tuple_no,field_no);
    unescape_binary((const u_char*)val,&retval);
    return retval;
}
*/

const char*
PostgresSQLImplementation::get_value(int tuple_no, int field_no)
{
    const char* ret ;
    ASSERT(query_result_);
    ret = PQgetvalue(query_result_, tuple_no, field_no);
    return ret;
}

bool
PostgresSQLImplementation::has_table(const char* tablename)
{
    bool retval = 0;
    oasys::StringBuffer query;
    
    query.appendf("select * from pg_tables where tablename = '%s'", tablename);
    int ret = exec_query(query.c_str());
    ASSERT(ret == 0);
    if (num_tuples() == 1) retval  = 1;

    return retval;
}

int
PostgresSQLImplementation::num_tuples()
{
    int ret = -1;
    ASSERT(query_result_);
    ret = PQntuples(query_result_);
    return ret;
}

static int
status_to_int(ExecStatusType t)
{
    if (t == PGRES_COMMAND_OK) return 0;
    if (t == PGRES_TUPLES_OK) return 0;
    if (t == PGRES_EMPTY_QUERY) return 0;
    return -1;
}

int
PostgresSQLImplementation::exec_query(const char* query)
{
    int ret = -1;

    if (query_result_ != NULL) {
        PQclear(query_result_);
        query_result_ = NULL;
    }
    
    query_result_ = PQexec(db_, query);
    ASSERT(query_result_);
    ExecStatusType t = PQresultStatus(query_result_);
    ret = status_to_int(t);
    
    return ret;
}

const char* 
PostgresSQLImplementation::escape_string(const char* from) 
{
    int length = strlen(from);
    char* to = (char *) malloc(2*length+1);
    PQescapeString (to,from,length);
    return to;
}

const u_char* 
PostgresSQLImplementation::escape_binary(const u_char* from, int from_length) 
{
    size_t to_length;
    u_char* from1 = (u_char *) from ; 
    u_char* to =  PQescapeBytea(from1,from_length,&to_length);
    return to;
}

const u_char* 
PostgresSQLImplementation::unescape_binary(const u_char* from) 
{
    u_char* from1 = (u_char *) from ; 
    size_t to_length ;
    const u_char* to = PQunescapeBytea(from1,&to_length);
    return to;
}

} // namespace dtn

#endif /* POSTGRES_ENABLED */
