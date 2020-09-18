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

#ifndef _KEYDB_H_
#define _KEYDB_H_

#ifdef BSP_ENABLED

#include <list>
#include <string>
#include <oasys/compat/inttypes.h>
#include <oasys/util/Singleton.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

class KeyDB : public oasys::Singleton<KeyDB, false> {
public:

    /**
     * Nested class used to provide storage for key bytes.
     */
    class Entry;

    /**
     * Constructor (called at startup).
     */
    KeyDB();

    /**
     * Destructor (called at shutdown).
     */
    ~KeyDB();

    /**
     * Startup-time initializer.
     */
    static void init();


    static void shutdown();
    /**
     * Set the key for a given host and ciphersuite type.  This will
     * overwrite any existing entry for the same host/ciphersuite.
     *
     * XXX Eventually we'd probably want to index by KeyID too, which
     * is how we'd distinguish between multiple keys for the same
     * host.
     */
    static void set_key(Entry& entry);

    /**
     * Find the key for a given host and ciphersuite type.
     *
     * @return A pointer to the matching Entry, or NULL if no
     * match is found.
     */
    static const Entry* find_key(const char* host, u_int16_t cs_num);
    
    /**
     * Delete the key entry for the given host and ciphersuite type.
     */
    static void del_key(const char* host, u_int16_t cs_num);

#ifdef LTPUDP_AUTH_ENABLED
    /**
     * Find the LTPUDP AUTH key for a given host, LTP ciphersuite number and key ID
     *
     * @return A pointer to the matching Entry, or NULL if no
     * match is found.
     */
    static const Entry* find_key(const char* host, u_int16_t cs_num, u_int32_t cs_key_id);
    
    /**
     * Delete the key entry for the given host and ciphersuite type.
     * Delete the LTPUDP AUTH key for a given host, LTP ciphersuite number and key ID
     */
    static void del_key(const char* host, u_int16_t cs_num, u_int32_t cs_key_id);
#endif // LTPUDP_AUTH_ENABLED


    /**
     * Delete all key entries (including wildcard entries).
     */
    static void flush_keys();

    /**
     * Dump the contents of the KeyDB in human-readable form.
     */
    static void dump(oasys::StringBuffer* buf);

    /**
     * Dump a human-readable header for the output of dump().
     */
    static void dump_header(oasys::StringBuffer* buf);

    /**
     * Validate the specified ciphersuite number (see if it
     * corresponds to a registered BAB ciphersuite).
     */
    static bool validate_cs_num(u_int16_t cs_num);

    /**
     * Validate that the specified key length matches what's expected
     * for the specified ciphersuite.  Stores the expected length back
     * into the key_len argument if it's wrong.
     */
    static bool validate_key_len(u_int16_t cs_num, size_t* key_len);

private:

    typedef std::list<Entry> EntryList;

    EntryList keys_;

};

class KeyDB::Entry {

public:

    /**
     * Constructor.
     *
     * @param host     Name of host for whom this key should be used.
     * @param cs_num   Ciphersuite number this key is to be used with.
     * @param key      Key data.
     * @param key_len  Length of key data (in bytes).
     */
    Entry(const char* host, u_int16_t cs_num, const u_char* key,
          size_t key_len);

    /**
     * Default constructor.
     */
    Entry();

    /**
     * Copy constructor.
     */
    Entry(const Entry& other);

    /**
     * Destructor.
     */
    ~Entry();

    /**
     * Assignment operator.
     */
    void operator=(const Entry& other);

    /**
     * Determine if two entries have the same host and ciphersuite
     * number.
     */
    bool match(const Entry& other) const;

    /**
     * Determine if this entry matches the given host and ciphersuite
     * number.
     */
    bool match(const char* host, u_int16_t cs_num) const;

    /**
     * Same as match(), but also matches wildcard entries (where host
     * is "*").
     */
    bool match_wildcard(const char* host, u_int16_t cs_num) const;

    /// @{ Non-mutating accessors.
    const std::string& host()     const { return host_; }
    u_int16_t          cs_num()   const { return cs_num_; }
    const u_char*      key()      const { return key_; }
    size_t             key_len()  const { return key_len_; }
    /// @}

    /**
     * Dump this entry to a StringBuffer in human-readable form.
     */
    void dump(oasys::StringBuffer* buf) const;

    /**
     * Dump a human-readable header for the output of dump().
     */
    static void dump_header(oasys::StringBuffer* buf);

private:
    std::string host_;
    u_int16_t   cs_num_;
    u_char*     key_;
    size_t      key_len_;
};

} // namespace dtn

#endif  /* BSP_ENABLED */

#endif  /* _KEYDB_H_ */
