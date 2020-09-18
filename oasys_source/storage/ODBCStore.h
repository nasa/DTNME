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


#ifndef __ODBC_TABLE_STORE_H__
#define __ODBC_TABLE_STORE_H__

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
#include "../thread/Mutex.h"
#include "../thread/SpinLock.h"
#include "../thread/Timer.h"

#include "DurableStore.h"

#define DATA_MAX_SIZE (1 * 1024 * 1024) //1M - increase this and table create for larger buffers

struct ODBC_dbenv
{
	/*!
	 * Standard ODBC DB handles (environment, database connection, statement).
	 * These items are common to all tables.  The environment and database connection
	 * are allocated when the storage instance is initialized and the database connection
	 * opened.  The statement handles are also allocated when the database connection
	 * is opened and are used for operations outside individual tables, including:
	 *  - hstmt: creating and dropping tables, getting all the table names
	 *  - trans_hstmt: by begin_transaction and end_transation
	 *  - idle_hstmt: by the keep_alive timer (where needed - currently only MySQL)
	 * The individual tables allocate their own statement handles for operations within
	 * that table.
	 */
    SQLHENV m_henv;				///< ODBC environment handle
    SQLHDBC m_hdbc;				///< ODBC database connection handle
    SQLHSTMT hstmt;				///< ODBC statement handle used for general table operations
    SQLHSTMT trans_hstmt;		///< ODBC statement handle used for transaction control
    SQLHSTMT idle_hstmt;		///< ODBC statement handle used for keep_alive timer where needed
};

namespace oasys
{

// forward declarations
    class ODBCDBStore;
    class ODBCDBTable;
    class ODBCDBIteratorDBIterator;
    class StorageConfig;

/**
 * Interface for the generic ODBC datastore
 * Caters for any number of different types of database engine using
 * a ODBC driver manager, the relevant ODBC driver for the selected
 * database engine.
 */
class ODBCDBStore:public DurableStoreImpl
{
    friend class ODBCDBTable;
    friend class ODBCDBIterator;

    public:
        ODBCDBStore (const char* derived_classname, const char *logpath);

        // Can't copy or =, don't implement these
        ODBCDBStore & operator= (const ODBCDBStore &);
        ODBCDBStore (const ODBCDBStore &);

        ~ODBCDBStore ();

        //! @{ Virtual from DurableStoreImpl
        //! Initialize derivative class - must be implemented in derived
        //! classes for specific database engines.
        // int init (const StorageConfig & cfg);

        int get_table (DurableTableImpl ** table,
                       const std::string & name,
                       int flags, PrototypeVector & prototypes);

        int del_table (const std::string & name);
        int get_table_names (StringVector * names);
        std::string get_info () const;
        bool aux_tables_available();

        int begin_transaction (void **txid);
        int end_transaction (void *txid, bool be_durable);
        void *get_underlying ();

        /// @}

    protected:
        //! @{ Common pieces of initialization code.
        //! Factored out of specific driver classes.
        DurableStoreResult_t connect_to_database(const StorageConfig & cfg);
        DurableStoreResult_t set_odbc_auto_commit_mode();
        DurableStoreResult_t create_aux_tables();

        /// @}

        bool init_;					///< Initialized?
        std::string dsn_name_;		///< Data source name (overload purpose of dbname in StorageConfig)
        ODBC_dbenv dbenv_;			///< database environment common to  all tables
        bool sharefile_;			///< share a single db file
        bool auto_commit_;			///< True if auto-commit is on
        SQLRETURN sqlRC;			///< Return code from ODBC routines.
        RefCountMap ref_count_;		///< Ref. count for open tables.
        bool aux_tables_available_;	///< True if implemented and configured

        /// Id that represents the metatable of tables
        static const std::string META_TABLE_NAME;

        /// Id that represents the ODBC DSN configuration file name
        static const std::string ODBC_INI_FILE_NAME;

        bool serialize_all_;            // Serialize all access across all tables
        SpinLock serialization_lock_; // For serializing all access to all tables

    private:


       SpinLock ref_count_lock_;

