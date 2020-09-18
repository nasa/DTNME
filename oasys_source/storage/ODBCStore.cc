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

//##########################################################
//########## README on configuration/setup #################
//##########################################################
//
//  This module is a vanilla ODBC implementation (cloned from BerkeleyDBStore.cc)
//  as a DB accessor within OASYS.  This module contains common code which should
//  be usable with any ODBC driver manager and most ODBC database drivers.
//  The initial version has been tested with SQLite 3 (version 3.3.x) and
//  MySQL (version 5.1.x - and also Version 5.0.70).
//
//  The main class in this module (ODBCDBStore) acts as a base class for database
//  specific classes that are instantiated according to the database engine that
//  will be specified in the configuration file of the DTNME daemon that is started.
//  By convention the storage types to be specified in the 'storage set type' commmand
//  will be odbc-<xxx> (e.g., odbc-sqlite, odbc-mysql).  If new database engines are
//  supported,  it is necessary to write a derived class such as
//     class ODBCDBFooSQLnew: public ODBCDBStore { ... }
//  with new files ODBCFooSQLnew.h and ODBCFooSQLnew.h
//  and put a new section into DurableStore.cc to allow the new storage type
//  (odbc-foosqlnew) to be used in programs.  The extra code in DurableStore.cc
//  looks something like:
//      #include "ODBCFooSQLnew.h"
//      ....
//      else if (config.type_ == "odbc-foosqlnew")
//      {
//          impl_ = new ODBCDBFooSQLnew(logpath_);
//          log_debug("impl_ set to new ODBCDBFooSQLnew");
//      }
//      ....
//
//  The derived classes typically need to implement a single constructor and
//  destructor, plus the init function.  Two large chunks of common code that
//  are typically used in setting up the connection and creating the database
//  tables are encapsulated in the functions 'connect_to_database' and
//  'create_tables'.  It is possible that certain database drivers may have
//  peculiarities that require special purpose code that may have to be
//  provided by overriding the functions in this module, but the two initial
//  implementations work with the generic code.
//
//  In order to use this software and an ODBC-interfaced database, the platform
//  must include an ODBC driver manager.  The software has been initially tested
//  the ODBC driver manager unixODBC (http://www.unixodbc.org/) on Linux. In
//  principle it should also be possible to use an alternative ODBC driver manager
//  (such as iODBC - http://www.iodbc.org/).  The ODBC driver manager should support
//  at least version 3.51 of ODBC.  For ODBC reference manual see MSDN (previously
//  Microsoft Developer Network) web site - (as of November 2011) at
//      http://msdn.microsoft.com/en-us/library/ms710252%28v=VS.85%29.aspx
//
//  The code has been tested with:
//     unixODBC  versions 2.2.11 (Ubuntu 10.04 LTS and Red Hat) and 2.3.0 (Gentoo)
//  It requires the development headers to be installed to provide:
//     sql.h, sqlext.h, sqltypes.h, and sqlucode.h
//  and the shared library
//     libodbc.so
//
//  It may be helpful to install the tool and library 'odbcinst' which comes as a
//  separate package.  This is particularly useful for determining where the odbc
//  initialization files are located (the location has apparently altered between
//  versions 2.2.11 (system odbc.ini and odbc.inst in /etc) and version 2.3.0 (in
//  /etc/unixODBC).  The relevant command is:
//     odbcinst -v -j
//
//  Additionally there are some GUI tools available to assist with
//  configuring unixODBC. These can be downloaded from
//     http://unixodbc-gui-qt.svn.sourceforge.net/
//  This appears not to be available as a precompiled package for
//  Debian/Ubuntu (and does not compile entirely cleanly as of
//  November 2011.)
//
//  See the relevant derived classes for documentation of the required libraries
//  and the setting up of the odbc.ini and odbcinst.ini files for the particular
//  database engine.  Note that root access will normally be required to set up the
//  system initialization files.
//  WARNING: This version of the code does not currently inspect the user ODBC
//  initialization file (default ~/.odbc.ini).  This may be fixed in a later
//  version.
//
//  A significant part of the database specific code is likely to be the treatment
//  of options and properties in the odbc.ini DSN section that are unique to the
//  particular database.
//
//  When using ODBC the interpretation of the storage parameters is slightly altered.
//  The 'dbname' storage parameter is used to identify the Data Source Name (DSN)
//  configured into the odbc.ini file.  For file based storage, the name and location
//  of the database storage will typically be defined as one of the the properties of
//  the specified DSN rather than being associated with the 'dbdir' storage parameter.
//  For client-server implementations, the name of the database in the server will
//  typically be defined as one of the properties of the specified DSN.
//  However the 'dbdir' parameter is still needed because the startup/shutdown system
//  can, on request, store a 'clean shutdown' indicator by creating the file .ds_clean
//  in the directory specified in 'dbdir'.   Hence 'dbdir' is still (just) relevant.
//
//  The following storage configuration items are particularly relevant -others are
//  interpreted as for other types of storage:
//     storage set type odbc-<engine type> # Selects the storage type as usual
//     storage set dbname <DSN> # Selects a Data Source Name (DSN) in odbc.ini
//     storage set dbdir <full directory path name> # Used only to store the .ds_clean file.
//     storage set payloaddir <full directory path name> # Bundle payload files location
//                                                       # used as for other storage types
//
//  TODO: Fix up test cases.
//  Required OASYS test files:
//     sqlite-db-test.cc
//
//  Modified OASYS test files:
//     Makefile:  add "sqlite-db-test" to TESTS list
//
//  NOTE: should disable or not use configuration tidy/prune or any dynamic DB creation
//  due to data loss!!
//  
//##########################################################


#ifdef HAVE_CONFIG_H
#include <oasys-config.h>
#endif

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include <debug/DebugUtils.h>
#include <io/FileUtils.h>

#include <util/StringBuffer.h>
#include <util/Pointers.h>
#include <util/ScratchBuffer.h>

#include <serialize/MarshalSerialize.h>
#include <serialize/TypeShims.h>

#include "ODBCStore.h"
#include "StorageConfig.h"
#include "StoreDetail.h"
#include "util/InitSequencer.h"

#if LIBODBC_ENABLED

#define NO_TX  0                // for easily going back and changing TX id's later

