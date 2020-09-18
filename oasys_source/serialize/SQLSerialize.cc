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
#  include <oasys-config.h>
#endif

#include "SQLSerialize.h"
#include "SQLImplementation.h"

#include "debug/DebugUtils.h"
#include "debug/Log.h"
#include "util/StringUtils.h"

namespace oasys {

/******************************************************************************
 *
 * SQLQuery
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLQuery::SQLQuery(action_t type, const char* table_name,
                   SQLImplementation* impl, const char* initial_query)
    
    : SerializeAction(type, Serialize::CONTEXT_LOCAL),
      table_name_(table_name),
      sql_impl_(impl),
      query_(256, initial_query)
{
}

/******************************************************************************
 *
 * SQLInsert
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLInsert::SQLInsert(const char* table_name, SQLImplementation* impl)
    : SQLQuery(Serialize::MARSHAL, table_name, impl)
{
}


// Virtual functions inherited from SerializeAction

/**
 * Initialize the query buffer.
 */
void 
SQLInsert::begin_action() 
{
    query_.appendf("INSERT INTO %s  VALUES(",table_name_);
}

/**
 * Clean the query in the end, trimming the trailing ',' and adding a
 * closing parenthesis.
 */
void 
SQLInsert::end_action() 
{
    if (query_.data()[query_.length() - 1] == ',') {
        query_.data()[query_.length() - 1] = ')';
    }
}


void 
SQLInsert::process(const char* name, u_int32_t* i)
{
    (void)name;
    query_.appendf("%u,", *i);
}

void 
SQLInsert::process(const char* name, u_int16_t* i)
{
    (void)name;
    query_.appendf("%u,", *i);
}

void 
SQLInsert::process(const char* name, u_int8_t* i)
{
    (void)name;
    query_.appendf("%u,", *i);
}

void 
SQLInsert::process(const char* name, int32_t* i)
{
    (void)name;
#ifdef __CYGWIN__
    query_.appendf("%ld,", *i);
#else
    query_.appendf("%d,", *i);
#endif
}

void 
SQLInsert::process(const char* name, int16_t* i)
{
    (void)name;
    query_.appendf("%d,", *i);
}

void 
SQLInsert::process(const char* name, int8_t* i)
{
    (void)name;
    query_.appendf("%d,", *i);
}

void 
SQLInsert::process(const char* name, bool* b)
{
    (void)name;
    if (*b) {
        query_.append("'TRUE',");
    } else {
        query_.append("'FALSE',");
    }
}


void 
SQLInsert::process(const char* name, std::string* s) 
{
    (void)name;
    query_.appendf("'%s',", sql_impl_->escape_string(s->c_str()));
}

void 
SQLInsert::process(const char* name, u_char* bp, u_int32_t len)
{
    (void)name;
    query_.appendf("'%s',", sql_impl_->escape_binary(bp, len));
}

void 
SQLInsert::process(const char* name, u_char** bp, u_int32_t* lenp, int flags)
{
    (void)name;
    (void)bp;
    (void)lenp;
    (void)flags;
    NOTIMPLEMENTED;
}

/******************************************************************************
 *
 * SQLUpdate
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLUpdate::SQLUpdate(const char* table_name, SQLImplementation* impl)
    : SQLQuery(Serialize::MARSHAL, table_name, impl)
{
}


// Virtual functions inherited from SerializeAction
void 
SQLUpdate::begin_action() 
{
    query_.appendf("UPDATE %s SET ",table_name_);
}

void 
SQLUpdate::end_action() 
{
    if (query_.data()[query_.length() - 2] == ',') {
        query_.data()[query_.length() - 2] =  ' ';
    }
}

void 
SQLUpdate::process(const char* name, u_int32_t* i)
{
    (void)name;
    query_.appendf("%s = %u, ", name, *i);
}

void 
SQLUpdate::process(const char* name, u_int16_t* i)
{
    (void)name;
    query_.appendf("%s = %u, ", name, *i);
}

void 
SQLUpdate::process(const char* name, u_int8_t* i)
{
    (void)name;
    query_.appendf("%s = %u, ", name, *i);
}

void 
SQLUpdate::process(const char* name, int32_t* i)
{
    (void)name;
#ifdef __CYGWIN__
    query_.appendf("%s = %ld, ", name, *i);
#else
    query_.appendf("%s = %d, ", name, *i);
#endif
}

void 
SQLUpdate::process(const char* name, int16_t* i)
{
    (void)name;
    query_.appendf("%s = %d, ", name, *i);
}

void 
SQLUpdate::process(const char* name, int8_t* i)
{
    (void)name;
    query_.appendf("%s = %d, ", name, *i);
}

void 
SQLUpdate::process(const char* name, bool* b)
{
    (void)name;
    if (*b) {
        query_.appendf("%s = 'TRUE', ", name);
    } else {
        query_.appendf("%s = 'FALSE', ", name);
    }
}


void 
SQLUpdate::process(const char* name, std::string* s) 
{
    (void)name;
    query_.appendf("%s = '%s', ", name, sql_impl_->escape_string(s->c_str()));
}

void 
SQLUpdate::process(const char* name, u_char* bp, u_int32_t len)
{
    (void)name;
    query_.appendf("%s = '%s', ", name, sql_impl_->escape_binary(bp, len));
}

void 
SQLUpdate::process(const char* name, u_char** bp, u_int32_t* lenp, int flags)
{
    (void)name;
    (void)bp;
    (void)lenp;
    (void)flags;
    NOTIMPLEMENTED;
}

/******************************************************************************
 *
 * SQLTableFormat
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLTableFormat::SQLTableFormat(const char* table_name,
                               SQLImplementation* impl)
    : SQLQuery(Serialize::INFO, table_name, impl)
{
}


// Virtual functions inherited from SerializeAction

void 
SQLTableFormat::begin_action() 
{
    query_.appendf("CREATE TABLE  %s  (",table_name_);
}

void 
SQLTableFormat::end_action() 
{
    if (query_.data()[query_.length() - 1] == ',') {
        query_.data()[query_.length() - 1] =  ')';
    }
}


void
SQLTableFormat::process(const char* name,  SerializableObject* object) 
{
    int old_len = column_prefix_.length();

    column_prefix_.appendf("%s__", name);
    object->serialize(this);
    column_prefix_.trim(column_prefix_.length() - old_len);
}

void 
SQLTableFormat::append(const char* name, const char* type)
{
    query_.appendf("%.*s%s %s,",
                   (int)column_prefix_.length(), column_prefix_.data(),
                   name, type);
}

// Virtual functions inherited from SerializeAction
void 
SQLTableFormat::process(const char* name, u_int32_t* i)
{
    (void)i;
    append(name, "BIGINT");
}

void 
SQLTableFormat::process(const char* name, u_int16_t* i)
{
    (void)i;
    append(name, "INTEGER");
}

void
SQLTableFormat::process(const char* name, u_int8_t* i) {

    (void)i;
    append(name, "SMALLINT");
}
 

// Mysql and Postgres do not have common implementation of bool
// XXX/demmer fix this
void 
SQLTableFormat::process(const char* name, bool* b)
{
    (void)b;
//    append(name, "BOOLEAN");
    append(name, "TEXT");
}

void 
SQLTableFormat::process(const char* name, std::string* s) 
{
    (void)s;
    append(name, "TEXT");
}

void 
SQLTableFormat::process(const char* name, u_char* bp, u_int32_t len)
{
    (void)bp;
    (void)len;
    append(name,sql_impl_->binary_datatype());
}
    
void 
SQLTableFormat::process(const char* name, u_char** bp, u_int32_t* lenp, int flags)
{
    (void)bp;
    (void)lenp;
    (void)flags;

    if (flags & Serialize::NULL_TERMINATED) {
        NOTIMPLEMENTED;
    }

    append(name,sql_impl_->binary_datatype());
}

/******************************************************************************
 *
 * SQLExtract
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLExtract::SQLExtract(SQLImplementation* impl)
    : SerializeAction(Serialize::UNMARSHAL, 
                      Serialize::CONTEXT_LOCAL)
{
    field_ = 0;
    sql_impl_ = impl;
}

const char* 
SQLExtract::next_field()
{
    return sql_impl_->get_value(0, field_++);
}

void
SQLExtract::process(const char* name, u_int32_t* i)
{
    (void)name;
    const char* buf = next_field();
    if (buf == NULL) return;
    
    *i = atoi(buf) ; 

    if (log_) logf(log_, LOG_DEBUG, "<=int32(%d)", *i);
}

void 
SQLExtract::process(const char* name, u_int16_t* i)
{
    (void)name;
    const char* buf = next_field();
    if (buf == NULL) return;

    *i = atoi(buf) ; 
    
    if (log_) logf(log_, LOG_DEBUG, "<=int16(%d)", *i);
}

void 
SQLExtract::process(const char* name, u_int8_t* i)
{
    (void)name;
    const char* buf = next_field();
    if (buf == NULL) return;
    
    *i = buf[0];
    
    if (log_) logf(log_, LOG_DEBUG, "<=int8(%d)", *i);
}

void 
SQLExtract::process(const char* name, bool* b)
{
    (void)name;
    const char* buf = next_field();

    if (buf == NULL) return;

    switch(buf[0]) {
    case 'T':
    case 't':
    case '1':
    case '\1':
        *b = true;
        break;
    case 'F':
    case 'f':
    case '0':
    case '\0':
        *b = false;
        break;
    default:
        logf("/sql", LOG_ERR, "unexpected value '%s' for boolean column", buf);
        signal_error();
        return;
    }
    
    if (log_) logf(log_, LOG_DEBUG, "<=bool(%c)", *b ? 'T' : 'F');
}

void 
SQLExtract::process(const char* name, std::string* s)
{
    (void)name;
    const char* buf = next_field();
    if (buf == NULL) return;
    
    s->assign(buf);
    
    u_int32_t len = s->length();
    
    if (log_) logf(log_, LOG_DEBUG, "<=string(%u: '%.*s')",
                   len, (int)(len < 32 ? len : 32), s->data());
}

void 
SQLExtract::process(const char* name, u_char* bp, u_int32_t len)
{
    (void)name;
    const char* buf = next_field();
    if (buf == NULL) return;
 
    const u_char* v = sql_impl_->unescape_binary((const u_char*)buf);

    memcpy(bp, v, len);
    if (log_) {
        std::string s;
        hex2str(&s, bp, len < 16 ? len : 16);
        logf(log_, LOG_DEBUG, "<=bufc(%u: '%.*s')",
             len, (int)s.length(), s.data());
    }

}


void 
SQLExtract::process(const char* name, u_char** bp, u_int32_t* lenp, int flags)
{
    (void)name;
    (void)bp;
    (void)lenp;
    (void)flags;
    NOTIMPLEMENTED;
}

} // namespace oasys
