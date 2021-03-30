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
//  For main configuration information relating to ODBC access to an MySQL
//  database see ODBCStore.cc.
//
//  This module filters out the non-standard parts of ODBC operation.
//  The class defined here (ODBCDBMySQL) inherits the standard ODBC code
//  from the ODBCDBStore class and provides extra initialization code for
//  checking that there is a odbc.ini section for the selected ODBC
//  DSN (Data Source Name).
//
//  For MySQL (and other client server databases) it is necessary to create the
//  database, setup a suitable user with a known password for accessing this
//  database and install the entry in odbc.ini before ODBC can access the database.
//  This is unlike (say) SQLite where the database file is automatically created
//  on first access provided that the necessary directory exists.
//
//  Required Software on Linux box to run:
//   unixODBC  (unixODBC-2.2.11-7.1 - this conforms to ODBC v3.80)
//   MySQL   (MySQL v5.1. client and server)
//   MySQL Connector/ODBC (v5.1, conforms to ODBC v3.51)
//  Ubuntu packages are available for Release 10.04 LTS for all these components
//
//
//  Modified OASYS storage files:
//    Insertions into DurableStore.cc to instantiate ODBCDBMySQL as the storage
//    implementation:
//    #include "ODBCMySQL.h"
//
//      ...
//  
//    #ifdef LIBODBC_ENABLED
//      ...
//      else if (config.type_ == "odbc-mysql")
//      {
//          impl_ = new ODBCDBMySQL(logpath_);
//          log_debug("impl_ set to new ODBCDBMySQL");
//      }
//     ...
//    #endif
//
//
// TODO: Add test stuff for MySQL
//  Required OASYS test files:
//     mysql-db-test.cc
//
//  When using ODBC the interpretation of the storage parameters is slightly
//  altered.
//  - The 'type' is set to 'odbc-mysql' for a MySQL database accessed via ODBC.
//  - The 'dbname' storage parameter is used to identify the Data Source Name (DSN)
//    configured into the odbc.ini file. In the case of MySQL, the database to be
//    used is defined in the DSN specification, along with necessary parameters
//    to allow access to that database via the MySQL client interface.
//  - The 'dbdir' parameter is still needed because the startup/shutdown system
//    can, on request, store a 'clean shutdown' indicator by creating the
//    file .ds_clean in the directory specified in 'dbdir'.   Hence 'dbdir' is
//    still (just) relevant.
//  - The storage payloaddir *is* still used to store the payload files which
//    are not held in the database.
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
// There is one additional ODBC/MySQL specific parameter:
//  - odbc_mysql_keep_alive_interval:
//									Specifies the interval in minutes beteen
//									runs of the MySQL connection keep alive
//									transaction. (Default 10 minutes).
//
//  Modifications to ODBC configuration files (requires root access):
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
//      [ODBC Drivers]
//      MyODBC          = Installed
//
//      [MyODBC]
//      Driver          = <installation directory>/lib/libmyodbc5.so
//      Description     = Driver for connecting to MySQL database server
//      Threading       = 0
//  
//    $ODBCSYSINI/odbc.ini:  add new sections for DTNME DSN(s).  A newly created
//      MySQL server always contains an empty 'test' database.  It is necessary
//      (or at least desirable) to create a user (e.g., dtn_user) that will be
//      used to access the DTNME database(s).  The user needs to be created and
//      given a password and relevant privileges for the database (ALL privileges
//      is probably OK). If the user is created before the database, it may be
//      necessary to explicitly grant privileges for the database even if the user
//      was given privileges for all databases previously, using the mysql command
//       line interface, thus:
//        mysql -u root -p
//        Password: <Root database user password>
//         SQL> CREATE USER "dtn_user"@"localhost" IDENTIFIED BY "<password>";
//         SQL> CREATE DATABASE dtn_test;
//         SQL> GRANT ALL ON dtn_test.* TO "dtn_user"@"localhost" IDENTIFIED BY "<password>";
//
//      If using a version of the MySQL database before 5.1.6 and defining TRIGGERS
//      on the database, it is also necessary to give the use the SUPER privilege, thus:
//         SQL> GRANT SUPER ON dtn_test.* TO "dtn_user"@"localhost" IDENTIFIED BY "<password>";
//
//      Note that it is advisable to make database names from the character set
//      [0-9a-zA-Z$_].  Using other characters (especially - and /) requires the
//      database name to be quoted using ` (accent grave/backtick). ODBC may or
//      may not cope with such names.
//      The possible values of the OPTION parameter are defined in
//      Section 20.1.4.2 "Connector/ODBC Connection Parameters" of the MySQL
//      Reference Manual (version 5.1).