namespace oasys
{

// Map from StoreDetail column types to ODBC C types.
static SQLSMALLINT odbc_col_c_type_map[] = {
		/* DK_CHAR	= 0, */	SQL_C_CHAR,			///< Character fields
		/* DK_SHORT, */		SQL_C_SSHORT,		///< Signed 16 bit integer fields
		/* DK_USHORT, */	SQL_C_USHORT,		///< Unsigned 16 bit integer fields
		/* DK_LONG, */		SQL_C_SLONG,		///< Signed 32 bit integer fields
		/* DK_ULONG, */		SQL_C_ULONG,		///< Unsigned 32 bit integer fields
		/* DK_LONG_LONG, */	SQL_C_SBIGINT,		///< Signed 64 bit integer fields
		/* DK_ULONG_LONG, */SQL_C_UBIGINT,		///< Unsigned 64 bit integer fields
		/* DK_FLOAT, */		SQL_C_FLOAT,		///< Single precision floating point fields
		/* DK_DOUBLE, */	SQL_C_DOUBLE,		///< Double precision floating point fields
		/* DK_DATETIME, */	SQL_C_TYPE_DATE,	///< Date/Time fields
		/* DK_VARCHAR, */	SQL_C_BINARY,		///< Variable width character fields
		/* DK_BLOB */		SQL_C_BINARY		///< Opaque data fields
};
// Map from StoreDetail column types to ODBC SQL column types.
static SQLSMALLINT odbc_col_sql_type_map[] = {
		/* DK_CHAR	= 0, */	SQL_CHAR,			///< Character fields
		/* DK_SHORT, */		SQL_SMALLINT,		///< Signed 16 bit integer fields
		/* DK_USHORT, */	SQL_SMALLINT,		///< Unsigned 16 bit integer fields
		/* DK_LONG, */		SQL_INTEGER,		///< Signed 32 bit integer fields
		/* DK_ULONG, */		SQL_INTEGER,		///< Unsigned 32 bit integer fields
		/* DK_LONG_LONG, */	SQL_BIGINT,			///< Signed 64 bit integer fields
		/* DK_ULONG_LONG, */SQL_BIGINT,			///< Unsigned 64 bit integer fields
		/* DK_FLOAT, */		SQL_FLOAT,			///< Single precision floating point fields
		/* DK_DOUBLE, */	SQL_DOUBLE,			///< Double precision floating point fields
		/* DK_DATETIME, */	SQL_TYPE_DATE,		///< Date/Time fields
		/* DK_VARCHAR, */	SQL_VARCHAR,		///< Variable width character fields
		/* DK_BLOB */		SQL_LONGVARBINARY	///< Opaque data fields
};
#if 0
// Map from StoreDetail column types to SQL column specifications.
// At present we don't create the auxiliary table programmatically so these mappings aren't used.
static const char * odbc_col_sql_spec_map[] = {
		/* DK_CHAR	= 0, */	"CHAR",				///< Character fields
		/* DK_SHORT, */		"SMALLINT",			///< Signed 16 bit integer fields
		/* DK_USHORT, */	"SMALLINT UNSIGNED",///< Unsigned 16 bit integer fields
		/* DK_LONG, */		"INTEGER",			///< Signed 32 bit integer fields
		/* DK_ULONG, */		"INTEGER UNSIGNED",	///< Unsigned 32 bit integer fields
		/* DK_LONG_LONG, */	"BIGINT",			///< Signed 64 bit integer fields
		/* DK_ULONG_LONG, */"BIGINT UNSIGNED",	///< Unsigned 64 bit integer fields
		/* DK_FLOAT, */		"FLOAT",			///< Single precision floating point fields
		/* DK_DOUBLE, */	"DOUBLE",			///< Double precision floating point fields
		/* DK_DATETIME, */	"DATE",				///< Date/Time fields
		/* DK_VARCHAR, */	"VARCHAR",			///< Variable width character fields
		/* DK_BLOB */		"BLOB"				///< Opaque data fields
};
#endif

/******************************************************************************
 *
 * ODBCDBStore
 *
 *****************************************************************************/
const
std::string	ODBCDBStore::META_TABLE_NAME("META_DATA_TABLES");
const
std::string ODBCDBStore::ODBC_INI_FILE_NAME("odbc.ini");

//----------------------------------------------------------------------------
ODBCDBStore::ODBCDBStore(const char* derived_classname, const char *logpath):
DurableStoreImpl(derived_classname, logpath),
init_(false),
auto_commit_(true),
serialize_all_(true)
{
    logpath_appendf("/ODBCDBStore/%s", derived_classname);
    log_debug("constructor enter/exit.");
}

//----------------------------------------------------------------------------
ODBCDBStore::~ODBCDBStore()
{
    log_debug("ODBCDBStore destructor enter.");
    StringBuffer err_str;

    err_str.append("Tables still open at deletion time: ");
    bool busy = false;

    for (RefCountMap::iterator iter = ref_count_.begin();
         iter != ref_count_.end(); ++iter)
    {
        if (iter->second != 0)
        {
            err_str.appendf("%s ", iter->first.c_str());
            busy = true;
        }
    }

    if (busy)
    {
        log_err("%s", err_str.c_str());
    }
    end_transaction(NULL, true);

    SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.trans_hstmt);
    SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.hstmt);
    SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.idle_hstmt);
    SQLDisconnect(dbenv_.m_hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);

    //dbenv_ = 0;
    log_info("db closed");
    log_debug("ODBCDBStore destructor exit.");
}

//----------------------------------------------------------------------------
// BEWARE: To start a transaction, different databases support various forms of
// command:
//   MySQL:		START TRANSACTION or BEGIN but not BEGIN TRANSACTION
//   SQLite:	BEGIN or BEGIN TRANSACTION but not START TRANSACTION
// So this routine uses BEGIN that seems to be supported by most.
int
ODBCDBStore::begin_transaction(void **txid)
{
	if (auto_commit_) {
		log_debug("begin_transaction -- AUTOCOMMIT is ON, returning");
		return DS_OK;
	}

    SQLRETURN ret;

    ScopeLockIf sl(&serialization_lock_, "Access by begin_transaction()", serialize_all_);

    log_debug("ODBCDBStore::begin_transaction enter.");

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     * Probably overkill as trans_hstmt doesn't use parameters or columns.
     */
    ret = SQLFreeStmt(dbenv_.trans_hstmt, SQL_CLOSE);
    if (!SQL_SUCCEEDED(ret))
    {
        log_crit("ERROR: begin_transaction - failed Statement Handle SQL_CLOSE");
        return(DS_ERR);
    }

    ret = SQLFreeStmt(dbenv_.trans_hstmt, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(ret))
    {
        log_crit("ERROR: begin_transaction - failed Statement Handle SQL_RESET_PARAMS");
        return(DS_ERR);
    }

    ret =
        SQLExecDirect(dbenv_.trans_hstmt,
                      (SQLCHAR *) "BEGIN",
                      SQL_NTS);
    if (!SQL_SUCCEEDED(ret) && (ret != SQL_NO_DATA))
    {
        log_crit("ERROR: begin_transaction - SQLExecDirect for 'BEGIN' failed - ret %d", ret);
        return (DS_ERR);
    }

    log_debug("ODBCDBStore::begin_transaction exit.");
    if ( txid!=NULL ) {
    	log_debug("begin_transaction: setting txid.");
        *txid = (void*) dbenv_.m_hdbc;
    }
    return DS_OK;
}

//----------------------------------------------------------------------------
// NOTE: This routine initially used SQLEndTran but that did not work with SQLite
// for reasons that are not clear - probably because of the NoTxn issue discussed
// in ODBCMySQL.cc.
int
ODBCDBStore::end_transaction(void *txid, bool be_durable)
{
    (void) txid;

	if (auto_commit_) {
		log_debug("end_transaction %p -- AUTOCOMMIT is ON, returning", txid);
		return DS_OK;
	}

    ScopeLockIf sl(&serialization_lock_, "Access by end_transaction()", serialize_all_);

    log_debug("ODBCDBStore::end_transaction %p enter.", txid);

    if (be_durable)
    {
        SQLRETURN ret;

        log_debug("end_transaction enter durable section");

        if (dbenv_.m_hdbc == NULL) {
        	log_debug("end_transaction called with dbenv_.m_hdbc NULL - skipping SQLEndTran.");
        	return(DS_ERR);
        }

        /*!
         * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
         * Probably overkill as trans_hstmt doesn't use parameters or columns.
         */
        ret = SQLFreeStmt(dbenv_.trans_hstmt, SQL_CLOSE);
        if (!SQL_SUCCEEDED(ret))
        {
            log_crit("ERROR: end_transaction - failed Statement Handle SQL_CLOSE");
            return(DS_ERR);
        }

        ret = SQLFreeStmt(dbenv_.trans_hstmt, SQL_RESET_PARAMS);
        if (!SQL_SUCCEEDED(ret))
        {
            log_crit("ERROR: end_transaction - failed Statement Handle SQL_RESET_PARAMS");
            return(DS_ERR);
        }
        ret =
            SQLExecDirect(dbenv_.trans_hstmt,
                          (SQLCHAR *) "COMMIT",
                          SQL_NTS);

        if (!SQL_SUCCEEDED(ret) && (ret != SQL_NO_DATA))
        {
            log_crit("ERROR: end_transaction: SQLExecDirect for COMMIT failed -ret %d", ret);
            return(DS_ERR);
        }
    }

    return DS_OK;
}

//----------------------------------------------------------------------------
void *
ODBCDBStore::get_underlying()
{
    log_debug("get_underlying enter.");
    return ((void *) &dbenv_);
}

