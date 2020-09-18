/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

#include <oasys/serialize/SerializableVector.h>
#include <servlib/naming/EndpointID.h>

namespace dtn {

/**
 * Simple data structure to handle a dictionary data structure, as
 * specified by the bundle protocol, i.e. a sequence of null
 * terminated strings.
 */
class Dictionary : public oasys::SerializableObject {
public:
    Dictionary();
    Dictionary(const oasys::Builder&);
    virtual ~Dictionary();
    virtual void serialize( oasys::SerializeAction *a );
    
    /// @{ Accessors
    u_int32_t     length() const { return length_; }
    const u_char* dict()   const { return dict_; }
    void set_dict(const u_char* dict, u_int32_t length);
    /// @}

    /**
     * Add the given string to the dictionary if it doesn't already
     * exist.
     */
    void add_str(const std::string& str);

    /**
     * Add the scheme and ssp of the given endpoint id to the dictionary.
     */
    void add_eid(const EndpointID& eid)
    {
        add_str(eid.scheme_str());
        add_str(eid.ssp());
    }

    /**
     * Look up the given string in the dictionary, returning true and
     * assigning the offset if found.
     */
    bool get_offset(const std::string& str, u_int32_t* offset);

    bool get_offset(const std::string& str, u_int64_t* offset);

    /**
     * Look up the given eid in the dictionary, returning true and
     * assigning the offsets if found.
     */
    bool get_offsets(const EndpointID& eid,
                     u_int32_t* scheme_offset,
                     u_int32_t* ssp_offset)
    {
        return (get_offset(eid.scheme_str(), scheme_offset) &&
                get_offset(eid.ssp(), ssp_offset));
    }

    bool get_offsets(const EndpointID& eid,
                     u_int64_t* scheme_offset,
                     u_int64_t* ssp_offset)
    {
        return (get_offset(eid.scheme_str(), scheme_offset) &&
                get_offset(eid.ssp(), ssp_offset));
    }

    /**
     * Create an eid from the dictionary, given the offsets.
     * Return true upon success.
     */
    bool extract_eid(EndpointID* eid,
                     u_int32_t scheme_offset,
                     u_int32_t ssp_offset);
    
    bool extract_eid(EndpointID* eid,
                     u_int64_t scheme_offset,
                     u_int64_t ssp_offset);
    
protected:
    u_char*   dict_;		///< Dictionary buffer
    u_int32_t dict_length_;	///< Length of the dictionary buffer
    u_int32_t length_;		///< Length of the filled-in portion
};

} // namespace dtn

#endif /* _DICTIONARY_H_ */