//      [ODBC Data Sources]
//      myodbc5           = MyODBC 3.51 Driver DSN test
//      dtn-test-mysql    = MySQL DTN test database
//
//      [myodbc5]
//      Driver       = MyODBC
//      Description  = Connector/ODBC 3.51 Driver DSN
//      SERVER       = localhost
//      PORT         =
//      USER         = dtn_user
//      Password     = <the MySQL password set up for dtn_user>
//      Database     = test
//      OPTION       = 3
//      SOCKET       =

//      [dtn-test-mysql]
//      Driver       			= MyODBC
//      Description  			= Connector/ODBC 3.51 Driver DSN
//		; Note that in principle the database could be on another host BUT the
//		; schema creation mechanism below would not work as programmed.  Also
//		; for the identified use with DTN having the database on a different machine
//		; does not make much sense as there would not be a permanent connection.
//      SERVER       			= localhost
//      PORT         			=
//      USER         			= dtn_user
//      Password     			= <password>
//      Database     			= dtn_test
//      OPTION       			= 3
//      SOCKET      			 =
//      ; You may also find the trace from unixODBC useful (although it didn't work for me with MySQL)...
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
//  
//##########################################################


#ifdef HAVE_CONFIG_H
#include <oasys-config.h>
#endif

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include <debug/DebugUtils.h>

#include <util/StringBuffer.h>
#include <util/Pointers.h>
#include <util/ScratchBuffer.h>

#include "StorageConfig.h"
#include "ODBCMySQL.h"
#include "util/InitSequencer.h"

#if LIBODBC_ENABLED

