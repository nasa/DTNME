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

#ifndef _OASYS_SQL_SERIALIZE_H_
#define _OASYS_SQL_SERIALIZE_H_

/**
 * @file
 *
 * This file defines the serialization and deserialization objects to
 * be used in an SQL storage context.
 */

#include "Serialize.h"
#include "../util/StringBuffer.h"

namespace oasys {

class SQLImplementation;

/**
 * SQLQuery implements common functionality used when building up a
 * SQL query string.
 */
class SQLQuery : public SerializeAction {
public:
    /**
     * Constructor.
     */
    SQLQuery(action_t type, const char* table_name, SQLImplementation* impl,
             const char* initial_query = 0);
    
    /**
     * Return the constructed query string.
     */
    const char* query() { return query_.c_str(); }
    
    /**
     * Return a reference to the query buffer.
     */
    StringBuffer* querybuf() { return &query_; }
    
protected:
    const char* table_name_;
    SQLImplementation* sql_impl_ ;
    StringBuffer query_;
};

/**
 * SQLInsert is a SerializeAction that builts up a SQL "INSERT INTO"
 * query statement based on the values in an object.
 */
class SQLInsert : public SQLQuery {
public:
    /**
     * Constructor.
     */
    SQLInsert(const char* table_name, SQLImplementation *impl);
  
    virtual void begin_action();
    virtual void end_action();
    
    /**
     * Since insert doesn't modify the object, define a variant of
     * action() that operates on a const SerializableObject.
     */
    using SerializeAction::action;
    int action(const SerializableObject* const_object)
    {
        return(SerializeAction::action((SerializableObject*)const_object));
    }
        
    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, int32_t* i);
    void process(const char* name, int16_t* i);
    void process(const char* name, int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);
    void process(const char* name, std::string* s);
};

/**
 * SQLUpdate is a SerializeAction that builts up a SQL "UPDATE"
 * query statement based on the values in an object.
 */
class SQLUpdate : public SQLQuery {
public:
    /**
     * Constructor.
     */
    SQLUpdate(const char* table_name, SQLImplementation *impl);
  
    virtual void begin_action();
    virtual void end_action();
    
    /**
     * Since update doesn't modify the object, define a variant of
     * action() that operates on a const SerializableObject.
     */
    using SerializeAction::action;
    int action(const SerializableObject* const_object)
    {
        return(SerializeAction::action((SerializableObject*)const_object));
    }
        
    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, int32_t* i);
    void process(const char* name, int16_t* i);
    void process(const char* name, int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);
    void process(const char* name, std::string* s);
};

/**
 * SQLTableFormat is a SerializeAction that builts up a SQL "CREATE
 * TABLE" query statement based on the values in an object.
 */
class SQLTableFormat : public SQLQuery {
public:
    /**
     * Constructor.
     */
    SQLTableFormat(const char* table_name, SQLImplementation *impl);
    
    virtual void begin_action();
    virtual void end_action();
    
    /**
     * Since table format doesn't modify the object, define a variant
     * of action() that operates on a const SerializableObject.
     */
    using SerializeAction::action;
    int action(const SerializableObject* const_object)
    {
        return(SerializeAction::action((SerializableObject*)const_object));
    }
        
    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    void process(const char* name,  SerializableObject* object);
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);
    void process(const char* name, std::string* s);

 protected:
    void append(const char* name, const char* type);
    StringBuffer column_prefix_;
};

/**
 * SQLExtract is a SerializeAction that constructs an object's
 * internals from the results of a SQL "select" statement.
 */
class SQLExtract : public SerializeAction {
public:
    SQLExtract(SQLImplementation *impl);
    
    // get the next field from the db
    const char* next_field() ;

    // Virtual functions inherited from SerializeAction
    using SerializeAction::process;
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, u_int32_t len);
    void process(const char* name, u_char** bp, u_int32_t* lenp, int flags);
    void process(const char* name, std::string* s); 

 protected:
    int field_;		///< counter over the fields in the returned tuple
    
 private:
    SQLImplementation *sql_impl_;
};

} // namespace oasys

#endif /* _OASYS_SQL_SERIALIZE_H_ */
