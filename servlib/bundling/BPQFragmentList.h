/*
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


#ifndef BPQFRAGMENTLIST_H_
#define BPQFRAGMENTLIST_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include <list>

namespace dtn {


class BPQFragment{
public:
	/**
	 * Constructor
	 */
	BPQFragment(size_t offset , size_t length) :
		offset_(offset),
		length_(length) {}

	/**
	 * Destructor
	 */
    ~BPQFragment() {}

    /// @{ Accessors
    size_t offset() const { return offset_; }
    size_t length() const { return length_; }
    /// @}

private:
    size_t offset_;              ///< Fragment offset
    size_t length_;              ///< Fragment length
};



class BPQFragmentList : public oasys::Logger {
private:
    typedef std::list<BPQFragment*> List;

public:
	typedef List::iterator iterator;
	typedef List::const_iterator const_iterator;


    /**
     * Constructor
     */
	BPQFragmentList(const std::string& name, oasys::SpinLock* lock = NULL);

    /**
     * Destructor -- clears the list.
     */
    ~BPQFragmentList();

    /**
     * Set the name of the list - used for logging
     */
    void set_name(const std::string& name);

    /**
     * Insert the given fragment sorted by offset.
     */
    void insert_sorted(BPQFragment* fragment);

    /**
     * Given that the list is sorted by offset
     * are there any gaps from byte 0 - total_len
     */
    bool is_complete(size_t total_len) const;

    /**
     * Tests if adding a new fragment would be obsolete
     * given the current fragments that are in the list
     * @return	true if the query requires the new fragment
     * 			false if it has already been answered
     */
    bool requires_fragment	(size_t total_len, size_t frag_offset, size_t frag_length) const;

    /**
     * Return the internal lock on this list.
     */
    oasys::SpinLock* lock() const { return lock_; }

    bool		   	empty() const { return list_.empty(); }
    size_t			size() const  { return list_.size();  }
    iterator       	begin()       { return list_.begin(); }
	iterator       	end()         { return list_.end();   }
	const_iterator 	begin() const { return list_.begin(); }
	const_iterator 	end()   const { return list_.end();   }

private:
    /**
     * Deletes all fragments in the list
     */
    void clear();

    std::string      name_;	///< name of the list
    List             list_;	///< underlying list data structure

    oasys::SpinLock* lock_;	///< lock for notifier
    bool             own_lock_; ///< bit to define lock ownership
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* BPQFRAGMENTLIST_H_ */