namespace oasys
{
/******************************************************************************
 *
 * ODBCDBMySQL
 *
 *****************************************************************************/

//----------------------------------------------------------------------------
ODBCDBMySQL::ODBCDBMySQL(const char *logpath):
						ODBCDBStore("ODBCDBMySQL", logpath),
						schema_creation_command_("")
{
    logpath_appendf("/ODBCDBMySQL");
    log_debug("constructor enter/exit.");
    aux_tables_available_ = true;
}

//----------------------------------------------------------------------------
ODBCDBMySQL::~ODBCDBMySQL()
{
    log_debug("ODBCMySQL destructor enter.");
    if (keep_alive_timer_)
    {
    	keep_alive_timer_->cancel();
    }

    // All the rest of the action (clean shutdown of database) takes place in ODBCDBStore destructor
    log_debug("ODBCMySQL destructor exit.");
}

//----------------------------------------------------------------------------

// For ODBC, the cfg.dbname_ refers to the Data Source Name (DSN) as defined
// in the odbc.ini file (i.e. the section name inside square brackets, e.g. [DTN],
// starting a section in the file).
// (Remember this is really an old style Microsoft configuration file).
// This code parses the odbc.ini file looking for the given DSN,

//  This code parses the odbc.ini file looking for the given DSN,
//  then checks that there is a non-empty 'Database =' entry in the odbc.ini file.
//
//  For MySQL this function serves to check that there is a DSN name of <dbname>
//  in the odbc.ini file, that it contains a reasonably sensible database name
//  (a warning is given if the name is not made up of characters from the set
//  [0-9a-zA-Z$_], which otherwise could lead to issues with quoting being required).

int
ODBCDBMySQL::parse_odbc_ini_MySQL(const char *dsn_name, char *database_name,
								  char *user_name, char *password)
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
    database_name[0] = '\0';
    user_name[0] ='\0';
    password[0] = '\0';

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
            // Extract the property value - name of database used
        	// Not sure if comments are allowed at the end of lines but
        	// stop at ; or # to be sure, as these are not good characters
        	// in database names.
        	// Note that for MySQL this is only used when adding in extra schema file
        	// which uses mysql directly and needs the database name.
        	// ODBC is only interested in the section name and we have
        	// to pre-create the database before ODBC can be used.
            tok = strtok(NULL, " \t\n=;#");
            if ( tok == NULL ) {
            	log_crit("Empty Database property in DSN '%s' in file '%s'.",
            			dsn_name, odbc_ini_filename );
            	break;
            }
            log_debug("Found database (%s) in odbc.ini file (%s).",
                      tok, odbc_ini_filename);
            strcpy(database_name, tok);
            // Check for a reasonably sensible name
            bool sensible = true;
            while ( (c = *tok++) != '\0' ) {
            	if ( ! ( isalnum(c) || ( c == '$' ) || ( c == '_') ) ) {
            		sensible = false;
            	}
            }
            if( ! sensible ) {
            	log_warn("Database name '%s' in DSN '%s' uses characters not from set [0-9a-zA-Z$_] - may need quoting.",
            			database_name, dsn_name);
            }
        }
        // In the right section look for User property - again not case sensitive
		else if ( found_dsn && ( strcasecmp(tok, "User") == 0 ) ) {
			// See Database extraction above...
			tok = strtok(NULL, " \t\n=;#");
			log_debug("Found User (%s) for DSN (%s) in odbc.ini file (%s).",
					  tok, dsn_name, odbc_ini_filename);
			strcpy(user_name, tok);
		}
        // In the right section look for Password property - again not case sensitive
		else if ( found_dsn && ( strcasecmp(tok, "Password") == 0 ) ) {
			// See Database extraction above...
			tok = strtok(NULL, " \t\n=;#");
			log_debug("Found Password (redacted!) for DSN (%s) in odbc.ini file (%s).",
					  dsn_name, odbc_ini_filename);
			strcpy(password, tok);
		}
    }
    free(one_line);
    fclose(f);

    // All is well if we found the section requested and it has a non-empty Database value
    if (found_dsn  && (database_name[0] != '\0') && (user_name[0] != '\0') && (password[0] != '\0') ) {
    	return(0);
    }

    if ( ! found_dsn ) {
    	log_crit("DSN '%s' not found in odbc.ini file (%s).  Aborting.",
    			dsn_name, odbc_ini_filename );
    } else {
    	if ( database_name[0] == '\0' ) {
    	log_crit("Database property is missing or empty in DSN '%s' in odbc.ini file (%s), Aborting.",
    			dsn_name, odbc_ini_filename );
    	}
        if ( user_name[0] == '\0' ){
        	log_crit("Database property is missing or empty in DSN '%s' in odbc.ini file (%s), Aborting.",
        			dsn_name, odbc_ini_filename );
        }
        if (password[0] == '\0' ) {
        	log_crit("Database property is missing or empty in DSN '%s' in odbc.ini file (%s), Aborting.",
        			dsn_name, odbc_ini_filename );
        }
    }

    return (-1);
}