//----------------------------------------------------------------------------
int
ODBCDBStore::get_table(DurableTableImpl ** table,
                         const std::string & name,
                         int flags, PrototypeVector & prototypes)
{
    log_debug("get_table entry.");
    (void) prototypes;

    u_int32_t db_flags;

    int key_size = (flags >> KEY_LEN_SHIFT) & KEY_LEN_MASK;

    ASSERT(init_);

    db_flags = 0;

    if (flags & DS_CREATE)
    {
        log_debug("get_table DS_CREATE on");

        if (flags & DS_EXCL)
        {
            log_debug("get_table DS_EXCL on");
        }
    }

    log_debug("get_table check if table %s exists in schema", name.c_str());
    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "SELECT count(*) FROM %s", name.c_str());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     * Probably overkill as trans_hstmt doesn't use parameters or columns.
     */
    sqlRC = SQLFreeStmt(dbenv_.hstmt, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sqlRC))
    {
        log_crit("ERROR:  get_table - failed Statement Handle SQL_CLOSE"); 
    }

    sqlRC = SQLFreeStmt(dbenv_.hstmt, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sqlRC))
    {
        log_crit("ERROR:  get_table - failed Statement Handle SQL_RESET_PARAMS");
    }

    sqlRC = SQLExecDirect(dbenv_.hstmt, (SQLCHAR *) my_SQL_str, SQL_NTS);
    if (!SQL_SUCCEEDED(sqlRC))
    {
    	if (flags &DS_AUX_TABLE) {
    		PANIC("Should not be trying to create aux table %s.", name.c_str());
    	}
        if (flags & DS_CREATE)
        {
            log_info("Creation of table %s in progress.", name.c_str());
           if (key_size == 0)
            {
            	snprintf(my_SQL_str, 500,
                     "CREATE TABLE %s(the_key VARBINARY(%d) PRIMARY KEY, the_data BLOB(1000000))",
                     name.c_str(),
                     KEY_VARBINARY_MAX);
            	log_debug
                    ("get_table DS_CREATE is ON and key_size is 0: CREATE VARBINARY key and Blob data");
            } else {
            	log_debug
                    ("get_table DS_CREATE is ON and key_size is %d: CREATE BINARY key and Blob data",
                     key_size);
                snprintf(my_SQL_str, 500,
                     "CREATE TABLE %s(the_key BINARY(%d) PRIMARY KEY, the_data BLOB(1000000))",
                     name.c_str(),
                     key_size);
            }
           log_debug("SQL: %s", my_SQL_str);
            sqlRC =
                SQLExecDirect(dbenv_.hstmt, (SQLCHAR *) my_SQL_str, SQL_NTS);
            if (!(SQL_SUCCEEDED(sqlRC) || (sqlRC == SQL_NO_DATA)))
            {
                log_crit("ERROR:  get_table - failed CREATE table %s - ret %d", name.c_str(), sqlRC);
                return  DS_ERR;
            }
            snprintf(my_SQL_str, 500, "INSERT INTO %s values('%s')",
                     META_TABLE_NAME.c_str(), name.c_str());
            sqlRC =
                SQLExecDirect(dbenv_.hstmt, (SQLCHAR *) my_SQL_str, SQL_NTS);
            if (!SQL_SUCCEEDED(sqlRC))
            {
                log_crit("ERROR:  get_table - failed table %s insert into META_DATA", name.c_str());
                return DS_ERR;
            }
        } else {
            log_crit
                ("get_table Table %s is missing and creation was not requested.", name.c_str());
            return DS_NOTFOUND;
        }
    } else {
        log_debug
            ("get_table - successful Select count(*) on table");
        if (flags & DS_CREATE)
        {
            if (flags & DS_EXCL)
            {
                log_debug
                    ("get_table DS_CREATE and DS_EXCL are ON and TABLE ALREADY EXISTS so return DS_EXISTS");
                return DS_EXISTS;       // added to match  if (err == EEXIST)
            }
        }
    }

    log_debug("get_table -- opened table %s", name.c_str());

    *table =
        new ODBCDBTable(logpath_, this, name, (flags & DS_MULTITYPE),
                          &dbenv_, key_size, (flags & DS_AUX_TABLE));

    log_debug("get_table exit.");
    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBStore::del_table(const std::string & name)
{
    log_debug("del_table (%s) enter.", name.c_str());
    char sqlstr[500];

    ASSERT(init_);

    if (ref_count_[name] != 0)
    {
        log_info
            ("del_table - Trying to delete table %s with %d refs still on it",
             name.c_str(), ref_count_[name]);

        return DS_BUSY;
    }

    log_info("del_table DROPPING table %s", name.c_str());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     * Probably overkill as trans_hstmt doesn't use parameters or columns.
     */
    sqlRC = SQLFreeStmt(dbenv_.hstmt, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sqlRC))
    {
        log_crit("ERROR:  del_table - failed Statement Handle SQL_CLOSE");
    }

    sqlRC = SQLFreeStmt(dbenv_.hstmt, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sqlRC))
    {
        log_crit("ERROR:  del_table - failed Statement Handle SQL_RESET_PARAMS");
    }

    snprintf(sqlstr, 500, "DROP TABLE %s", name.c_str());
    sqlRC = SQLExecDirect(dbenv_.hstmt, (SQLCHAR *) sqlstr, SQL_NTS);
    if (sqlRC == SQL_NO_DATA_FOUND)
    {
        return DS_NOTFOUND;
    } else if (!SQL_SUCCEEDED(sqlRC))
    {
        log_crit("ERROR:  del_table - unable to SQLExecDirect 'DROP TABLE %s'", name.c_str());
        return DS_ERR;
    }

    ref_count_.erase(name);
    log_debug("del_table exit.");

    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBStore::get_table_names(StringVector * names)
{
    log_debug
        ("get_table_names enter - list from META_DATA table).");
    SQLRETURN sql_ret;
    char the_table_name[128];

    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "SELECT the_table FROM %s",
             META_TABLE_NAME.c_str());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(dbenv_.hstmt, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  get_table_names - failed Statement Handle SQL_CLOSE");
    }

    sql_ret = SQLFreeStmt(dbenv_.hstmt, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  get_table_names - failed Statement Handle SQL_RESET_PARAMS");
    }

    sql_ret = SQLPrepare(dbenv_.hstmt, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get_table_names SQLPrepare error %d", sql_ret);
        return DS_ERR;
    }

    sql_ret =
        SQLBindCol(dbenv_.hstmt, 1, SQL_C_DEFAULT, the_table_name, 128, NULL);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get_table_names SQLBindCol error %d", sql_ret);
        return DS_ERR;
    }

    sql_ret = SQLExecute(dbenv_.hstmt);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("get_table_names SQLExecute NO_DATA_FOUND");
        return DS_NOTFOUND;
    }
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_debug("get_table_names SQLExecute error %d", sql_ret);
        return DS_ERR;
    }

    names->clear();
    while ((sql_ret = SQLFetch(dbenv_.hstmt)) == SQL_SUCCESS)
    {
        log_debug("get_table_names fetched table <%s>",
                  the_table_name);
        names->push_back(std::string(the_table_name, strlen(the_table_name)));
    }
    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug
            ("get_table_names SQLFetch NO_DATA_FOUND so done");
    } else if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get SQLFetch error %d", sql_ret);
        return DS_ERR;
    }

    log_debug("get_table_names exit.");
    return DS_OK;
}

//----------------------------------------------------------------------------
std::string ODBCDBStore::get_info()const
{
    StringBuffer
        desc;

    return "ODBCDB";
}

//----------------------------------------------------------------------------
bool
ODBCDBStore::aux_tables_available()
{
	return aux_tables_available_;
}
//----------------------------------------------------------------------------
int
ODBCDBStore::acquire_table(const std::string & table)
{
    log_debug("acquire_table enter.");
    ASSERT(init_);

    ++ref_count_[table];
    ASSERT(ref_count_[table] >= 0);

    log_debug("acquire_table table %s, +refcount=%d",
              table.c_str(), ref_count_[table]);

    log_debug("acquire_table exit.");
    return ref_count_[table];
}

//----------------------------------------------------------------------------
int
ODBCDBStore::release_table(const std::string & table)
{
    log_debug("release_table (%s) enter.", table.c_str());
    ASSERT(init_);

    --ref_count_[table];
    ASSERT(ref_count_[table] >= 0);

    log_debug("release_table table %s, -refcount=%d",
              table.c_str(), ref_count_[table]);

    log_debug("release_table exit.");
    return ref_count_[table];
}

