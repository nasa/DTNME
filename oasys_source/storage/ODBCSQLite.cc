/*
 *    Copyright 2004-2006 Intel Corporation
 *    Copyright 2011 Mitre Corporation
 *    Copyright 2011 Trinity Collage Dublin
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
//  For main configuration information relating to ODBC access to an SQLite
//  database see ODBCStore.cc.
//
//  This module filters out the non-standard parts of ODBC operation.
//  The main class defined here (ODBCDBSQLite) inherits the standard ODBC code
//  from the ODBCDBStore class and provides extra initialization code for
//  locating the file used to store the SQLite database and, when required,
//  initializing tha directory where it is to be stored.  If the database file
//  doesn't exist when the connection is opened, the file is created (provided
//  the directory exists). Note that this differs from the client-server case
//  wherethe database has to be created and a user set up before the database
//  can be accessed by ODBC.
//
//
//  Required Software on Linux box to run:
//   unixODBC
//   SQLite
//   SQLite ODBC libs (libsqlite3odbc-0.83.so -
//                     download from http://www.ch-werner.de/sqliteodbc/)
//
//  Ubuntu packages are available for Release 10.04 LTS for all these components
//  although the versions are marginally different across releases and distributions:
//   unixODBC   ==> 2.2.11-21
//   SQLite     ==> 3.3.6
//   SQLiteODBC ==> 0.80
//
//  Required OASYS storage files:
//    ODBCStore.h
//    ODBCSQLite.h
//    ODBCStore.cc
//    ODBCSQLite.cc
//  NOTE: need to modify init() to also CREATE TABLE for all VRL router schema tables!!!
//    SQLite_DTN_and_VRL_schema.ddl
//  NOTE: need to add CREATE TABLE for all VRL router schema tables!!!
//
//  Modified OASYS storage files:
//    Insertions into DurableStore.cc to instantiate ODBCDBSQLite as the storage
//    implementation:
//     #include "ODBCSQLite.h"
//
//     ...
//  
//      else if (config.type_ == "odbc-sqlite")
//      {
//          impl_ = new ODBCDBSQLite(logpath_);
//          log_debug("impl_ set to new ODBCDBSQLite");
//      }
//
//  Required OASYS test files:
//     sqlite-db-test.cc
//
//  When using ODBC the interpretation of the storage parameters is slightly
//  altered.
//  - The 'type' is set to 'odbc-sqlite' for a SQLite database accessed via ODBC.
//  - The 'dbname' storage parameter is used to identify the Data Source Name (DSN)
//    configured into the odbc.ini file. In the case of SQLite, the full path name
//    of the database file to be used is defined in the DSN specification, along
//    with necessary parameters especially the connection timeout.
//  - The 'dbdir' parameter is still needed because the startup/shutdown system
//    can, on request, store a 'clean shutdown' indicator by creating the
//    file .ds_clean in the directory specified in 'dbdir'.   Hence 'dbdir' is
//    still (just) relevant.
//  - The storage payloaddir *is* still used to store the payload files which
//    are not held in the database.
//
//  Because there is a possibility that 'dbdir' is the same as the directory used
//  to store the SQLite database which is independently specified in the odbc.ini
//  file, the database 'tidy' operation is less straightforward than for some other
//  storage mechanisms.  The 'standard' process is to delete the 'dbdir' directory
//  and all its contents.  This gets rid of .ds_clean if it exists but it may or
//  may not remove the SQLite database file depending on whether the two independently
//  specified directories are the same or not.  The implementation choice made here
//  allows the user to decide whether tidy wipes the database file or just empties
//  the tables and leaves the file to be reused:
//    If the directories are the same:
//      Recursively delete the directory and all its contents.
//      Then recreate the directory, the database file and the tables.
//    Otherwise:
//      Recursively delete the directory 'dbdir' and all its contents.
//      Do not delete the database file or its directory,
//         thus leaving any other files intact.
//      Open the existing database and drop all the relevant tables.
//      Then recreate the tables.
//
//  WARNING:  In either case any data in the database will be lost.
//            In the first case any other (possibly unrelated) files  in the
//            'dbdir' directory will be unconditionally destroyed.
//            A warning and time delay is given to allow the user to change
//            his/her mind but this may not be visible if the log is
//            redirected to a file.
//
//  Two optional ODBC specific parameters may be defined (default empty string):
//  - odbc_schema_pre_creation:		string with full path name of file of SQL
//                              	commands run before main tables are created.
//                              	Typically used to create auxiliary tables.
//  - odbc_schema_post_creation: 	string with full path name of file of SQL
//                              	commands run before main tables are created.
//									Typically used to create triggers once all
//									tables are in place.
//
//  Modifications to system ODBC configuration files (requires root access):
//    $ODBCSYSINI/odbcinst.ini:  add new section if MySQL ODBC lib *.so
//    is not already extant.  The default for ODBCSYSINI seems to have
//    altered  between version 2.2.x and version 2.3.x of unixODBC.
//    Older versions use /etc. Newer seem to use /etc/unixODBC.
//    You can check what is right for your system with the command
//      odbcinst -j -v
//    Additionally there are some GUI tools available to assist with
//    configuring unixODBC. These can be downloaded from
//       http://unixodbc-gui-qt.svn.sourceforge.net/
//    This appears not to be available as a precompiled package for
//    Debian/Ubuntu (and does not compile entirely cleanly as of
//    November 2011.)
//    BEWARE: There is also a user odbc.ini (default ~/.odbc.ini).
//    TODO: Scan this file as well as the system odbc.ini.
//    Note that unixODBC scans the user odbc.ini BEFORE the system one.
//    If you have a section in this that matches the section name in the
//    DTNME configuration file dbname parameter, then unixODBC will try to
//    use that DSN rather than one with the same name in the system odbc.ini.
//    If they are different this could lead to confusion.
//
//    After much tearing of hair, inspection of the ODBC trace file (see Trace and
//    TraceFile options in the DSN below), and finally delving into the source code
//    of sqliteodbc, it appears that sqliteodbc immediately reissues BEGIN TRANSACTION
//    after a COMMIT.  This is done internally without the application asking for it
//    when auto-commit is turned off.  This means that when the application issues
//    its own BEGIN TRANSACTION, SQLite barfs because it thinks it is a nested
//    transaction and thereafter the application end_transaction doesn't work
//    because it doesn't believe that a transaction is in progress.  However, there
//    is an essentially undocumented feature in sqliteodbc that turns off the
//    automatic generation of BEGIN TRANSACTION statements: include
//    NoTxn = Yes
//    in the DSN section and the transactions start working.
//
//  
//      [SQLite3]
//      Description  = SQLite3 ODBC Driver
//      Driver       = <installation directory for sqlite odbc driver>/libsqlite3odbc-0.83.so
//      Setup        = <installation directory for sqlite odbc driver>/libsqlite3odbc-0.83.so
//      UsageCount   = 1
//  
//    $ODBCSYSINI/odbc.ini:  add new DSN sections for DTN test/production DB as necessary
//
//      [<config DB name for DTN DB>]
//      Description   			= SQLite database storage DSN for DTNME
//      Driver        			= SQLite3
//      Database      			= <full_path to DB directory>/<SQLite database file name>
//      Timeout       			= 100000
//      StepAPI       			= No
//      NoWCHAR       			= No
//      ; This more or less undocumented feature of sqliteodbc must be turned on to prevent
//      ; sqliteodbc issuing its own BEGIN TRANSACTION immediately after every COMMIT.
//      ; With 'NoTxn = Yes' you are in control and can issue BEGIN TRANSACTION as wanted.
//      NoTxn                   = Yes
//      ; You may also find the trace from unixODBC useful...
//      Trace                   = Yes
//      TraceFile               = <full path name of some suitable scratch file>
//
//    After much investigation and experiment, it appears that the
//    keywords and the section names in both odbc.ini and odbcinst.ini
//    should be case insensitive.  unixODBC uses strcasecmp to check for
//    equality of section names and property names. However the values
//    of properties *are* case sensitive and spaces other than round
//    the '=' are preserved.
//
//    The odbc.ini parsing code now ignores all blank lines and comment lines
//    starting with ';' or '#'.  Section and property names are case insensitive.
//    Property lines may have trailing comments starting with ; or #.
//  
//##########################################################


#ifdef HAVE_CONFIG_H
#include <oasys-config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <debug/DebugUtils.h>
#include <io/FileUtils.h>

#include <util/StringBuffer.h>
#include <util/Pointers.h>
#include <util/ScratchBuffer.h>

#include "StorageConfig.h"
#include "ODBCSQLite.h"
#include "util/InitSequencer.h"

#if LIBODBC_ENABLED

namespace oasys
{
/******************************************************************************
 *
 * ODBCDBSQLite
 *
 *****************************************************************************/