       /// Get meta-table
       int get_meta_table (ODBCDBTable ** table);

       /// @{ Changes the ref count on the tables, used by
       /// ODBCDBTable
       int acquire_table (const std::string & table);
       int release_table (const std::string & table);
       /// @}

};

/**
 * ODBCDBTable:
 * Object that encapsulates a single table in an SQL database accessed through ODBC.
 * It is instantiated once per table that is managed by Oasys.
 *
 * There are two types of table:
 *  - standard ones intended to provide the persistent storage for objects that
 *    are maintained in memory structures while a program that uses the Oasys
 *    framework is active but need to be recreated from the persistent store after
 *    the program is shutdown and then restarted.  It is not intended that this data
 *    should be accessed by other programs or stored in a format that makes it easy
 *    to inspect the values.  Accordingly these tables just have two columns:
 *    the_key and the_data. This is partly a reflection of the original usage of the
 *    Berkeley database (key, value) pair paradigm which was the original mainstay of
 *    Oasys persistent data storage.
 *		the_data is the serialized data pickled from what ever object is being
 *				 made persistent. There is no external structure and reconstruction of
 *				 the memory structure relies on Serialization functions of the memory
 *				 objects. To ensure that the data can be reconstructed a mechanism
 *				 needs to be provided that provides some assurance that a version of the
 *				 database matches with a version of the (serialization processes of the)
 *				 program.
 *		the_key  is the 'flattened' serialized representation of whatever is the
 *				 unique identifier for each object.  Depending on the type of object
 *				 used for the key, the column may be constructed as a VARBINARY(255)
 *				 or a BINARY(<n>).  The size to be used is passed in from the TypeShim
 *				 class used for the key object using the shim_length function.  If this
 *				 value is 0, a VARBINARY column is used because the keys are variable
 *				 length (typically strings).  The other major sort used is integers of
 *				 4 bytes.
 *
 *	 - auxiliary tables that are intended to make a subset of the data in one of the
 *	   standard tables accessible to other programs by splitting out individual items
 *	   into separate table columns.  How ever these tables should:
 *	   (1) have a one-to-one correspondence of rows with the corresponding standard table
 *	       and use a common key (the_key).  It is best that this constraint is maintained
 *	       by means of database triggers which will create an auxiliary table row whenever
 *	       a corresponding standard table row is created, and delete it again when the
 *	       standard table row is deleted.  Accordingly the DurableStoreImpl put routine
 *	       for auxiliary tables does *not* create rows - it is expected that they will
 *	       already have been created - and the put implementation just has to write the
 *	       data into the empty row.
 *	   (2) be essentially write only as far as the generating program is concerned. It
 *	       should never need to read the data back in again as all the information is in
 *	       the standard table.  Correspondingly, any other program that accesses the
 *	       auxiliary tables should treat them as read only and should not modify the
 *	       data.  This restriction could be enforced by suitable use of database user ids
 *	       with appropriate permissions.
 *	    At present, the data schema for the auxiliary tables is not generated automatically
 *	    although this could be contrived with some additional work.  The data objects given
 *	    as parameters for the table put and get routines must be instances of a
 *	    derived class of the StoreDetail class.  Such an instance contains a vector of
 *	    StoreDetailItem instances that describe the data and the SQL column type. The SQL
 *	    for selecting and updating rows in the auxiliary table is then generated
 *	    automatically from the vector of StoreDetailItems. (Note it would probably be a good
 *	    idea to cache the SQL statement, since it should always be the same, but it will
 *	    still be necessary to bind the data referred to in the StoreDetailItem instances
 *	    to the ODBC column descriptors using SqlBindCol (get case) or SQLBindParameter
 *	    (put case).  The data schema for the auxiliary tables should currently be created
 *	    by the pre- and post-creation scripts that are fed into the database when it is
 *	    initialized or tidied.  Table and index creation would normally be in the pre-creation
 *	    script and trigger creation in the post-creation script.  The column names and SQL
 *	    data types have to be consistent between the creation scripts and the StoreDetailItem
 *	    instances created by the calls of StoreDetail::add_detail that builds the instance
 *	    vector in the constructor (or otherwise) of an instance of a class derived from
 *	    StoreDetail.  StoreDetail derived classes are notionally SerializableObjects but
 *	    this is a handy fiction that allows them to be passed to the methods of
 *	    InternalKeyDurableTable instances.. see below for explanation.
 *
 *	    The process by which instances of ODBCDBTable corresponding to a specific database
 *	    table are created, without Oasys being configured with names and allowing many different
 *	    storage mechanisms to be encapsulated, is perhaps inevitably mind numbingly complex.
 *
 *	    The application program sees two sorts of thing:
 *	    - a singleton instance of the DurableStore class.  Depending on the storage
 *	      configuration this encapsulates an instance of an appropriate class derived
 *	      from DurableStoreImpl.  For the ODBC/SQL cases this is a class derived from
 *	      ODBCDBStore with a thin shim to handle initialization as it differs between
 *	      SQL engines that provide an ODBC driver.  Currently this can either be
 *	      ODBCDBMySQL or ODBCDBSQLite.  The key method is ODBCDBStore::get_table.
 *	      Typically this singleton is created and initialized during application startup.
 *	    - A set of instances of templated classes derived from InternalKeyDurableTable,
 *	      one for each table to be managed.
 *	      The constructor template for these classes accepts
 *	      - a TypeShim class name that is a SerializableObject that encapsulates
 *	      - the type of the actual key item used for the tables, and
 *	      - the class name of a SerializableObject that encapsulates the data to be
 *	        stored in the table.
 *
 *	    Standard tables are initialized by calling the InternalKeyDurableTable::do_init
 *	    method which sets up some flags depending on whether initialization is expected
 *	    or not and in turn calls the DurableStore::get_table method.  Here, the
 *	    DurableStoreImpl::get_table routine in the configured storage class (such as
 *	    ODBCDBSTORE) is called.  If initialization was requested this get_table creates
 *	    the table.
 *
 *	    In all cases it instantiates a class derived from DurableTableImpl
 *	    (such as ODBCDBTable seen here) that does the actual work of interfacing with
 *	    the data storage sub-system and passes a pointer to it back to
 *	    DurableTable::get_table.  There it is encapsulated in an instance of
 *	    StaticTypedDurableTable.  A pointer to this is passed back to the do_init
 *	    method in InternalKeyDurableTable where it is squirrelled up in the instance
 *	    for later use when the various add, get update, delete, etc. methods are called.
 *
 *	    The InternalKeyDurableTable methods are just there to call an appropriate method
 *	    in the DurableTable instance (put, get or del) which in turn calls the
 *	    corresponding method in the DurableTableImpl instance (e.g., ODBCDBTable instance)
 *	    that actually does the work.
 *
 *	    If the table to be created is an auxiliary table rather than a standard one,
 *	    the do_init_aux routine is called in the InternalKeyDurableTable templated
 *	    class instead of do_init.  This passes a flag down through the get_table call
 *	    chain which
 *	    (1) Suppresses standard table creation in the DurableStoreImpl::get_table method, and
 *	    (2) Can be used to set a flag in the DurableTableImpl class instance that records
 *	        that is *is* an auxiliary table.  This can then be used to modify the
 *	        behaviour of the various methods in the DurableTableImpl (i.e., ODBCDBTable)
 *	        instance according to whether the table is a standard or an auxiliary table.
 *
 *	    Finally, in order to control the SQL type of the_key column, the
 *	    InternalKeyDurableTable do_init and do_init_aux methods call the shim_length
 *	    static method in the ShimType used for the key field.  This either returns 0 if
 *	    key is variable length, or the length in octets of the flattened key (e.g., 4 for
 *	    an unsigned integer).  This number is incorporated into the flags parameter
 *	    passed down through the get_table chain and onto the constructor of the
 *	    DurableTableImpl instance.  This allows DurableTableImpl::get_table to specify
 *	    the data type for the_key column (other databases can ignore it), and the database
 *	    interaction routines can specify the correct local and database column types
 *	    for the_key when accessing the database (again it can be ignored if it is of
 *	    no consequence for other types of datastore).
 *
 *	    For the SQL databases, the_key columns are either BINARY(<n>) if the size is fixed
 *	    or VARBINARY(255) if the size is varible.
 *
 *	    [Elwyn: I hope this goes some way to explain an extremely complex scheme.  It
 *	    took me a considerable time to get my brain around the scheme and I still get lost
 *	    sometimes after taking a break form this area.] -
 */
class ODBCDBTable:public DurableTableImpl, public Logger
{
    friend class ODBCDBStore;
    friend class ODBCDBIterator;