//----------------------------------------------------------------------------
// Common pieces of initialization code.
// Factored out of specific driver classes.
DurableStoreResult_t
ODBCDBStore::connect_to_database(const StorageConfig & cfg)
{

    dbenv_.m_henv = SQL_NULL_HENV;
    dbenv_.m_hdbc = SQL_NULL_HDBC;

    // Allocate the ODBC environment handle for SQL
    if ((sqlRC =
         SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE,
                        &(dbenv_.m_henv))) != SQL_SUCCESS
        && sqlRC != SQL_SUCCESS_WITH_INFO)
    {
        log_crit
            ("connect_to_database: ERROR: Failed to allocate environment handle");
        return DS_ERR;
    }
    // set ODBC environment: ODBC version
    if ((sqlRC =
         SQLSetEnvAttr(dbenv_.m_henv, SQL_ATTR_ODBC_VERSION,
                       (void *) SQL_OV_ODBC3, 0)) != SQL_SUCCESS
        && sqlRC != SQL_SUCCESS_WITH_INFO)
    {
        log_crit("connect_to_database: ERROR: Failed to set OBDC Version for data server");
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }
    log_debug("init - Set ODBC Version success");

    // allocate the ODBC Connection handle
    if ((sqlRC =
         SQLAllocHandle(SQL_HANDLE_DBC, dbenv_.m_henv,
                        &(dbenv_.m_hdbc))) != SQL_SUCCESS
        && sqlRC != SQL_SUCCESS_WITH_INFO)
    {
        log_crit
            ("connect_to_database: ERROR: Failed to allocate ODBC Connection handle");
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }

    if (dbenv_.m_hdbc == SQL_NULL_HDBC)
    {
        log_crit
            ("connect_to_database: ERROR: Allocated ODBC Connection Handle is null");
        return DS_ERR;
    }
    log_info
        ("connect_to_database: Allocated ODBC Connection Handle successfully");

    // set ODBC Connection attributes: login timeout
     if ((sqlRC =
          SQLSetConnectAttr(dbenv_.m_hdbc, SQL_ATTR_LOGIN_TIMEOUT,
                            (SQLPOINTER) 10,
                            SQL_IS_UINTEGER)) != SQL_SUCCESS
         && sqlRC != SQL_SUCCESS_WITH_INFO)
     {
         log_crit("connect_to_database: ERROR: Failed to set DB Connect timeout");
         SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
         SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
         return DS_ERR;
     }
     log_info("connect_to_database: Set DB Connection timeout success");


    sqlRC = SQLConnect(dbenv_.m_hdbc,
                       (SQLCHAR *) cfg.dbname_.c_str(),
                       SQL_NTS, NULL, SQL_NTS, NULL, SQL_NTS);
    if (!SQL_SUCCEEDED(sqlRC))
    {
        log_crit
            ("connect_to_database: Failed to SQLConnect to <%s> with NULL login/pswd",
             cfg.dbname_.c_str());
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }
    log_debug
        ("connect_to_database: Connect to DB <%s> success w/NULL login/pswd (per odbc.ini and odbcinst.ini)",
         cfg.dbname_.c_str());

    log_info
        ("connect_to_database: For ODBC, AutoCommit is always ON until suspended by 'BEGIN TRANSACTION'");

    dbenv_.hstmt = SQL_NULL_HSTMT;
    if ((sqlRC =
         SQLAllocHandle(SQL_HANDLE_STMT, dbenv_.m_hdbc,
                        &(dbenv_.hstmt))) != SQL_SUCCESS
        && (sqlRC != SQL_SUCCESS_WITH_INFO))
    {
        log_crit
            ("connect_to_database: ERROR: Failed to allocate Statement handle");
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }

    if (dbenv_.hstmt == SQL_NULL_HSTMT)
    {
        log_crit
            ("connect_to_database: ERROR: Statement handle is null so skip statement");
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }

    dbenv_.trans_hstmt = SQL_NULL_HSTMT;
    if ((sqlRC =
         SQLAllocHandle(SQL_HANDLE_STMT, dbenv_.m_hdbc,
                        &(dbenv_.trans_hstmt))) != SQL_SUCCESS
        && (sqlRC != SQL_SUCCESS_WITH_INFO))
    {
        log_crit
            ("connect_to_database: ERROR: Failed to allocate Trans Statement handle");
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }

    if (dbenv_.trans_hstmt == SQL_NULL_HSTMT)
    {
        log_crit
            ("connect_to_database: ERROR: Trans Statement handle is null so skip statement");
        //SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.hstmt);
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }
    dbenv_.idle_hstmt = SQL_NULL_HSTMT;
    if ((sqlRC =
         SQLAllocHandle(SQL_HANDLE_STMT, dbenv_.m_hdbc,
                        &(dbenv_.idle_hstmt))) != SQL_SUCCESS
        && (sqlRC != SQL_SUCCESS_WITH_INFO))
    {
        log_crit
            ("connect_to_database: ERROR: Failed to allocate idle Statement handle");
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }

    if (dbenv_.idle_hstmt == SQL_NULL_HSTMT)
    {
        log_crit
            ("connect_to_database: ERROR: Idle Statement handle is null so skip statement");
        //SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.hstmt);
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }
    log_debug("connect_to_database: ODBC Main statement and Trans statement handles successfully allocated.");
    return DS_OK;
}

//----------------------------------------------------------------------------
// Set ODBC Connection attributes to match requested configuration: auto-commit
// Configuration setting has been copied into auto_commit_ private variable
// in the init routine.
// SQL databases typically start new sessions in auto-commit mode (certainly true for
// MySQL and SQLite). If the user configures the use of transactions, which involves
// turning off auto-commit mode (storage set auto_commit false), then the ODBC connection
// has to have the mode set correctly.  Unfortunately there is a problem here when using
// SQLite.  If the database schema is being initialized, the mechanism potentially
// involves sourcing a couple of external files with SQL commands directly into the
// database rather than through the ODBC connection. Since both connections are trying
// to write to the database to create new tables and triggers, SQLite doesn't like this.
// (See http://www.sqlite.org/faq.html#q5.) It isn't a problem if auto-commit is on,
// because every command is a separate transaction.  As originally written, the init
// routine in this class intended to build the database as one transaction and commit
// it when finished.  In practice this doesn't really help with robustness as intended
// because the DROP TABLE and CREATE TABLE (and corresponding trigger commands) terminate
// transactions anyway.  The upshot of all this is that turning off auto-commit needs
// to be postponed until database schema initialization (if happening) has occurred.
// Calling this routine in create_finalize() is a good plan.
//
//
DurableStoreResult_t
ODBCDBStore::set_odbc_auto_commit_mode()
{
     // Configuration value is  in auto_commit_ by now
     if ( ! auto_commit_ )
     {
         if ((sqlRC =
               SQLSetConnectAttr(dbenv_.m_hdbc,
                                 SQL_ATTR_AUTOCOMMIT,
                                 SQL_AUTOCOMMIT_OFF,
                                 SQL_IS_UINTEGER)) != SQL_SUCCESS
              && sqlRC != SQL_SUCCESS_WITH_INFO)
          {
              log_crit("connect_to_database: ERROR: Failed to set DB auto-commit");
              SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
              SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
              return DS_ERR;
          }
          log_info("connect_to_database: ODBC auto-commit disabled successfully - now in transaction mode.");
     }
     return DS_OK;

}

//----------------------------------------------------------------------------
DurableStoreResult_t
ODBCDBStore::create_aux_tables()
{
    char sql_cmd[500];
    /** Execute the SQL to create any auxiliary tables needed
     *  Currently the only auxiliary table used is the META_DATA_TABLE
     *  which contains a list of the storage tables that are automatically
     *  created by calls of get_table if they are not present at startup.
     */
    log_debug("create_aux_tables enter");

    /* Execute the query to create META_DATA table */
    snprintf(sql_cmd, 500,
             "CREATE TABLE %s (the_table varchar(128))",
             META_TABLE_NAME.c_str());
    sqlRC = SQLExecDirect(dbenv_.hstmt, (SQLCHAR *)sql_cmd, SQL_NTS);
    // if ( !SQL_SUCCEEDED(sqlRC) )
    if (!SQL_SUCCEEDED(sqlRC) && sqlRC != SQL_NO_DATA)
    {
        log_crit
            ("create_tables: ERROR: unable to SQLExecDirect '%s' (%d)",
             sql_cmd, sqlRC);
        SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.trans_hstmt);
        SQLFreeHandle(SQL_HANDLE_STMT, dbenv_.hstmt);
        SQLDisconnect(dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbenv_.m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, dbenv_.m_henv);
        return DS_ERR;
    }
    log_debug("%s table successfully created.", META_TABLE_NAME.c_str());
    log_debug("create_aux_tables exit - table(s) created successfully.");
    return DS_OK;

}

/******************************************************************************
 *
 * ODBCDBTable
 *
 *****************************************************************************/
ODBCDBTable::ODBCDBTable(const char *logpath,
                             ODBCDBStore * store,
                             const std::string & table_name,
                             bool multitype, ODBC_dbenv * db,
                             int key_size,
                             bool is_aux_table):
DurableTableImpl(table_name, multitype),
Logger("ODBCDBTable", "%s/%s", logpath, table_name.c_str()),
db_(db),
store_(store),
key_size_(key_size),
is_aux_table_(is_aux_table)
{
    SQLRETURN sqlRC;

    log_debug("ODBCDBTable constructor for table %s", table_name.c_str());
    log_debug("logpath is: <%s>", logpath);
    store_->acquire_table(table_name);

    hstmt_ = SQL_NULL_HSTMT;
    if ((sqlRC =
         SQLAllocHandle(SQL_HANDLE_STMT, db_->m_hdbc,
                        &hstmt_)) != SQL_SUCCESS
        && (sqlRC != SQL_SUCCESS_WITH_INFO))
    {
        log_crit
            ("init ERROR: Failed to allocate Statement handle");
        SQLDisconnect(db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, db->m_henv);
    }

    if (hstmt_ == SQL_NULL_HSTMT)
    {
        log_crit
            ("init ERROR: Statement handle is null so skip statement");
        SQLDisconnect(db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, db->m_henv);
    }

    iterator_hstmt_ = SQL_NULL_HSTMT;
    if ((sqlRC = SQLAllocHandle(SQL_HANDLE_STMT, db->m_hdbc, &(iterator_hstmt_))) != SQL_SUCCESS
       && sqlRC != SQL_SUCCESS_WITH_INFO)
    {
        log_crit("init ERROR: Failed to allocate Iterator Statement handle");
        SQLDisconnect(db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, db->m_henv);
    }

    if (iterator_hstmt_ == SQL_NULL_HSTMT)
    {
        log_crit("init ERROR: Iterator Statement handle is null so skip statement");
        SQLDisconnect(db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, db->m_hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, db->m_henv);
    }
}