//----------------------------------------------------------------------------
ODBCDBSQLite::ODBCDBSQLite(const char *logpath):
ODBCDBStore("ODBCDBSQLite", logpath)
{
    logpath_appendf("/ODBCDBSQLite");
    log_debug("constructor enter/exit.");
    aux_tables_available_ = true;
}

//----------------------------------------------------------------------------
ODBCDBSQLite::~ODBCDBSQLite()
{
    log_debug("ODBCDBSQLite destructor enter/exit.");
    // All the action (clean shutdown of database) takes place in ODBCDBStore destructor
}

//----------------------------------------------------------------------------
//  For ODBC, the cfg.dbname_ refers to the Data Source Name (DSN) as defined
//  in the odbc.ini file (i.e. the name inside square brackets, e.g. [DTN],
//  starting a section in the file).
//  This code parses the odbc.ini file looking for the given DSN,
//  then checks that there is a non-empty 'Database =' entry in the odbc.ini file.
//  It also extracts the extension SchemaPre/PostCreationFile entries if present.
//
//  For SQLite this function serves to check that there is a DSN name of <dbname>
//  in the odbc.ini fileand that it contains a database file path name.
int
ODBCDBSQLite::parse_odbc_ini_SQLite(const char *dsn_name, char *full_path)
{
    FILE *f;
    char odbc_ini_filename[1000];
    char *one_line;
    char *tok;
    char c;
    size_t line_len;

    char odbc_dsn_identifier[500];
    char section_start = '[';
    char section_end = ']';
    bool found_dsn = false;

    sprintf(odbc_dsn_identifier, "%c%s%c", section_start, dsn_name, section_end);
    full_path[0] = '\0';

    // TODO: (Elwyn) Some installations may use /etc/unixODBC to house the odbc.ini file.
    // Note that ODBCSYSINI is the name of the directory where the system odbc.ini
    // and odbcinst.ini are found (not the name of either of these files).
    // We really ought to also check in the user's ODBCINI (defaults to ~/.odbc.ini).
    if ( ( tok = getenv("ODBCSYSINI") ) == NULL ) {
        sprintf(odbc_ini_filename, "/etc/%s", ODBCDBStore::ODBC_INI_FILE_NAME.c_str());
    } else {
        sprintf(odbc_ini_filename, "%s/%s", tok, ODBCDBStore::ODBC_INI_FILE_NAME.c_str());
    }

    f = fopen(odbc_ini_filename, "r");
    if (f == NULL)
    {
        log_debug("Can't open odbc.ini file (%s) for reading", odbc_ini_filename);
        return (-1);
    }

    one_line = (char *) malloc(1000);
    while (getline(&one_line, &line_len, f) > 0)
    {
        tok = strtok(one_line, "= \t\n");
    	// Ignore blank lines and comment lines.
        if ( ( tok==NULL ) || ( (c = tok[0]) == ';' ) || ( c == '#') ) {
        	continue;
        }

        // Stop at beginning of next section
        if ( found_dsn && ( c == section_start ) ) {
        	break;
        }

        // Look for interesting section start - names are not case sensitive
        if ( !found_dsn && ( strcasecmp(tok, odbc_dsn_identifier) == 0 ) )
        {
            log_debug("Found DSN %s", odbc_dsn_identifier);
            found_dsn = true;
            continue;
        // In the right section look for Database property - again not case sensitive
        } else if ( found_dsn && ( strcasecmp(tok, "Database") == 0 ) ) {
            // Extract the property value - name of database file used
        	// Not sure if comments are allowed at the end of lines but
        	// stop at ; or # to be sure, as these are not good characters
        	// in database names.
        	// For SQLite this path name is needed both to ensure that the
        	// directory containing the file is present when initializing
        	// the database so that SQLite3 can create the file on first access,
        	// and also to allow sqlite3 to be used directly to read in the
        	// additional schema file, if any.
            tok = strtok(NULL, " \t\n=;#");
            if ( tok == NULL ) {
            	log_crit("Empty Database property in DSN '%s' in file '%s'.",
            			dsn_name, odbc_ini_filename );
            	break;
            }
            log_debug("Found database path name (%s) in odbc.ini file (%s).",
                      tok, odbc_ini_filename);
            strcpy(full_path, tok);
        }
    }
    free(one_line);
    fclose(f);

    // All is well if we found the section requested and it has a non-empty Database value
    if (found_dsn  && (full_path[0] != '\0') ) {
    	return(0);
    }

    if ( ! found_dsn ) {
    	log_crit("DSN '%s' not found in odbc.ini file (%s).  Aborting.",
    			dsn_name, odbc_ini_filename );
    } else {
    	log_crit("Database property is missing or empty in DSN '%s' in odbc.ini file (%s), Aborting.",
    			dsn_name, odbc_ini_filename );
    }

    return (-1);
}