    public:
        ~ODBCDBTable ();
        /// @{ virtual from DurableTableImpl
        int get (const SerializableObject & key, SerializableObject * data);
        int get (const SerializableObject & key,
                 SerializableObject ** data,
                 TypeCollection::Allocator_t allocator);

        int put (const SerializableObject & key,
                 TypeCollection::TypeCode_t typecode,
                 const SerializableObject * data, int flags);

        int del (const SerializableObject & key);

        size_t size () const;

        int print_error (SQLHENV henv, SQLHDBC hdbc, SQLHSTMT hstmt);

        /**
         * Create an iterator for table t. These should not be called
         * except by ODBCDBTable.
         */
        DurableIterator *itr ();
        /// @}
    protected:
        /*!
         * Access for key size needed in iterator.
         */
        int get_key_size() { return key_size_; }

    private:
        bool is_aux_table() { return is_aux_table_; }

        ODBC_dbenv * db_;		///< ODBC environment and connection handles
        ODBCDBStore *store_;	///< The ODBCDBStore instance that created this table
        int key_size_;			///< The number of bytes in the key - if 0 then VARBINARY
        bool is_aux_table_;		///< If this is set the table is not one of the
								///< standard set with a serialized blob.  On put
								///< and get the data item will be a pointer to a
								///< class instance derived from StoreDetail and the
								///< SQL has to be constructed dynamically from the
								///< vector of descriptors in the data item.