//----------------------------------------------------------------------------
ODBCDBTable::~ODBCDBTable()
{
    // Note: If we are to multithread access to the same table, this
    // will have potential concurrency problems, because close can
    // only happen if no other instance of Db is around.
    store_->release_table(name());

    // SQLFreeHandle(SQL_HANDLE_STMT, hstmt_);
    // SQLFreeHandle(SQL_HANDLE_STMT, iterator_hstmt_);

    log_debug("destructor");
}

//----------------------------------------------------------------------------
int
ODBCDBTable::get(const SerializableObject & key, SerializableObject * data)
{
    log_debug("get enter.");
    ASSERTF(!multitype_, "single-type get called for multi-type table");

    ScopeLockIf sl(&store_->serialization_lock_,
				   "Access by get()",
				   store_->serialize_all_);
    ScopeLock l(&lock_, "Access by get()");

    ScratchBuffer < u_char *, 256 > key_buf;
    SQLLEN key_buf_len = flatten(key, &key_buf);
    ASSERT(key_buf_len != 0);
    u_char * key_buf_ptr = key_buf.buf();

    // Variables used for auxiliary tables
	StoreDetail				*data_detail;		///< Information about data to be retrieved
	StoreDetail::iterator	iter;				///< Iterator used to go through columns
	int						col_cnt = 0;		///< Set to column count
	SQLLEN *				user_data_sizes;	///< Dynamically allocated array of places to put column data lengths

    SQLRETURN sql_ret;
    u_char *fetched_blob = NULL;

    log_debug("get  Table=%s: key length %d", name(), (int)key_buf_len);
    char my_SQL_str[500];
    if (is_aux_table()){
    	// Check that we have a vector of descriptors to work with
    	data_detail = dynamic_cast<StoreDetail *>(data);
    	ASSERT(data_detail != NULL);
    	char col_list[400];
    	*col_list = '\0';
    	for (iter = data_detail->begin();
    			iter != data_detail->end(); ++iter) {
    		// Add separator text for columns after the first
    		if (*col_list != '\0') {
        		strcat(col_list, ", ");
    		}
    		strcat(col_list, (*iter)->column_name());
    		col_cnt++;
    	}
    	snprintf(my_SQL_str, 500, "SELECT %s FROM %s WHERE the_key = ?",
    			 col_list, name());

    } else {
        snprintf(my_SQL_str, 500, "SELECT the_data FROM %s WHERE the_key = ?",
                 name());
    }
    log_debug("get SQL command is '%s'", my_SQL_str);

    /*!
     * To fully free up the hstmt_ it is necessary to both unbind any preexisting
     * bound output columns (SQL_CLOSE) and any preexisting bound parameters (SQL_RESET_PARAMETERS)
     * This needs two calls to SQLFreeStmt (nicer if you could combine the options..).
     * If this is not done and the last usage was a 'put' or 'get' with multiple
     * parameters you run the risk of seeing random data in the bound parameters
     * which have potentially recorded addresses which are no longer valid.  If
     * you get an unexpected 'SQL_NEED_DATA' return from SQLExecute this is a
     * possible (and *extremely* difficult to diagnose) problem.
     */
    sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);       //close from any prior use
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  get - failed Statement Handle SQL_CLOSE");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  get - failed Statement Handle SQL_RESET_PARAMS");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get SQLPrepare error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    // Bind the key parameter
 	log_debug("get bind table key");

    sql_ret =
         SQLBindParameter(hstmt_, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                          (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                          0, 0, key_buf_ptr, 0, &key_buf_len);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get SQLBindParameter error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    if (is_aux_table()){
    	int col_no = 1;
    	user_data_sizes = new SQLLEN[col_cnt];

    	for (iter = data_detail->begin();
    			iter != data_detail->end(); ++iter) {
            sql_ret =
                SQLBindCol(hstmt_, col_no,
                		   odbc_col_c_type_map[(*iter)->column_type()],
                		   (*iter)->data_ptr(),
                		   (*iter)->data_size(),
                           &user_data_sizes[col_no - 1]);

            if (!SQL_SUCCEEDED(sql_ret))
            {
                log_err("get SQLBindCol error %d at column %d", sql_ret, col_no);
                print_error(db_->m_henv, db_->m_hdbc, hstmt_);
                delete user_data_sizes;
                return DS_ERR;
            }
            col_no++;
    	}
    } else {
    	log_debug("get bind blob column");
    	// Allocate space for a big blob - the whole serialized data for the object
        if ((fetched_blob = (u_char *) malloc(DATA_MAX_SIZE)) == NULL)
        {
            log_err("get malloc(DATA_MAX_SIZE) error");
            return DS_ERR;
        }
        user_data_sizes = new SQLLEN[1];
        user_data_sizes[0] = 0;

        sql_ret =
            SQLBindCol(hstmt_, 1, SQL_C_BINARY, fetched_blob, DATA_MAX_SIZE,
                       &user_data_sizes[0]);

        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_err("get SQLBindCol error %d", sql_ret);
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            free(fetched_blob);
            fetched_blob = NULL;
            delete user_data_sizes;
            return DS_ERR;
        }
    }

    sql_ret = SQLExecute(hstmt_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("get SQLExecute NO_DATA_FOUND");
        if (fetched_blob != NULL) free(fetched_blob);
        fetched_blob = NULL;
        delete user_data_sizes;
        return DS_NOTFOUND;
    }
    switch(sql_ret) {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
        break;
    case SQL_NO_DATA:
        log_debug("get SQLExecute returns SQL_NO_DATA after check");
        break;
    case SQL_NEED_DATA:
    	log_debug("get SQLExecute returns SQL_NEED_DATA");
    	// Fall through
    default:
        log_debug("get SQLExecute error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        if (fetched_blob != NULL) free(fetched_blob);
        fetched_blob = NULL;
        delete user_data_sizes;
        return DS_ERR;
    }

    sql_ret = SQLFetch(hstmt_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("get SQLFetch NO_DATA_FOUND");
        if (fetched_blob != NULL) free(fetched_blob);
        fetched_blob = NULL;
        delete user_data_sizes;
        return DS_NOTFOUND;
    }
    if (!(SQL_SUCCEEDED(sql_ret) || (sql_ret == SQL_NEED_DATA)))
    {
        log_err("get SQLFetch error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        if (fetched_blob != NULL) free(fetched_blob);
        fetched_blob = NULL;
        delete user_data_sizes;
        return DS_ERR;
    }

    if (is_aux_table()){
    	// Iterate through columns recording actual lengths for variable length items
    	int col_no = 0;  // Start from 0 this time to ease array access
    	detail_kind_t col_type;

    	for (iter = data_detail->begin();
    			iter != data_detail->end(); ++iter, col_no++) {
    		if ( user_data_sizes[col_no] == SQL_NULL_DATA) {
    			(*iter)->set_data_size(StoreDetailItem::SDI_NULL);
    		} else {
    			col_type = (*iter)->column_type();
    			if ((col_type == DK_VARCHAR) || (col_type == DK_BLOB)) {
    				// Insert actual size in length
    				(*iter)->set_data_size(user_data_sizes[col_no]);
    			}
    		}
    	}

    } else {
        if (user_data_sizes[0] == SQL_NULL_DATA)
        {
            log_err("get SQLFetch SQL_NULL_DATA");
            if (fetched_blob != NULL) free(fetched_blob);
            fetched_blob = NULL;
            delete user_data_sizes;
            return DS_ERR;
        }

        log_debug("get first 8-bytes of DATA=%x08 plus size=%ld",
                  *((u_int32_t *) fetched_blob), user_data_sizes[0]);

        Unmarshal unmarshaller(Serialize::CONTEXT_LOCAL, fetched_blob, user_data_sizes[0]);

        if (unmarshaller.action(data) != 0)
        {
            log_err("get: error unserializing data object");
            if (fetched_blob != NULL) free(fetched_blob);
            fetched_blob = NULL;
            delete user_data_sizes;
            return DS_ERR;
        }

    }

    if (fetched_blob != NULL) free(fetched_blob);
    fetched_blob = NULL;
    delete user_data_sizes;
    log_debug("ODBCDBStore::get exit.");
    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBTable::get(const SerializableObject & key,
                   SerializableObject ** data,
                   TypeCollection::Allocator_t allocator)
{
    ASSERTF(multitype_, "multi-type get called for single-type table");
    ScopeLockIf sl(&store_->serialization_lock_,
				   "Access by get2()",
				   store_->serialize_all_);
    ScopeLock l(&lock_, "Access by get2()");

    log_debug("ODBCDBStore::get2 enter.");

    ScratchBuffer < u_char *, 256 > key_buf;
    SQLLEN key_buf_len = flatten(key, &key_buf);
    if (key_buf_len == 0)
    {
        log_err("get2 zero or too long key length");
        return DS_ERR;
    }
    u_char * key_buf_ptr = key_buf.buf();

    SQLRETURN sql_ret;
    u_char *fetched_blob = NULL;
    SQLLEN user_data_size;

    log_debug("get2  Table=%s", name());
    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "SELECT the_data FROM %s WHERE the_key = ?",
             name());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  ODBCDBStore::get2 - failed Statement Handle SQL_CLOSE");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  get2 - failed Statement Handle SQL_RESET_PARAMS");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get2 SQLPrepare error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    // Bind the key parameter
 	log_debug("get2 bind table key");
    sql_ret =
         SQLBindParameter(hstmt_, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                          (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                          0, 0, key_buf_ptr, 0, &key_buf_len);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get2 SQLBindParameter error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    if ((fetched_blob = (u_char *) malloc(DATA_MAX_SIZE)) == NULL)
    {
        log_err("get2 malloc(DATA_MAX_SIZE) error");
        return DS_ERR;
    }

    sql_ret =
        SQLBindCol(hstmt_, 1, SQL_C_BINARY, fetched_blob, DATA_MAX_SIZE,
                   &user_data_size);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get2 SQLBindCol error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_ERR;
    }

    sql_ret = SQLExecute(hstmt_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("get2 SQLExecute NO_DATA_FOUND");
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_NOTFOUND;
    }
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get2 SQLExecute error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_ERR;
    }

    sql_ret = SQLFetch(hstmt_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("get2 SQLFetch NO_DATA_FOUND");
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_NOTFOUND;
    }
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get2 SQLFetch error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_ERR;
    }

    if (user_data_size == SQL_NULL_DATA)
    {
        log_err("get2 SQLFetch SQL_NULL_DATA");
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_ERR;
    }


    log_debug
        ("get2 first 8-bytes of DATA=%x08 plus size=%d",
         *((u_int32_t *)fetched_blob), (int)user_data_size);

    u_char *bp = fetched_blob;
    size_t sz = user_data_size;

    TypeCollection::TypeCode_t typecode;
    size_t typecode_sz = MarshalSize::get_size(&typecode);

    Builder b;
    UIntShim type_shim(b);
    Unmarshal type_unmarshaller(Serialize::CONTEXT_LOCAL, fetched_blob, typecode_sz);

    if (type_unmarshaller.action(&type_shim) != 0)
    {
        log_err("ODBCDBStore::get2: error unserializing type code");
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_ERR;
    }

    typecode = type_shim.value();

    bp += typecode_sz;
    sz -= typecode_sz;

    int err = allocator(typecode, data);
    if (err != 0)
    {
        log_err("ODBCDBStore::get2: error in allocator");
        free(fetched_blob);
        fetched_blob = NULL;
        *data = NULL;
        return DS_ERR;
    }

    ASSERT(*data != NULL);

    Unmarshal unmarshaller(Serialize::CONTEXT_LOCAL, bp, sz);

    if (unmarshaller.action(*data) != 0)
    {
        log_err("ODBCDBStore::get2: error unserializing data object");
        delete *data;
        *data = NULL;
        free(fetched_blob);
        fetched_blob = NULL;
        return DS_ERR;
    }

    free(fetched_blob);
    fetched_blob = NULL;
    user_data_size = 0;
    log_debug("ODBCDBStore::get2 exit.");
    return DS_OK;
}

