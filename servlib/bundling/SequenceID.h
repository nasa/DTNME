/*
 *    Copyright 2008 Intel Corporation
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

#ifndef _SEQUENCE_ID_H_
#define _SEQUENCE_ID_H_

#include <oasys/debug/Formatter.h>
#include "../naming/EndpointID.h"

namespace dtn {

/**
 * A bundle SequenceID is a version vector of (EID, counter) and/or
 * (EID, identifier) tuples.
 */
class SequenceID : public oasys::Formatter {
public:
    SequenceID();

    /**
     * Type variable for whether an entry in the vector is a counter
     * or an unordered identifier.
     */
    typedef enum {
        EMPTY      = 0,
        COUNTER    = 1,
        IDENTIFIER = 2
    } type_t;
    
    /**
     * Valid return values from compare().
     */
    typedef enum {
        LT   = -1,               ///< Less than
        EQ   = 0,                ///< Equal to
        GT   = 1,                ///< Greater than
        NEQ  = 2,                ///< Incomparable
        ILL  = 999		 ///< Illegal state
    } comp_t;

    /// Pretty printer for comp_t.
    static const char* comp2str(comp_t eq);

    /// Add an (EID, counter) tuple to the vector
    void add(const EndpointID& eid, u_int64_t counter);

    /// Add an (EID, identifier) tuple to the vector
    void add(const EndpointID& eid, const std::string& identifier);

    /**
     * Get the counter for a particular EID.
     * If no entry is found or the found entry is an identifier, returns 0.
     */
    u_int64_t get_counter(const EndpointID& eid) const;
    
    /**
     * Get the identifier for a particular EID. If no entry is found
     * or the found entry is a counter, returns an empty string.
     */
    std::string get_identifier(const EndpointID& eid) const;
    
    /// Virtual from Formatter
    int format(char* buf, size_t sz) const;

    /// Get a string representation
    std::string to_str() const;

    /// Build a sequence id from a string representation. If the value
    /// part contains only numeric characters, it is treated as a
    /// counter, otherwise it is treated as an identifier
    bool parse(const std::string& str);

    /// Compare two vectors, returning a comp_t.
    comp_t compare(const SequenceID& v) const;

    /// @{ Operator overloads
    bool operator<(const SequenceID& v) const  { return compare(v) == LT; }
    bool operator>(const SequenceID& v) const  { return compare(v) == GT; }
    bool operator==(const SequenceID& v) const { return compare(v) == EQ; }

    /** Note! This means not comparable -NOT- not equal to */
    bool operator!=(const SequenceID& v) const { return compare(v) == NEQ; }
    
    bool operator<=(const SequenceID& v) const
    {
        int cmp = compare(v);
        return cmp == LT || cmp == EQ;
    }

    bool operator>=(const SequenceID& v) const
    {
        int cmp = compare(v);
        return cmp == GT || cmp == EQ;
    }
    /// @}

    /// Reset the SequenceID to be a copy of the other
    void assign(const SequenceID& other);
    
    /// Update the sequence id to include the max of all current
    /// entries and the new one.
    void update(const SequenceID& other);

    /// An entry in the vector
    struct Entry {
        Entry()
            : eid_(), type_(EMPTY), counter_(0), identifier_("") {}
        
        Entry(const EndpointID& eid, u_int64_t counter)
            : eid_(eid), type_(COUNTER), counter_(counter), identifier_("") {}
        
        Entry(const EndpointID& eid, const std::string& id)
            : eid_(eid), type_(IDENTIFIER), counter_(0), identifier_(id) {}
        
        Entry(const Entry& other)
            : eid_(other.eid_), type_(other.type_),
              counter_(other.counter_), identifier_(other.identifier_) {}
    
        EndpointID  eid_;
        type_t      type_;
        u_int64_t   counter_;
        std::string identifier_;
    };

    /// Get an entry for the given EID. If no matching entry exists in
    /// the vector, returns a static null entry with counter == 0 and
    /// identifier == ""
    const Entry& get_entry(const EndpointID& eid) const;
    
    /// @{ Typedefs and wrappers for the entry vector and iterators
    typedef std::vector<Entry> EntryVec;
    typedef EntryVec::iterator iterator;
    typedef EntryVec::const_iterator const_iterator;

    bool                     empty() const { return vector_.empty(); }
    EntryVec::iterator       begin()       { return vector_.begin(); }
    EntryVec::iterator       end()         { return vector_.end();   }
    EntryVec::const_iterator begin() const { return vector_.begin(); }
    EntryVec::const_iterator end()   const { return vector_.end();   }
    /// @}

private:
    /// The entry vector
    EntryVec vector_;

    /// Compare two entries
    static comp_t compare_entries(const Entry& left, const Entry& right);
    
    /// Compare vectors in a single direction
    static comp_t compare_one_way(const SequenceID& lv,
                                  const SequenceID& rv,
                                  comp_t cur_state);
};

} // namespace dtn

#endif /* _SEQUENCE_ID_H_ */
