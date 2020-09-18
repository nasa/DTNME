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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <ctype.h>
#include <oasys/util/SafeRange.h>
#include <oasys/util/StringAppender.h>
#include <oasys/util/StringBuffer.h>
#include "SequenceID.h"

namespace dtn {

//----------------------------------------------------------------------
SequenceID::SequenceID()
{
}

//----------------------------------------------------------------------------
const char* 
SequenceID::comp2str(comp_t eq)
{
    switch(eq) {
    case LT:  return "less than";
    case GT:  return "greater than";
    case EQ:  return "equal to";
    case NEQ: return "uncomparable to";
    default:  NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
SequenceID::add(const EndpointID& eid, u_int64_t counter)
{
    vector_.push_back(Entry(eid, counter));
}

//----------------------------------------------------------------------
void
SequenceID::add(const EndpointID& eid, const std::string& identifier)
{
    vector_.push_back(Entry(eid, identifier));
}

//----------------------------------------------------------------------
const SequenceID::Entry&
SequenceID::get_entry(const EndpointID& eid) const
{
    for (const_iterator i = begin(); i != end(); ++i) {
        if (i->eid_ == eid) {
            return *i;
        }
    }

    static Entry null_entry;
    return null_entry;
}

//----------------------------------------------------------------------------
u_int64_t
SequenceID::get_counter(const EndpointID& eid) const
{
    return get_entry(eid).counter_;
}

//----------------------------------------------------------------------------
std::string
SequenceID::get_identifier(const EndpointID& eid) const
{
    return get_entry(eid).identifier_;
}

//----------------------------------------------------------------------------
int
SequenceID::format(char* buf, size_t sz) const
{
    oasys::StringAppender sa(buf, sz);

    sa.append('<');
    for (const_iterator i = begin(); i != end(); ++i)
    {
        if (i->type_ == COUNTER) {
            sa.appendf("%s(%s %llu)", 
                       (i == begin()) ? "" : " ",
                       i->eid_.c_str(), 
                       U64FMT(i->counter_));
        } else if (i->type_ == IDENTIFIER) {
            sa.appendf("%s(%s %s)", 
                       (i == begin()) ? "" : " ",
                       i->eid_.c_str(), 
                       i->identifier_.c_str());
        } else {
            NOTREACHED;
        }
    }
    sa.append('>');
    
    return sa.desired_length();
}

//----------------------------------------------------------------------
std::string
SequenceID::to_str() const
{
    oasys::StaticStringBuffer<256> buf;
    buf.appendf("*%p", this);
    return (std::string)buf.c_str();
}

//----------------------------------------------------------------------------
bool
SequenceID::parse(const std::string& str)
{
#define EAT_WS() while (isspace(sr[idx])) { idx++; }
#define MATCH_CHAR(_c) do {                          \
    if (sr[idx] != _c) { goto bad; } else { idx++; } \
} while (0)

    EntryVec old_vec = vector_;
    vector_.clear();

    oasys::SafeRange<const char> sr(str.c_str(), str.size());

    try 
    {
        size_t idx = 0;
        const char *start;
        char *end;
        
        MATCH_CHAR('<');
        EAT_WS();
        while (sr[idx] == '(')
        {
            MATCH_CHAR('(');
            start = &sr[idx];

            do {
                ++idx;
            } while (!isspace(sr[idx]));
            
            EndpointID eid(start, &sr[idx] - start);
            if (! eid.valid()) {
                goto bad;
            }

            EAT_WS();

            bool isnum = true;
            size_t idx2 = idx;
            do {
                // this check should handle the case of having no
                // value part in the entry as well as bogus characters
                if (! (isalnum(sr[idx2]) ||
                       sr[idx2] == '_' ||
                       sr[idx2] == '-' ) )
                {
                    goto bad;
                }
                
                isnum = isnum && isdigit(sr[idx2]);
                ++idx2;
                
            } while (sr[idx2] != ')' && sr[idx2] != '\0');
            
            if (isnum) {
                u_int64_t counter = strtoull(&sr[idx], &end, 10);
                ASSERT(end == &sr[idx2]);
                vector_.push_back(Entry(eid, counter));

            } else {
                std::string identifier(&sr[idx], idx2-idx);
                vector_.push_back(Entry(eid, identifier));
            }
            
            idx = idx2;
            MATCH_CHAR(')');
            EAT_WS();
        }
        
        if (sr[idx] != '>')
        {
            goto bad;
        }

        return true;
    }
    catch (oasys::SafeRange<const char>::Error e)
    { 
        // drop through to bad below
    }

bad:
    vector_.swap(old_vec);
    return false;
    
#undef EAT_WS
#undef MATCH_CHAR
}

//----------------------------------------------------------------------------
SequenceID::comp_t
SequenceID::compare(const SequenceID& other) const
{
    if (empty() && other.empty())
    {
        return EQ;
    }

    // We need to compare the sequence vectors in both directions,
    // since entries that exist in one but not the other are
    // implicitly zero. This approach unnecessarily repeats
    // comparisons but it's not a big deal since the vectors shouldn't
    // be very long.
    
    comp_t cur_state = EQ;
    cur_state = compare_one_way(other, *this, cur_state);
    ASSERT(cur_state != ILL);
    
    if (cur_state != NEQ) 
    {
        // invert the state and call again, this time in the "forward"
        // direction
        switch (cur_state) {
        case LT:   cur_state = GT; break;
        case GT:   cur_state = LT; break;
        case EQ:   cur_state = EQ; break;
        case NEQ:
        case ILL:
            NOTREACHED;
        }
            
        cur_state = compare_one_way(*this, other, cur_state);
    }

    return cur_state;
}

//----------------------------------------------------------------------------
SequenceID::comp_t
SequenceID::compare_one_way(const SequenceID& lv, 
                            const SequenceID& rv,
                            comp_t            cur_state)
{
    // State transition for comparing the vectors
    static comp_t states[4][3] = {
        /* cur_state   cur_comparison */
        /*              LT    EQ    GT  */
        /* LT   */    { LT,   LT,   NEQ }, 
        /* EQ   */    { LT,   EQ,   GT  }, //<- new
        /* GT   */    { NEQ,  GT,   GT  }, //<- state
        /* NEQ  */    { NEQ,  NEQ,  NEQ }
    };
    
// To align the enum (i.e. values -1 to 2) with the correct array
// indices, we have to add one to the index values
#define next_state(_cur_state, _cur_comp) \
    states[(_cur_state) + 1][(_cur_comp) + 1]

    for (const_iterator i = lv.begin(); i != lv.end(); ++i)
    {
        const Entry& left  = lv.get_entry(i->eid_);
        const Entry& right = rv.get_entry(i->eid_);

        cur_state = next_state(cur_state, compare_entries(left, right));

        // once we get into NEQ state we can't ever get out so short
        // circuit the remainder of the comparisons
        if (cur_state == NEQ)
        {
            return NEQ;
        }
    }

    return cur_state;

#undef next_state
}

//----------------------------------------------------------------------------
SequenceID::comp_t
SequenceID::compare_entries(const Entry& left, const Entry& right)
{
    if (left.type_ == COUNTER || left.type_ == EMPTY)
    {
        // if right is an identifier and the left is either a counter
        // or empty, they can't be equal
        if (right.type_ == IDENTIFIER)
        {
            return NEQ;
        }

        if (left.counter_ == right.counter_)
        {
            return EQ;
        }
        else if (left.counter_ < right.counter_)
        {
            return LT;
        }
        else
        {
            return GT;
        }
    }
    else if (left.type_ == IDENTIFIER)
    {
        // in the case of identifiers, the lack of a column entry is
        // not the same thing as an existing column with a value of
        // "", so if the right's type is counter OR empty, return NEQ
        if (right.type_ != IDENTIFIER)
        {
            return NEQ;
        }
        
        if (left.identifier_ != right.identifier_)
        {
            return NEQ;
        }
        else
        {
            return EQ;
        }
    }

    NOTREACHED;
}

//----------------------------------------------------------------------
void
SequenceID::assign(const SequenceID& other)
{
    vector_ = other.vector_;
}

//----------------------------------------------------------------------
void
SequenceID::update(const SequenceID& other)
{
    for (const_iterator other_entry = other.begin();
         other_entry != other.end();
         ++other_entry)
    {
        iterator my_entry = begin();
        while (my_entry != end()) {
            if (other_entry->eid_ == my_entry->eid_)
                break;
            ++my_entry;
        }

        if (my_entry == end())
        {
            if (other_entry->type_ == COUNTER) {
                add(other_entry->eid_, other_entry->counter_);

            } else if (other_entry->type_ == IDENTIFIER) {
                add(other_entry->eid_, other_entry->identifier_);

            } else {
                NOTREACHED;
            }
            
            continue;
        }

        if (other_entry->type_ != my_entry->type_) {
            PANIC("can't update with mismatched types");
        }

        if (other_entry->type_ == COUNTER &&
            other_entry->counter_ > my_entry->counter_)
        {
            my_entry->counter_ = other_entry->counter_;
        }

        if (other_entry->type_ == IDENTIFIER &&
            other_entry->identifier_ != my_entry->identifier_) 
        {
            // always replace
            my_entry->identifier_ = other_entry->identifier_;
        }
    }
}

} // namespace dtn