//----------------------------------------------------------------------------
int
ODBCDBTable::put(const SerializableObject & key,
                   TypeCollection::TypeCode_t typecode,
                   const SerializableObject * data,
                   int flags)
{
    ScopeLockIf sl(&store_->serialization_lock_,
				   "Access by put()",
				   store_->serialize_all_);
    ScopeLock l(&lock_, "Access by put()");

    log_debug("put thread(%08X) flags(%02X)",
              (unsigned int) pthread_self(),
              flags);

    // Variables used for auxiliary tables
	StoreDetail			   *data_detail;		///< Information about data to be retrieved
	StoreDetail::iterator	iter;				///< Iterator used to go through columns
	int						col_cnt = 0;		///< Set to column count

	// flatten and fill in the key
    ScratchBuffer < u_char *, 256 > key_buf;
    SQLLEN key_buf_len = flatten(key, &key_buf);
    u_char * key_buf_ptr = key_buf.buf();

    // Temporary buffer to store serialized data
    ScratchBuffer < u_char *, 1024 > data_buf;
    SQLLEN data_buf_len;
    u_char *full_buf;

    SQLRETURN sql_ret;
    char my_SQL_str[500];
    int insert_sqlcode;
    bool row_exists = false;
    bool create_new_row = false;

    log_debug("put checking if key size %d exists", (int)key_buf_len);
    int err = key_exists(key_buf_ptr, key_buf_len);
    if (err == DS_ERR)
    {
        log_debug("put return DS_ERR per key_exists");
        return DS_ERR;
    }
    row_exists = (err != DS_NOTFOUND);
    log_debug("row_exists: %s; err: %d", row_exists ? "true": "false", err);

    // Determine what to do next - depends on flags
    if ( row_exists ) {
    	if ( flags & DS_EXCL ) {
    		if (is_aux_table()) {
    			log_debug("put ignoring DS_EXCL flag for aux table");
    		} else {
    			log_debug("put attempting to update existing row when DS_EXCL set: aborting.");
    			return DS_EXISTS;
    		}
    	} else {
    		log_debug("put will update existing row as DS_EXCL not set.");
    	}
    }
    else
    {
    	if (is_aux_table()) {
    		log_crit("put attempting to write into non-existent row in auxiliary table %s",
    				name());
    		log_debug("Rows in auxiliary table should only be created by triggers.");
    		return DS_ERR;
    	}
    	if ( ! flags & DS_CREATE ) {
    		log_debug("put attempting to update a row that does not exist without DS_CREATE set. Aborting." );
    		return DS_NOTFOUND;
    	}
    	log_debug("put will create new row as DS_CREATE is set and row is not present.");
    	create_new_row = true;
    }

    // Check if need to insert a new row -
    if ( create_new_row ) {
        log_debug("put inserting the_data blob with NULL value into new table row");
        snprintf(my_SQL_str, 500, "INSERT INTO %s values(?, NULL)", name());

        /*!
         * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
         */
        sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);       //close for later reuse
        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_crit("ERROR:  put insert - failed Statement Handle SQL_CLOSE");
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        }

        sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_crit("ERROR:  put insert - failed Statement Handle SQL_RESET_PARAMS");
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        }

        sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_err("put insert SQLPrepare error %d", sql_ret);
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            return DS_ERR;
        }
        sql_ret =
                SQLBindParameter(hstmt_, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                                 (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                                 0, 0, key_buf_ptr, 0, &key_buf_len);

        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_err("put insert SQLBindParameter error %d", sql_ret);
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            return DS_ERR;
        }

        insert_sqlcode = 0;
        sql_ret = SQLExecute(hstmt_);

        if (!SQL_SUCCEEDED(sql_ret))
        {
        	//need to capture internal SQLCODE from SQLError for processing Duplicate PK
            log_debug("put insert SQLExecute error %d", sql_ret);
            insert_sqlcode = print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            return DS_ERR;
        } else {
            log_debug("put insert SQLExecute succeeded");
        }
    }

    if (!is_aux_table()){
        // figure out the size of the data
        MarshalSize sizer(Serialize::CONTEXT_LOCAL);
        if (sizer.action(data) != 0)
        {
            log_err("put error sizing data object");
            return DS_ERR;
        }
        size_t object_sz = sizer.size();

        // and the size of the type code (if multitype)
        size_t typecode_sz = 0;
        if (multitype_)
        {
            typecode_sz = MarshalSize::get_size(&typecode);
        }
        // XXX/demmer -- one little optimization would be to pass the
        // calculated size out to the caller (the generic DurableTable),
        // so we don't have to re-calculate it in the object cache code

        log_debug
            ("put serializing %zu byte object (plus %zu byte typecode)",
             object_sz, typecode_sz);

        data_buf_len = typecode_sz + object_sz;
        full_buf = data_buf.buf(data_buf_len);

        // if we're a multitype table, marshal the type code
        if (multitype_)
        {
            log_debug("marshaling type code");
            Marshal typemarshal(Serialize::CONTEXT_LOCAL, full_buf, typecode_sz);
            UIntShim type_shim(typecode);

            if (typemarshal.action(&type_shim) != 0)
            {
                log_err("put error serializing type code");
                return DS_ERR;
            }
        }

        Marshal m(Serialize::CONTEXT_LOCAL, full_buf + typecode_sz, object_sz);
        if (m.action(data) != 0)
        {
            log_err("put error serializing data object");
            return DS_ERR;
        }
    }

    // added - after insert, now do update of the_data blob (update is easier since PUT() supports both)
    log_debug("put update table row (real data this time)");
    if (is_aux_table()){
    	// Check that we have a vector of descriptors to work with
    	data_detail = dynamic_cast<StoreDetail *>(const_cast<SerializableObject *>(data));
    	ASSERT(data_detail != NULL);
    	char col_list[400];
    	*col_list = '\0';
    	for (iter = data_detail->begin();
    			iter != data_detail->end(); ++iter) {
    		// Add separator text for columns after the first
    		if (*col_list != '\0'){
        		strcat(col_list, " = ?, ");
    		}
    		strcat(col_list, (*iter)->column_name());
    		col_cnt++;
    	}
    	// The generated SQL statement has to have (col_cnt + 1) parameters bound to it
    	// This statement puts in the ' =  ?' for the last column.
    	snprintf(my_SQL_str, 500, "UPDATE %s SET %s = ? WHERE the_key = ?",
    			 name(), col_list);
    	log_debug("put: SQL for aux table is '%s'.", my_SQL_str);

    } else {
        snprintf(my_SQL_str, 500,
                 "UPDATE %s SET the_data = ? WHERE the_key = ?", name());
    }

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);       //close for later reuse
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  put update - failed Statement Handle SQL_CLOSE");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  put update - failed Statement Handle SQL_RESET_PARAMS");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("put update SQLPrepare error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

	if (is_aux_table()) {
		// For auxiliary tables bind the (col_cnt) parameters and the key to the SQL statement
    	SQLUSMALLINT col_no = 1;
    	detail_kind_t col_type;

    	for (iter = data_detail->begin();
    			iter != data_detail->end(); ++iter, col_no++) {
    		col_type = (*iter)->column_type();
    		log_debug("put: bind parameter %s c_type %d size %d",
    				  (*iter)->column_name(),
    				  odbc_col_c_type_map[col_type],
    				  *((*iter)->data_size_ptr()));
            sql_ret = SQLBindParameter(hstmt_, col_no, SQL_PARAM_INPUT,
									   odbc_col_c_type_map[col_type],
									   odbc_col_sql_type_map[col_type],
									   0,
									   0,
					         		   (*iter)->data_ptr(),
					         		   0,
					         		   (SQLLEN *)((*iter)->data_size_ptr()));
            if (!SQL_SUCCEEDED(sql_ret))
            {
                log_err
                    ("put update SQLBindParameter %s:  error %d",
                    		(*iter)->column_name(), sql_ret);
                print_error(db_->m_henv, db_->m_hdbc, hstmt_);
                return DS_ERR;
            }

    	}
        // Bind the key parameter
    	log_debug("put aux table key col_no %d", col_no);
        sql_ret =
            SQLBindParameter(hstmt_, col_no, SQL_PARAM_INPUT, SQL_C_BINARY,
                             (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                             0, 0, key_buf_ptr, 0, &key_buf_len);

        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_err("put update SQLBindParameter PK:  error %d",
                    sql_ret);
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            return DS_ERR;
        }


	} else {
    	// Non-auxiliary tables - just insert serialized data as a BLOB
        log_debug
            ("put update first 8-bytes of DATA=%x08 plus size=%d",
             *((u_int32_t* ) full_buf), (int)data_buf_len);
        sql_ret = SQLBindParameter(hstmt_, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
								   SQL_LONGVARBINARY,
                                   0, 0, full_buf, 0, &data_buf_len);
        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_err
                ("put update SQLBindParameter DATA  error %d",
                 sql_ret);
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            return DS_ERR;
        }
        // Bind the key parameter
    	log_debug("put standard table key");
        sql_ret =
            SQLBindParameter(hstmt_, 2, SQL_PARAM_INPUT, SQL_C_BINARY,
                             (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                             0, 0, key_buf_ptr, 0, &key_buf_len);

        if (!SQL_SUCCEEDED(sql_ret))
        {
            log_err("put update SQLBindParameter PK  error %d",
                    sql_ret);
            print_error(db_->m_henv, db_->m_hdbc, hstmt_);
            return DS_ERR;
        }

    }

    sql_ret = SQLExecute(hstmt_);

    switch (sql_ret)
    {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
        break;
    case SQL_NO_DATA:
        log_debug
            ("put update SQLExecute error %d (SQL_NO_DATA) - ignoring",
             sql_ret);
        break;
    default:
        log_err("put update: SQLExecute returned error code %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    log_debug("put exit thread(%08X)", (u_int32_t) pthread_self());
    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBTable::del(const SerializableObject & key)
{

    ScopeLockIf sl(&store_->serialization_lock_,
				   "Access by del()",
				   store_->serialize_all_);
    ScopeLock l(&lock_, "Access by del()");
    log_debug("del enter thread(%08X).",(u_int32_t) pthread_self());

    u_char key_buf[KEY_VARBINARY_MAX];
    SQLLEN key_buf_len;

    key_buf_len = flatten(key, key_buf, sizeof(key_buf));
    if (key_buf_len == 0)
    {
        log_err("del - zero or too long key length");
        return DS_ERR;
    }
    u_char * key_buf_ptr = key_buf;

    //check if PK row already exists
    int err = key_exists(key_buf_ptr, key_buf_len);
    if (err == DS_NOTFOUND)
    {
        log_debug("::del return NO_DATA_FOUND per key_exists");
        return DS_NOTFOUND;
    }

    SQLRETURN sql_ret;

    log_debug("del");
    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "DELETE FROM %s WHERE the_key = ?", name());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);       //close from any prior use
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  del - failed Statement Handle SQL_CLOSE");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  del - failed Statement Handle SQL_RESET_PARAMS");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("del SQLPrepare error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    // Bind the key parameter
 	log_debug("del bind table key");
    sql_ret =
         SQLBindParameter(hstmt_, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                          (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                          0, 0, key_buf, 0, &key_buf_len);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("del SQLBindParameter error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    sql_ret = SQLExecute(hstmt_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug
            ("del SQLExecute NO_DATA_FOUND (SQLite does not indicate NDF)");
        return DS_NOTFOUND;
    }
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_debug("del SQLExecute error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    log_debug("del exit thread (%08X)", (u_int32_t) pthread_self());
    return 0;
}

//----------------------------------------------------------------------------
size_t
ODBCDBTable::size() const
{
    ScopeLock l(&lock_, "Access by size()");
    log_debug("size enter thread(%08X)", (u_int32_t) pthread_self());
    SQLRETURN sql_ret;
    SQLINTEGER my_count;   //long int
    size_t ret;

    log_debug("size  Table=%s", name());
    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "SELECT count(*) FROM %s", name());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);        //close from any prior use
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  size - failed Statement Handle SQL_CLOSE");
    }

    sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  size - failed Statement Handle SQL_RESET_PARAMS");
    }

    sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("size SQLPrepare error %d", sql_ret);
        return DS_ERR;
    }

    sql_ret = SQLBindCol(hstmt_, 1, SQL_C_SLONG, &my_count, 0, NULL);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("size SQLBindCol error %d", sql_ret);
        return DS_ERR;
    }

    sql_ret = SQLExecute(hstmt_);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("size SQLExecute error %d", sql_ret);
        return DS_ERR;
    }

    sql_ret = SQLFetch(hstmt_);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("get SQLFetch error %d", sql_ret);
        return DS_ERR;
    }

    ret = (size_t) my_count;
    log_debug ("size exit thread(%08X) w/ret=%d my_count(long int)=%ld  my_count(int)=%d.",
               (u_int32_t) pthread_self(), (int) ret, (long int) my_count, (int) my_count);
    return ret;
}

