/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include <cstring>
#include <string>
#include <oasys/compat/inttypes.h>
#include <oasys/util/StringBuffer.h>

#include "KeyDB.h"
#include "Ciphersuite.h"
#include "Ciphersuite_BA.h"

template <>
dtn::KeyDB* oasys::Singleton<dtn::KeyDB, false>::instance_ = NULL;

namespace dtn {

static const char * log = "/dtn/bundle/security";

KeyDB::KeyDB()
{
}

KeyDB::~KeyDB()
{
}

void
KeyDB::init()
{
    if (instance_ != NULL) 
    {
        PANIC("KeyDB already initialized");
    }
    
    instance_ = new KeyDB();
	log_debug_p(log, "KeyDB::init() done");
}

void KeyDB::shutdown()
{
    if(instance_ != NULL && !getenv("OASYS_CLEANUP_SINGLETONS")) 
    {
        delete instance_;
        log_debug_p(log, "KeyDB::shutdown() completed");
    }
}

void
KeyDB::set_key(Entry& entry)
{
    EntryList& keys = instance()->keys_;
    EntryList::iterator iter;

    for (iter = keys.begin(); iter != keys.end(); iter++)
    {
        if (iter->match(entry)) {
            *iter = entry;
            return;
        }
    }
    
    // if we get here then there was no existing matching entry;
    // insert the new entry just before any existing wildcard entries
    for (iter = keys.begin();
         iter != keys.end() && iter->host() != std::string("*");
         iter++)
    {
    }
    keys.insert(iter, entry);
	log_debug_p(log, "KeyDB::set_key() done");
}

const KeyDB::Entry*
KeyDB::find_key(const char* host, u_int16_t cs_num)
{
    EntryList& keys = instance()->keys_;
    EntryList::iterator iter;
	log_debug_p(log, "KeyDB::find_key()");

    for (iter = keys.begin(); iter != keys.end(); iter++)
    {
        if (iter->match_wildcard(host, cs_num)) {
            oasys::StringBuffer *buf;
            buf = new oasys::StringBuffer();
            iter->dump(buf);
            log_debug_p(log, "KeyDB::find_key returning %s",buf->c_str());
            delete buf;
            return &(*iter);
        }
    }
    
    // not found
    return NULL;
}

void
KeyDB::del_key(const char* host, u_int16_t cs_num)
{
    EntryList& keys = instance()->keys_;
    EntryList::iterator iter;
	log_debug_p(log, "KeyDB::del_key()");

    for (iter = keys.begin(); iter != keys.end(); iter++)
    {
        if (iter->match(host, cs_num)) {
            keys.erase(iter);
            return;
        }
    }

    // if not found, then nothing to do
}

#ifdef LTPUDP_AUTH_ENABLED
const KeyDB::Entry*
KeyDB::find_key(const char* engine_id, u_int16_t cs_num, u_int32_t cs_key_id)
{
    string ltp_engine;
    EntryList& keys = instance()->keys_;
    EntryList::iterator iter;
    log_debug_p(log, "KeyDB::find_key()");

    std::stringstream stream;
    stream << cs_key_id;
    std::string result = stream.str();
    
    ltp_engine = std::string("LTP:") + std::string(engine_id) + std::string(":") + result;

    for (iter = keys.begin(); iter != keys.end(); iter++)
    {
        if (iter->match_wildcard(ltp_engine.c_str(), cs_num)) {
            oasys::StringBuffer *buf;
            buf = new oasys::StringBuffer();
            iter->dump(buf);
            delete buf;
            return &(*iter);
        }
    }
    
    // not found
    return NULL;
}

void
KeyDB::del_key(const char* engine_id, u_int16_t cs_num, u_int32_t cs_key_id)
{
    string ltp_engine;
    EntryList& keys = instance()->keys_;
    EntryList::iterator iter;
    log_debug_p(log, "KeyDB::del_key()");

    std::stringstream stream;
    stream << cs_key_id;
    std::string result = stream.str();

    ltp_engine = std::string("LTP:") + std::string(engine_id) + std::string(":") + result;

    for (iter = keys.begin(); iter != keys.end(); iter++)
    {
        if (iter->match(ltp_engine.c_str(), cs_num)) {
            keys.erase(iter);
            return;
        }
    }

    // if not found, then nothing to do
}
#endif // LTPUDP_AUTH_ENABLED

void
KeyDB::flush_keys()
{
	log_debug_p(log, "KeyDB::flush_keys()");
    instance()->keys_.clear();
}

/// Dump the current contents of the KeyDB in human-readable form.
void
KeyDB::dump(oasys::StringBuffer* buf)
{
    EntryList& keys = instance()->keys_;
    EntryList::iterator iter;

    for (iter = keys.begin(); iter != keys.end(); iter++)
        iter->dump(buf);
}

/// Dump a human-readable header for the output of dump().
void
KeyDB::dump_header(oasys::StringBuffer* buf)
{
    KeyDB::Entry::dump_header(buf);
}

/// Validate the specified ciphersuite number (see if it corresponds
/// to a registered BAB ciphersuite).
bool
KeyDB::validate_cs_num(u_int16_t cs_num)
{
    return (Ciphersuite::find_suite(cs_num) != NULL) && (Ciphersuite::config->get_block_type(cs_num) == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK);
}

bool
KeyDB::validate_key_len(u_int16_t cs_num, size_t* key_len)
{
    if(Ciphersuite::config->get_block_type(cs_num) != BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
        return false;
    }
    Ciphersuite_BA* cs =
        dynamic_cast<Ciphersuite_BA*>(Ciphersuite::find_suite(cs_num));
    if (cs == NULL)
        return false;
    if (*key_len != cs->result_len()) {
        *key_len = cs->result_len();
        return false;
    }
    return true;
}

/// Detailed constructor.
KeyDB::Entry::Entry(const char* host, u_int16_t cs_num, const u_char* key,
                    size_t key_len)
    : host_(host),
      cs_num_(cs_num),
      key_len_(key_len)
{
    ASSERT(key != NULL);
    ASSERT(key_len != 0);

    key_ = new u_char[key_len];
    memcpy(key_, key, key_len);
}

/// Default constructor.
KeyDB::Entry::Entry()
    : host_(""),
      cs_num_(0),
      key_(NULL),
      key_len_(0)
{
}

/// Copy constructor.
KeyDB::Entry::Entry(const Entry& other)
    : host_(other.host_),
      cs_num_(other.cs_num_),
      key_len_(other.key_len_)
{
    if (other.key_ == NULL)
        key_ = NULL;
    else {
        key_ = new u_char[other.key_len_];
        memcpy(key_, other.key_, other.key_len_);
    }
}

/// Destructor.
KeyDB::Entry::~Entry()
{
    if (key_ != NULL) {
        memset(key_, 0, key_len_);
        delete[] key_;
        key_len_ = 0;
    } else
        ASSERT(key_len_ == 0);
}

/// Assignment operator.
void
KeyDB::Entry::operator=(const Entry& other)
{
    if (key_ != NULL) {
        memset(key_, 0, key_len_);
        delete[] key_;
    }

    host_   = other.host_;
    cs_num_ = other.cs_num_;

    if (other.key_ == NULL)
        key_ = NULL;
    else {
        key_ = new u_char[other.key_len_];
        memcpy(key_, other.key_, other.key_len_);
    }

    key_len_ = other.key_len_;
}

/// Determine if two entries have the same host and ciphersuite
/// number.
bool
KeyDB::Entry::match(const Entry& other) const
{
    return (host_ == other.host_ && cs_num_ == other.cs_num_);
}

/// Determine if this entry matches the given host and ciphersuite
/// number.
bool
KeyDB::Entry::match(const char* host, u_int16_t cs_num) const
{
    return (host_ == std::string(host) && cs_num_ == cs_num);
}

/// Same as match(), but also matches wildcard entries (where host
/// is "*").
bool
KeyDB::Entry::match_wildcard(const char* host, u_int16_t cs_num) const
{
    return ((host_ == std::string(host) || host_ == std::string("*"))
            && cs_num_ == cs_num);
}

/// Dump this entry to a StringBuffer in human-readable form.
void
KeyDB::Entry::dump(oasys::StringBuffer* buf) const
{
    buf->appendf("%15s   0x%03x  ", host_.c_str(), cs_num_);
    //buf->appendf("(-- not shown --)");
    for (int i = 0; i < (int)key_len_; i++)
        buf->appendf("%02x", (int)key_[i]);
    buf->appendf("\n");
}

/// Dump a human-readable header for the output of dump().
void
KeyDB::Entry::dump_header(oasys::StringBuffer* buf)
{
    buf->appendf("host              cs_num   key\n"                );
    buf->appendf("---------------   ------   -------------------\n");
}

} // namespace dtn

#endif  /* BSP_ENABLED */
