/*
 *    Copyright 2004-2006 Intel Corporation
 *    Copyright 2011 Mitre Corporation
 *    Copyright 2011 Trinity College Dublin
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


#ifndef __ODBC_MYSQL_STORE_H__
#define __ODBC_MYSQL_STORE_H__

#ifndef OASYS_CONFIG_STATE
#error "MUST INCLUDE oasys-config.h before including this file"
#endif

#if LIBODBC_ENABLED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <map>

// includes for standard ODBC headers typically in /usr/include
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <sqlucode.h>

#include "../debug/Logger.h"

#include "StorageConfig.h"
#include "ODBCStore.h"

#define DATA_MAX_SIZE (1 * 1024 * 1024) //1M - increase this and table create for larger buffers


namespace oasys
{


/**
 * Interface for the generic datastore
 */
class ODBCDBMySQL: public ODBCDBStore
{

  public:
      ODBCDBMySQL (const char *logpath);

    // Can't copy or =, don't implement these
      ODBCDBMySQL & operator= (const ODBCDBMySQL &);
      ODBCDBMySQL (const ODBCDBMySQL &);

     ~ODBCDBMySQL ();

    //! @{ Virtual from DurableStoreImpl
    //! Initialize ODBCDBMySQL
    int init (const StorageConfig & cfg);

    //! Complete initialization (including addition of triggers)
    int create_finalize(const StorageConfig& cfg);

    //! Return string description
    std::string get_info () const;

    /// @}

  private:
    /**
     * Timer class used to periodically refresh the database connection .
     */
    class KeepAliveTimer:public oasys::Timer, public oasys::Logger
    {
        public:
            KeepAliveTimer (const char *logbase,
                    		ODBCDBMySQL * store,
                            int frequency):
                        	   Logger ("ODBCDBMySql::KeepAliveTimer",
                                       "%s/%s", logbase,
                                       "keep_alive_timer"),
                               store_(store),
                               frequency_ (frequency*60000) { }

            void reschedule ();
            void timeout (const struct timeval &now);

        protected:
            ODBCDBMySQL * store_;
            int frequency_;
    };

    KeepAliveTimer *keep_alive_timer_;

    // Execute non-intrusive database action to keep database
    // connection open
    void do_keepalive_transaction(void);


  //private:

    //! Parser for odbc.ini files - identifies DSN corresponding to dsn_name
    //  and returns selected items from the DSN as output parameters.
    int parse_odbc_ini_MySQL(const char *dsn_name, char *database_name,
    						 char *user_name, char *password);
    std::string schema_creation_command_;	///< Holds command to be run at end of initialisation.

};


}; // namespace oasys

#endif // LIBODBC_ENABLED

#endif //__ODBC_MYSQL_STORE_H__