//----------------------------------------------------------------------------
DurableIterator *
ODBCDBTable::itr()
{
    return new ODBCDBIterator(this);
}

//----------------------------------------------------------------------------
int
ODBCDBTable::key_exists(const void *key, size_t key_len)
{
    log_debug("key_exists enter thread(%08X)",(u_int32_t) pthread_self());

    SQLRETURN sql_ret;
    SQLINTEGER my_count;        //long int
    SQLLEN sql_key_len = key_len;

    log_debug("key_exists.");
    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "SELECT count(*) FROM %s WHERE the_key = ?",
             name());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(hstmt_, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  key_exists - failed Statement Handle SQL_CLOSE");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLFreeStmt(hstmt_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  key_exists - failed Statement Handle SQL_RESET_PARAMS");
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
    }

    sql_ret = SQLPrepare(hstmt_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("key_exists SQLPrepare error %d",
                sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }
    // Bind the key parameter
 	log_debug("key exists bind table key");
    sql_ret =
         SQLBindParameter(hstmt_, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                          (key_size_ == 0) ? SQL_VARBINARY : SQL_BINARY,
                          0, 0, const_cast < void *>(key), 0, &sql_key_len);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("key_exists SQLBindParameter error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    sql_ret = SQLBindCol(hstmt_, 1, SQL_C_SLONG, &my_count, 0, NULL);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("key_exists SQLBindCol error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    sql_ret = SQLExecute(hstmt_);

    switch ( sql_ret ) {
    case SQL_SUCCESS:
    case SQL_SUCCESS_WITH_INFO:
        break;
    case SQL_NO_DATA:
        log_debug("key_exists SQLExecute returns SQL_NO_DATA");
        break;
    default:
        log_err("key_exists SQLExecute error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    sql_ret = SQLFetch(hstmt_);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("key_exists SQLFetch error %d", sql_ret);
        print_error(db_->m_henv, db_->m_hdbc, hstmt_);
        return DS_ERR;
    }

    if (my_count == 0)
    {
        log_debug("key_exists return DS_NOTFOUND.");
        return DS_NOTFOUND;
    }

    log_debug("key_exists exit (return 0 for FOUND) thread(%08X)",
              (u_int32_t) pthread_self());
    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBTable::print_error(SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt)
{
    SQLCHAR buffer[SQL_MAX_MESSAGE_LENGTH + 1];
    SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
    SQLINTEGER sqlcode;
    SQLSMALLINT length;
    int i = 0;
    int first_sqlcode = 0;
    log_debug("print_error");

    while (SQLError(henv, hdbc, hstmt, sqlstate, &sqlcode, buffer,
                    SQL_MAX_MESSAGE_LENGTH + 1, &length) == SQL_SUCCESS)
    {
        i++;
        log_err("**** ODBCDBTable::print_error (lvl=%d) *****", i);
        log_err("         SQLSTATE: %s", sqlstate);
        log_err("Native Error Code: %d", (int) sqlcode);
        log_err("          Message: %s", buffer);
        if (i == 1)
        {
            first_sqlcode = (int) sqlcode;
        }
    };
    return first_sqlcode;
}
/******************************************************************************
 *
 * ODBCDBIterator
 *
 *****************************************************************************/
ODBCDBIterator::ODBCDBIterator(ODBCDBTable* t):
    Logger("ODBCDBIterator", "%s/iter", t->logpath()),
    cur_(0),
    valid_(false)
{
    for_table_ = t;

    logpath_appendf("ODBCDBIterator/%s", t->name());
    for_table_->iterator_lock_.lock("Iterator");
    if ( t->store_->serialize_all_ ) {
        t->store_->serialization_lock_.lock("Access by table iterator()");
    }

    log_debug("iterator constructor enter.");

    SQLRETURN sql_ret;
    cur_ = t->iterator_hstmt_;
    strcpy(cur_table_name_, t->name());

    //TODO Elwyn: Why do we need the data here?  Only interested in iterating over keys.
    log_debug("constructor SELECT the_key FROM %s (unqualified)", t->name());
    //("constructor SELECT the_key,the_data FROM %s (unqualified)", t->name());

    char my_SQL_str[500];
    snprintf(my_SQL_str, 500, "SELECT the_key FROM %s", t->name());
    //snprintf(my_SQL_str, 500, "SELECT the_key,the_data FROM %s", t->name());

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(cur_, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  ODBCDBIterator(%s)::constructor - failed Statement Handle SQL_CLOSE",
                 t->name());
        t->print_error(t->db_->m_henv, t->db_->m_hdbc, cur_);
    }

    sql_ret = SQLFreeStmt(cur_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  ODBCDBIterator(%s)::constructor - failed Statement Handle SQL_RESET_PARAMETERS",
                 t->name());
        t->print_error(t->db_->m_henv, t->db_->m_hdbc, cur_);
    }

    sql_ret = SQLPrepare(cur_, (SQLCHAR *) my_SQL_str, SQL_NTS);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("constructor SQLPrepare error %d", sql_ret);
        t->print_error(t->db_->m_henv, t->db_->m_hdbc, cur_);
        return;
    }

    sql_ret =
        SQLBindCol(cur_, 1, SQL_C_BINARY, &key_, KEY_VARBINARY_MAX, &key_len_);

    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("constructor SQLBindParameter error %d",
                sql_ret);
        t->print_error(t->db_->m_henv, t->db_->m_hdbc, cur_);
        return;
    }

    sql_ret = SQLExecute(cur_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("constructor SQLExecute NO_DATA_FOUND");
        return;
    }
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("constructor SQLExecute error %d", sql_ret);
        t->print_error(t->db_->m_henv, t->db_->m_hdbc, cur_);
        return;
    }

    valid_ = true;              //cursor is available

    log_debug("constructor exit; cursor (%p) is available.", cur_);
}

