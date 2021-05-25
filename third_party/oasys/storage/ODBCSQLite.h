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


#ifndef __ODBC_SQLITE_STORE_H__
#define __ODBC_SQLITE_STORE_H__

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


namespace oasys
{


/**
 * Interface for the generic datastore
 */
class ODBCDBSQLite: public ODBCDBStore
{

  public:
      ODBCDBSQLite (const char *logpath);

    // Can't copy or =, don't implement these
      ODBCDBSQLite & operator= (const ODBCDBSQLite &);
      ODBCDBSQLite (const ODBCDBSQLite &);

     ~ODBCDBSQLite ();

    //! @{ Virtual from DurableStoreImpl
    //! Initialize ODBCDBSQLite
    int init (const StorageConfig& cfg);

    //! Complete initialization (including addition of triggers)
    int create_finalize(const StorageConfig& cfg);

    //! Return string description
    std::string get_info () const;
    /// @}

  private:

    //! Parser for odbc.ini files - identifies DSN corresponding to dsn_name
    //  and returns selected items from the DSN as output parameters.
    int parse_odbc_ini_SQLite(const char *dsn_name, char *full_path);
    std::string schema_creation_command_;	///< Holds command to be run at end of initialisation.
};


};                              // namespace oasys

#endif // LIBODBC_ENABLED

#endif //__ODBC_SQLITE_STORE_H__