int
ODBCDBMySQL::init(const StorageConfig & cfg)
{
    int ret;

    log_debug("init entry.");

    // Note that for ODBC-based storage methods, the cfg.dbdir_ variable is only
    // used to specify the directory used to store the clean exit recording file
    // (.ds_clean - see DurableStore.cc).
    std::string dbdir = cfg.dbdir_;

    // Record the ODBC DSN to be used (variable defined in ODBCDBStore)
    dsn_name_ = cfg.dbname_;

    // Record whether a shared file is requested - this is not relevant for MySQL
    // Storage is controlled entirely by the MySQL server
    sharefile_ = cfg.db_sharefile_;
    if ( ! sharefile_) {
        log_warn("For MySQL databases, specifying separate files ('storage db_sharefile = no') is ignored.");
    }

    // Record configured value of auto_commit
    auto_commit_ = cfg.auto_commit_;

    bool force_schema_creation = false;

    char database_name[500] = "";
    char database_username[100] = "";
    char database_password[100] = "";

    // Parse the odbc.ini file to find the database name, user mname, password and
    // path names of files with extra schema definition statements.
    ret = parse_odbc_ini_MySQL(dsn_name_.c_str(), database_name,
    						   database_username, database_password);

    if ( ret!=0 ) {
        return DS_ERR;
    }

	// For MySQL the only way to test that the database exists is to connect to it.
    // It is not possible to connect to a database that doesn't exist.
    log_info("init connecting to db with name %s", database_name );

    ret = connect_to_database( cfg );
    if ( ret != DS_OK ) {
    	return ret;
    }

    if (cfg.tidy_)
    {
        log_warn("init WARNING:  tidy/prune will delete all tables from selected database (%s) \nand remove directory %s used for .ds_clean.",
        		 database_name, dbdir.c_str() );

        prune_db_dir(dbdir.c_str(), cfg.tidy_wait_);

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
				if (!SQL_SUCCEEDED(sqlRC)) {
					log_crit("ERROR: init - tidy: Unable to drop table %s", iter->c_str());
                    return DS_ERR;
				}
				log_debug("init: successfully dropped table %s.", iter->c_str());
			}
		} else if (ret == DS_NOTFOUND) {
			log_warn("init: no tables found during tidy - continue with creation.");
		}
		else {
			log_crit("Unable to read table names from META_DATA_TABLES during tidy - MySQL %s database uninitialized or corrupt.",
					 database_name);
			return DS_ERR;
		}
        // Need to rebuild the database tables now
        force_schema_creation = true;
    }

    if (cfg.init_) {
    	force_schema_creation = true;
    }

    // Check that the directory cfg.dbdir_ used for .ds_clean file exists and create it if
    // it does not and we are initializing database (i.e., if cfg.init_ is true)
    bool db_dir_exists;

    int err = check_db_dir(dbdir.c_str(), &db_dir_exists);
    if (err != 0)
    {
        log_crit("init  ERROR:  CHECK failed on DB dir %s so exit!",
                 dbdir.c_str());
        return DS_ERR;
    }
    if (!db_dir_exists)
    {
        log_debug("init: Directory %s for .ds_clean does not exist.",
        		  dbdir.c_str());
        if (cfg.init_)
        {
            if (create_db_dir(dbdir.c_str()) != 0)
            {
                return DS_ERR;
            }
            log_info("Directory %s for .ds_clean created,", dbdir.c_str());
        } else {
            log_crit
                ("init  directory for .ds_clean %s does not exist and not told to create!",
                 dbdir.c_str());
            return DS_ERR;
        }
    }
    else{
    	log_debug("init: directory for .ds_clean %s exists.", dbdir.c_str());
    }

    if (force_schema_creation)
    {
        log_info("Installing new empty database schema in database %s",
                 database_name);

        // We don't off auto-commit mode until installation is complete.
        // This doesn't matter too much for MySQL but is vital for SQLite.
        // See comments on set_auto_commit_mode in ODBCStore.cc.

        // Create auxiliary tables - main storage tables are created automatically
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
        // Note the tricksy deletion of character '-'.  This is done so that a single
        // script is good for both MySQL and SQLite.  For MySQL it makes some comment
        // lines into active lines to handle delimiter change which is not wanted for
        // SQLite.  Shame they aren't the same!
        char schema_creation_string[1024];
        if ( cfg.odbc_schema_pre_creation_.length() > 0 ) {
            log_info("init: sourcing odbc schema pre table creation file (%s).",
            		cfg.odbc_schema_pre_creation_.c_str());
            if (access(cfg.odbc_schema_pre_creation_.c_str(), R_OK) != 0) {
            	log_crit("Configured schema pre table creation file (%s) is not readable.",
            			cfg.odbc_schema_pre_creation_.c_str());
            	return DS_ERR;
            }

            sprintf(schema_creation_string, "cat %s | tr -d '-' | mysql %s -u %s -p%s",
            		cfg.odbc_schema_pre_creation_.c_str(), database_name,
            		database_username, database_password );
            ret = system(schema_creation_string);

            // Check that the command actually ran and exited with a success code.
        	if (!(WIFEXITED(ret) && (WEXITSTATUS(ret) == 0))) {
        		log_crit("init: Sourcing schema pre table creation file failed.");
        		return DS_ERR ;
        	}
        }

        // The command for post table creation is created and saved for execution by
        // create_finalize once the tables have been created.  This avoids having to
        // squirrel up all three parameters.
        if ( cfg.odbc_schema_post_creation_.length() > 0 ) {
            log_info("init: checking odbc schema post table creation file (%s) to be sourced after creation",
            		cfg.odbc_schema_post_creation_.c_str());
            if (access(cfg.odbc_schema_post_creation_.c_str(), R_OK) != 0) {
            	log_crit("Configured schema post table creation file (%s) is not readable.",
            			cfg.odbc_schema_post_creation_.c_str());
            	return DS_ERR;
            }
            sprintf(schema_creation_string, "cat %s | tr -d '-' | mysql %s -u %s -p%s",
            		cfg.odbc_schema_post_creation_.c_str(), database_name,
            		database_username, database_password);
            schema_creation_command_ = std::string(schema_creation_string);
        }
    }

    // Set aux_tables_available_ according to configuration setting as
    // MySQL can have these tables available
    // [Note - Elwyn] Really ought to check that we did actually create the
	// auxiliary tables we are expecting but since Oasys doesn't actually
	// know which tables are to be created we delegate this check to the
	// user of this scheme.
    aux_tables_available_ = cfg.odbc_use_aux_tables_;

    log_debug("init: aux_tables_available = %s", aux_tables_available_ ? "true" : "false");

    keep_alive_timer_ =
                new KeepAliveTimer(logpath_, this,
                				   cfg.odbc_mysql_keep_alive_interval_);
            keep_alive_timer_->reschedule();

    init_ = true;

    log_debug("init exit.");
    return 0;
}