//----------------------------------------------------------------------------
ODBCDBIterator::~ODBCDBIterator()
{
    log_debug("destructor enter.");

    SQLRETURN sql_ret;
    valid_ = false;

    /*!
     * See the comment on first 'get' routine about needing both SQLFreeStmt calls.
     */
    sql_ret = SQLFreeStmt(cur_, SQL_CLOSE);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  destructor - failed Statement Handle SQL_CLOSE");
        for_table_->print_error(for_table_->db_->m_henv, for_table_->db_->m_hdbc, cur_);
    }

    sql_ret = SQLFreeStmt(cur_, SQL_RESET_PARAMS);
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_crit("ERROR:  destructor - failed Statement Handle SQL_RESET_PARAMS");
        for_table_->print_error(for_table_->db_->m_henv, for_table_->db_->m_hdbc, cur_);
    }

    if ( for_table_->store_->serialize_all_ ) {
        for_table_->store_->serialization_lock_.unlock();
    }

    for_table_->iterator_lock_.unlock();
    log_debug("destructor exit.");
}

//----------------------------------------------------------------------------
int
ODBCDBIterator::next()
{
    log_debug("next - enter cur_ is (%p) thread(%08X)",
              cur_, (u_int32_t) pthread_self());
    ASSERT(valid_);
    ASSERT(for_table_->iterator_lock_.is_locked_by_me());

    SQLRETURN sql_ret;
    memset(&key_, 0, sizeof(key_));
    key_len_ = 0;

    sql_ret = SQLFetch(cur_);

    if (sql_ret == SQL_NO_DATA_FOUND)
    {
        log_debug("next SQLFetch NO_DATA_FOUND");
        return DS_NOTFOUND;
    }
    if (!SQL_SUCCEEDED(sql_ret))
    {
        log_err("next SQLFetch error %d", sql_ret);
        valid_ = false;
        return DS_ERR;
    }
    key_[key_len_] = '\0';
    if (for_table_->get_key_size() == 0)
    {
    	log_debug("next key (length %d): %s", (int)key_len_, &key_[4]);
    } else {
    	// Assume only using 4 byte integers here
    	if (key_len_ == 4)
    	{
    		log_debug("next key (unsigned integer): %u", reverse(key_));
    	}
    	else
    	{
    		log_debug("next (raw) key (length %d): %s", (int)key_len_, key_);
    	}
    }

    log_debug("next - exit.");
    return 0;
}

//---------------------------------------------------------------------------t
int
ODBCDBIterator::get_key(SerializableObject * key)
{
    log_debug("get_key - enter.");
    ASSERT(key != NULL);
    ASSERT(for_table_->iterator_lock_.is_locked_by_me());
    log_debug("Unmarshal size: %d", (int)key_len_);
    oasys::Unmarshal un(oasys::Serialize::CONTEXT_LOCAL,
                        key_, key_len_);

    if (un.action(key) != 0)
    {
        log_err("get_key - error unmarshalling");
        return DS_ERR;
    }

    log_debug("get_key - exit.");
    return 0;
}

//----------------------------------------------------------------------------
u_int32_t
ODBCDBIterator::reverse(u_char* key_int)
{
	u_int32_t r;
	u_char *rc = (u_char *)&r;
	rc[0] = key_int[3];
	rc[1] = key_int[2];
	rc[2] = key_int[1];
	rc[3] = key_int[0];

	return r;

}

}                               // namespace oasys

#endif // LIBODBC_ENABLED