//----------------------------------------------------------------------------
int
ODBCDBSQLite::init(const StorageConfig & cfg)
{
    int ret;

    log_debug("init entry.");

    // Note that for ODBC-based storage methods, the cfg.dbdir_ variable is only
    // used to specify the directory used to store the clean exit recording file
    // (.ds_clean - see DurableStore.cc).   However, if the user chooses, it may
    // be that the SQLite database file is in the same directory.  See discussion
    // at the head of this file.
    std::string dbdir = cfg.dbdir_;

    // Record the ODBC DSN to be used  (variable defined in ODBCDBStore)
    dsn_name_ = cfg.dbname_;

    // Record whether a shared file is requested - this is not really relevant for SQLite
    // because although it is in principle possible to keep tables in separate files
    // additional work is needed and it is not easy (possible?) when using ODBC.
    // Thus currently this will be ignored.
    sharefile_ = cfg.db_sharefile_;
    if ( ! sharefile_) {
        log_warn("For SQLite databases, specifying separate files ('storage db_sharefile = no') is ignored.");
    }

    // Record configured value of auto_commit
    auto_commit_ = cfg.auto_commit_;

    char database_fullpath[500] = "";
    char database_dir[500] = "";

    // For ODBC, the cfg.dbname_ refers to the Data Source Name (DSN) as defined
    // in the odbc.ini file (i.e. the section name inside square brackets, e.g. [DTN]),
    // starting a section in the file).
    // (Remember this is really an old style Microsoft configuration file).
    // This code parses the odbc.ini file looking for the given DSN,
    // then uses the 'Database =' entry from the odbc.ini file to determine
    // the location of the actual database file.
    //
    //
    // Parse the odbc.ini file to find the database name; copy the full path to
    // the database file into database_fullpath and find the names of the extra schema
    // files if present.
    ret = parse_odbc_ini_SQLite(dsn_name_.c_str(), database_fullpath);
    if ( ret!=0 ) {
        return DS_ERR;
    }
    char *c;
    strcpy(database_dir, database_fullpath);
    c = rindex(database_dir, '/');
    if ( c != NULL )
    {
    	if ( c != &database_dir[0] )
    	{
    		// Normal case full path name with non-empty path
    		*c = '\0';
    	}
    	else
    	{
    		// Special case - file in root directory - not recommended!
    		strcpy(database_dir, "/");
    	}
    }
    else
    {
    	// Just a file name given - no path - database will be found in process working directory (not desirable)
    	strcpy(database_dir, ".");
    }
    if (database_dir[0] != '/')
    {
    	log_warn("Database is not specified with a full pathname in DSN %s - results may be inconsistent as database is accessed in current working directory.",
    			 dsn_name_.c_str());
    }

    bool force_schema_creation = false;

    // Remove 'dbdir' directory if 'tidy' requested
    bool dirs_are_same = false;

    log_debug("dbdir: %s; database_dir: %s", cfg.dbdir_.c_str(), database_dir);

    if (cfg.tidy_)
    {
    	// Check if dbdir and database_dir are the same directory
    	// It is possible that either one or both does not exist - this is not an error
    	// but we assume they are different if one doesn't exist.  If neither exists
    	// then tidy has nothing to do anyway.
    	struct stat stat_dbdir, stat_database_dir;

    	if ((stat(cfg.dbdir_.c_str(), &stat_dbdir) == 0) &&
    		(stat(database_dir, &stat_database_dir) == 0)) {
    		// Directories are the same if device of residence and inode are the same
    		if ((stat_dbdir.st_dev == stat_database_dir.st_dev) &&
    			(stat_dbdir.st_ino == stat_database_dir.st_ino)) {
    			dirs_are_same = true;
    			log_crit("init WARNING:  tidy/prune will delete any database files in dbdir directory %s.",
    					 cfg.dbdir_.c_str());
    			log_crit("               This is the same directory as specified in DSN %s.",
    					 cfg.dbname_.c_str());
    		}
    	}

        log_crit("init WARNING:  tidy/prune will now delete all files in directory %s.  Is this what you want?",
                 dbdir.c_str());
        // Note.. we postpone tidying the database tables until it has been connected to
        // below.  This is because this clearout may have deleted the database file.
        if (!dirs_are_same){
        	log_crit("init WARNING:  All data will be deleted from database file %s. Is this what you want?",
        			 database_fullpath);
        }
        prune_db_dir(dbdir.c_str(), cfg.tidy_wait_);
        force_schema_creation = true;

    }

    if (cfg.init_){
    	force_schema_creation = true;
    }

    // Check that the directory used for SQLite files exists and create it if
    // it does not and we are initializing database
    // (i.e., if cfg.init_ or cfg.tidy_ is true).
    bool db_dir_exists;
    int err;

    err = check_db_dir(database_dir, &db_dir_exists);
    if (err != 0)
    {
        log_crit("init  ERROR:  CHECK failed on SQLite DB dir %s so exit!",
                 dbdir.c_str());
        return DS_ERR;
    }
    if (!db_dir_exists)
    {
        log_crit("init  WARNING:  SQLite DB dir %s does not exist SO WILL CREATE which means all prior data has been lost!",
                 database_dir);
        if (force_schema_creation)
        {
            if (create_db_dir(database_dir) != 0)
            {
                return DS_ERR;
            }
            log_debug("init SQLite database directory %s created.",
            		  database_dir);
        } else {
            log_crit("init  SQLite DB dir %s does not exist and not told to create!",
                     database_dir);
            return DS_ERR;
        }
    } else {
    	log_debug("init  directory %s for SQLite files exists", database_dir);
    }

    // Check that the directory cfg.dbdir_ used for .ds_clean file exists and create it if
    // it does not and we are initializing database (i.e., if cfg.init_ is true)
    err = check_db_dir(cfg.dbdir_.c_str(), &db_dir_exists);
    if (err != 0)
    {
        log_crit("init  ERROR:  CHECK failed on DB dir %s so exit!",
                 cfg.dbdir_.c_str());
        return DS_ERR;
    }
    if (!db_dir_exists)
    {
        log_crit("init  WARNING: directory for .ds_clean %s does not exist.",
                 cfg.dbdir_.c_str());
        if (force_schema_creation)
        {
            if (create_db_dir(cfg.dbdir_.c_str()) != 0)
            {
                return DS_ERR;
            }
            log_info("Directory %s for .ds_clean created,", cfg.dbdir_.c_str());
        } else {
            log_crit
                ("init  directory for .ds_clean %s does not exist and not told to create!",
                 cfg.dbdir_.c_str());
            return DS_ERR;
        }
    }
    else{
    	log_debug("init: directory for .ds_clean %s exists.", cfg.dbdir_.c_str());
    }

    // xxx Elwyn: Not sure what this was for.
    //log_debug("init - Verify Env info for DB Alias=%s\n", cfg.db_name_.c_str());

	// ODBC automatically creates DB file on SQLConnect so check file first
	struct stat statBuf;
	log_debug("init - check if SQLite DB file %s exists, is readable/writable and is non-empty.\n",
		 database_fullpath);
	if (stat(database_fullpath, &statBuf) == 0)
	{
		if (!S_ISREG(statBuf.st_mode) || (access(database_fullpath, (R_OK | W_OK)) != 0))
		{
			log_crit("init  SQLite database file %s is not a regular file or is not readable/writable.",
					  database_fullpath);
			return DS_ERR;
		}
		if (statBuf.st_size == 0)
		{
			log_info("init - SQLite DB file %s exists but IS EMPTY so continue and CREATE DB SCHEMA",
				     database_fullpath);
			force_schema_creation = true;
		} else {
			log_debug("init - SQLite DB file %s is ready for access.", database_fullpath);
		}
	} else if (!force_schema_creation) {
		log_crit("init - SQLite DB file %s DOES NOT EXIST or does not permit access and creation was not requested.",
			 database_fullpath);
	}

	// NOTE: It would be possible to use multiple files (e.g., one for each table
	// in SQLite3 by using the ATTACH DATABASE mechanism.  It isn't clear that this
	// would help performance and it significantly increases the setup complexity.
	// Currently one common file is used and the db_sharefile parameter is ignored.
    log_info("init initializing DSN name %s (sharefile requested %s - currently ignored), database=%s",
             dsn_name_.c_str(), cfg.db_sharefile_ ? "shared" : "not shared",
             database_fullpath);

    // Setup all the ODBC environment and connect to the database.
    ret = connect_to_database( cfg );
    if ( ret != DS_OK ) {
    	return ret;
    }

    // If tidy was requested and the database was not scrubbed by previous directory
    // pruning, drop all the tables and recreate them.
    if (cfg.tidy_ && !dirs_are_same){
        // Iterate through existing tables deleting them (META_DATA_TABLES last).
        StringVector table_names;
        if ((ret = get_table_names(&table_names)) == DS_OK) {
            table_names.push_back(META_TABLE_NAME);
            log_debug("init: starting to drop tables.");
            for (StringVector::iterator iter = table_names.begin();
                    iter != table_names.end(); iter++ ) {

                char sql_string[100];
				snprintf(sql_string, 100, "drop table if exists %s",
                         iter->c_str());

				/*!
				 * Release any previously bound parameters and columns.
				 */
				sqlRC = SQLFreeStmt(dbenv_.hstmt, SQL_CLOSE);
				if (!SQL_SUCCEEDED(sqlRC))
				{
					log_crit("ERROR:  int - tidy - failed Statement Handle SQL_CLOSE");
                    return DS_ERR;
				}

				sqlRC = SQLFreeStmt(dbenv_.hstmt, SQL_RESET_PARAMS);
				if (!SQL_SUCCEEDED(sqlRC))
				{
					log_crit("ERROR:  int - tidy - failed Statement Handle SQL_RESET_PARAMS");
                    return DS_ERR;
				}

				sqlRC = SQLExecDirect(dbenv_.hstmt, (SQLCHAR *) sql_string, SQL_NTS);
				if (!(SQL_SUCCEEDED(sqlRC) || (sqlRC == SQL_NO_DATA))) {
					log_crit("ERROR: init - tidy: Unable to drop table %s - ret %d", iter->c_str(), sqlRC);
                    return DS_ERR;
				}
				log_debug("init: successfully dropped table %s.", iter->c_str());
			}
		} else if (ret == DS_NOTFOUND) {
			log_warn("init: no tables found during tidy - continue with creation.");
		}
		else {
			log_crit("Unable to read table names from META_DATA_TABLES during tidy - SQLite %s database uninitialized or corrupt.",
					 database_fullpath);
			return DS_ERR;
		}
        // Need to rebuild the database tables now
        force_schema_creation = true;
    }

    if (force_schema_creation)
    {
        log_info("Installing new empty database schema in database file %s",
                 database_fullpath);

        // NB.  Do NOT turn off auto-commit mode until schema installation is complete
        // See comments on set_auto_commit_mode in ODBCStore.cc.

        // Create internal auxiliary tables - main storage tables are created automatically
        // when ODBCDBTable instances are created for specific tables with DS_CREATE true.
        ret = create_aux_tables();
        if ( ret != DS_OK ) {
        	return ret;
        }
        // Some schema work may be needed both before and after main tables are created.
        // The full path names of the two possible odbc_schema_pre/post_creation files
        // are specified in the storage configuration.  Each may contain SQL that needs
        // to be run, respectively, before or after the creation of the main tables.
        // These files allow other SQL tables to be created and cross-table functions
        // (such as triggers) to be installed once all the tables have been set up.
        // This allows items such as auxiliary detail tables to be created more easily
        // than using ODBC but effectively assumes that the Oasys code is running on
        // the same server as the database.
        //
        // Fabricate a string containing a command to be run on the server machine
        // in each case. Run the pre table creation (if any) now.
        //
        // Note the tricksy deletion of character '\'.  This is done so that a single
        // script is good for both MySQL and SQLite.  For SQLite it removes the escaping
        // of newlines in multi-line command input. It seems SQlite doesn't want this.
        // Shame they aren't the same!
        char schema_creation_string[1024];
        if ( cfg.odbc_schema_pre_creation_.length() > 0 ) {
            log_info("init: sourcing odbc schema pre table creation file (%s).",
            		cfg.odbc_schema_pre_creation_.c_str());
            if (access(cfg.odbc_schema_pre_creation_.c_str(), R_OK) != 0) {
            	log_crit("Configured schema pre table creation file (%s) is not readable.",
            			cfg.odbc_schema_pre_creation_.c_str());
            	return DS_ERR;
            }

            sprintf(schema_creation_string, "cat %s | tr -d '\\\\' | sqlite3 %s",
            		cfg.odbc_schema_pre_creation_.c_str(), database_fullpath);

            ret = system(schema_creation_string);

            // Check that the command actually ran and exited with a success code.
        	if (!(WIFEXITED(ret) && (WEXITSTATUS(ret) == 0))) {
        		log_crit("init: Sourcing schema pre table creation file failed.");
        		return DS_ERR ;
        	}
        }

        // The command for post table creation is created and saved for execution by
        // create_finalize once the tables have been created.  This avoids having to
        // squirrel both parameters and know about sqlite3 later.
        if ( cfg.odbc_schema_post_creation_.length() > 0 ) {
            log_info("init: checking odbc schema post table creation file (%s) to be sourced after creation",
            		cfg.odbc_schema_post_creation_.c_str());
            if (access(cfg.odbc_schema_post_creation_.c_str(), R_OK) != 0) {
            	log_crit("Configured schema post table creation file (%s) is not readable.",
            			cfg.odbc_schema_post_creation_.c_str());
            	return DS_ERR;
            }

            sprintf(schema_creation_string, "cat %s | tr -d '\\\\' | sqlite3 %s",
            		cfg.odbc_schema_post_creation_.c_str(), database_fullpath);
            schema_creation_command_ = std::string(schema_creation_string);
        }

    }

    // Set aux_tables_available_ according to configuration setting as
    // SQLIte3 can have these tables available
    // [Note - Elwyn] Really ought to check that we did actually create the
	// auxiliary tables we are expecting but since Oasys doesn't actually
	// know which tables are to be created we delegate this check to the
	// user of this scheme.
    aux_tables_available_ = cfg.odbc_use_aux_tables_;

    log_debug("init: aux_tables_available = %s", aux_tables_available_ ? "true" : "false");
    init_ = true;

    log_debug("init exit.");
    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBSQLite::create_finalize(const StorageConfig& cfg)
{
	int ret = DS_OK;

	if (cfg.init_) {
        if ( schema_creation_command_.length()>0 ) {
        	log_info("Running schema finalization command: %s", schema_creation_command_.c_str());
        	int status = system(schema_creation_command_.c_str());
        	if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
        		ret = DS_OK;
        	} else {
        		ret = DS_ERR;
        	}
        } else {
        	log_debug("No schema finalization commands to run");
        }
	}

	// Initialization complete (if relevant) - can now select transaction mode if required.
    if (ret == DS_OK)
    {
		ret = set_odbc_auto_commit_mode();
    }

	return ret;

}

//----------------------------------------------------------------------------
std::string
ODBCDBSQLite::get_info()const
{
    return "ODBC driver with SQLite database";
}

//----------------------------------------------------------------------------
} // namespace oasys

#endif // LIBODBC_ENABLED