//----------------------------------------------------------------------------
int
ODBCDBMySQL::create_finalize(const StorageConfig& cfg)
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
void
ODBCDBMySQL::do_keepalive_transaction()
{
	SQLRETURN ret;
	char var_name[80];
	char var_value[512];
	SQLLEN len_name, len_value;

	ScopeLockIf sl(&serialization_lock_,
				   "Access by KeepAliveTimer::timeout()",
				   serialize_all_);

	ret = SQLFreeStmt(dbenv_.idle_hstmt, SQL_CLOSE);      //close from any prior use
	if (!SQL_SUCCEEDED(ret))
	{
		log_warn("ERROR timeout - failed Statement Handle SQL_CLOSE");
		return;
	}
	ret = SQLBindCol(dbenv_.idle_hstmt, 1, SQL_C_BINARY, var_name, 80, &len_name);
	if (!SQL_SUCCEEDED(ret))
	{
		log_err("get SQLBindCol error %d at column 1", ret);
		return;
	}
	ret = SQLBindCol(dbenv_.idle_hstmt, 2, SQL_C_BINARY, var_value, 80, &len_value);
	if (!SQL_SUCCEEDED(ret))
	{
		log_err("get SQLBindCol error %d at column 1", ret);
		return;
	}
	log_debug("execing SQLEXECDirect");
	ret = SQLExecDirect(dbenv_.idle_hstmt, (SQLCHAR *) "SHOW VARIABLES LIKE 'flush'", SQL_NTS);
	if (!SQL_SUCCEEDED(ret))
	{
		log_warn("ERROR: timeout: Unable to access server variable 'flush'.");
	}
	ret = SQLFetch(dbenv_.idle_hstmt);
	var_name[len_name]= '\0';
	var_value[len_value] = '\0';
	log_debug("Result: var_name %s, var_value %s", var_name, var_value);
	return;
}

//----------------------------------------------------------------------------
std::string
ODBCDBMySQL::get_info()const
{
    return "ODBC driver with MySQL database";
}

//----------------------------------------------------------------------------
void
ODBCDBMySQL::KeepAliveTimer::reschedule()
{
    log_debug("KeepAliveTimer::rescheduling in %d msecs", frequency_);
    schedule_in(frequency_);
}

//----------------------------------------------------------------------------
void
ODBCDBMySQL::KeepAliveTimer::timeout(const struct timeval &now)
{
	log_debug
		("KeepAliveTimer::timeout - requesting value of server variable 'flush' at time %s", ctime(&now.tv_sec));
    store_->do_keepalive_transaction();
    reschedule();
}
//----------------------------------------------------------------------------

}                               // namespace oasys

#endif // LIBODBC_ENABLED