        // Lock to ensure that only one thread accessed a particular
        // table at a time.  There is a single SQLHSTMT per table
        // that stores the current command; multiple threads concurrently
        // messing with that is bad.
        SpinLock lock_;

        // Lock to ensure that only one iterator is active at a given
        // time for a given table.  this is not necessarily a
        // constraint of the database but as with the lock above,
        // there's only one SQLHSTMT per table to be used for iterators.
        SpinLock iterator_lock_;

        SQLHSTMT hstmt_;
        SQLHSTMT iterator_hstmt_;
        
        //! Only ODBCDBStore can create ODBCDBTables
        ODBCDBTable (const char *logpath,
                       ODBCDBStore * store,
                       const std::string & table_name,
                       bool multitype,
                       ODBC_dbenv * db,
                       int key_size,
                       bool is_aux_table = false);

        /// Whether a specific key exists in the table.
        int key_exists (const void *key, size_t key_len);
};

/**
 * Iterator class for ODBC DB tables.
 */
class ODBCDBIterator:public DurableIterator, public Logger
{
    friend class ODBCDBTable;

    private:

        ODBCDBIterator (ODBCDBTable * t);

        ODBCDBTable* for_table_;

        u_int32_t reverse(u_char *key_int);

    public:
        virtual ~ ODBCDBIterator ();
        /// @{ virtual from DurableIteratorImpl
        int next ();
        int get_key (SerializableObject * key);
        /// @}

    protected:
        SQLHSTMT cur_;        // current stmt handle which controls current cursor
        char cur_table_name_[128];
        bool valid_;            ///< Status of the iterator

        u_char key_[KEY_VARBINARY_MAX + 1];	///< Current element key - with guaranteed space for a nul.
        SQLLEN key_len_;					///< Current key length
        u_int32_t rev_key_;					///< reversed byte key when an integer
};

};     // namespace oasys

#endif // LIBODBC_ENABLED

#endif //__ODBC_TABLE_STORE_H__
