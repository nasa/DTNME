/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _ENDPOINT_ID_H_
#define _ENDPOINT_ID_H_

#include <string>
#include <oasys/serialize/Serialize.h>
#include <oasys/serialize/SerializableVector.h>
#include <oasys/util/URI.h>

struct dtn_endpoint_id_t;

namespace dtn {

typedef oasys::URI URI;
        
class EndpointID;
class EndpointIDPattern;
class Scheme;

class EndpointID : public oasys::SerializableObject {
public:
    /**
     * Default constructor
     */
    EndpointID() : scheme_(NULL), valid_(false), is_pattern_(false) {}

    /**
     * Constructor for deserialization.
     */
    EndpointID(const oasys::Builder&)
        : scheme_(NULL), valid_(false), is_pattern_(false) {}

    /**
     * Construct the endpoint id from the given string.
     */
    EndpointID(const std::string& str)
        : uri_(str), scheme_(NULL), valid_(false), is_pattern_(false)
    {
        validate();
    }

    /**
     * Construct the endpoint id from the given string.
     */
    EndpointID(const char* str, size_t len)
        : uri_(), scheme_(NULL), valid_(false), is_pattern_(false)
    {
        uri_.assign(str, len);
        validate();
    }
    /**
     * Construct the endpoint id from another.
     */
    EndpointID(const EndpointID& other)
        : SerializableObject(other)
    {
        if (&other != this)
            assign(other);
    }

    /**
     * Destructor.
     */
    virtual ~EndpointID() {}

    /**
     * Assign this endpoint ID as a copy of the other.
     */
    bool assign(const EndpointID& other)
    {
        uri_         = other.uri_;
        scheme_      = other.scheme_;
        valid_       = other.valid_;
        is_pattern_  = other.is_pattern_;
        return true;
    }
        
    /**
     * Set the string and validate it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const std::string& str)
    {
        uri_.assign(str);
        return validate();
    }

    /**
     * Set the string and validate it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const char* str, size_t len)
    {
        uri_.assign(str, len);
        return validate();
    }

    /**
     * Set the string from component pieces and validate it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const std::string& scheme, const std::string& ssp)
    {
        uri_.assign(scheme + ":" + ssp);
        return validate();
    }

    /**
     * Simple equality test function
     */
    bool equals(const EndpointID& other) const
    {
        return uri_ == other.uri_;
    }

    /**
     * Operator overload for equality syntactic sugar
     */
    bool operator==(const EndpointID& other) const
    {
        return uri_ == other.uri_;
    }
    
    /**
     * Operator overload for inequality syntactic sugar
     */
    bool operator!=(const EndpointID& other) const
    {
        return uri_ != other.uri_;
    }

    /**
     * Operator overload for STL comparison-based data structures
     * (such as a std::map).
     */
    bool operator<(const EndpointID& other) const
    {
        return uri_ < other.uri_;
    }

    /**
     * Three way lexographical comparison
     */
    int compare(const EndpointID& other) const
    {
        return uri_.compare(other.uri_);
    }

    /**
     * Set the string from the API type dtn_endpoint_id_t
     *
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const dtn_endpoint_id_t* eid);

    /**
     * @ return true if the given EndpointID is contained within
     *   this EndpointID; otherwise false.
     */
    bool subsume(const EndpointID& other) const
             { return uri_.subsume(other.uri_); }

    /**
     * Append the specified service tag (in a scheme-specific manner)
     * to the ssp.
     *
     * @return true if successful, false if the scheme doesn't support
     * service tags
     */
    bool append_service_tag(const char* tag);

    /**
     * Append a wildcard (in a scheme-specific manner) to form a route 
     * pattern.
     *
     * @return true if successful, false if the scheme doesn't support
     * wildcards
     */
    bool append_service_wildcard();

    /**
     * Reduce EndpointID to routing endpoint
     *
     * @return true if eid is set to node_id, false otherwise
     */
    bool remove_service_tag();

    /**
     * Typedef for the return value possibilities from is_singleton.
     */
    typedef enum { UNKNOWN, SINGLETON, MULTINODE } singleton_info_t;
    
    /**
     * Return whether or not this endpoint id is a singleton or a
     * multi-node endpoint.
     */
    singleton_info_t is_singleton() const;

    /**
     * Default setting for endpoint ids in unknown schemes.
     */
    static singleton_info_t is_singleton_default_;

    /**
     * Bit to control how to match unknown schemes.
     */
    static bool glob_unknown_schemes_;

    /**
     * Copy the endpoint id contents out to the API type
     * dtn_endpoint_id_t.
     */
    void copyto(dtn_endpoint_id_t* eid) const;

    /**
     * Return an indication of whether or not the scheme is known.
     */
    bool known_scheme() const
    {
        return (scheme_ != NULL);
    }

    /**
     * Return the special endpoint id used for the null endpoint,
     * namely "dtn:none".
     */
    inline static const EndpointID& NULL_EID();
    
    /**
     * The scheme and SSP parts each must not exceed this length.
     */
    static const size_t MAX_EID_PART_LENGTH = 1023;

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(oasys::SerializeAction* a);
    
    /// @{
    /// Accessors and wrappers around the various fields.
    ///
    const URI&         uri()        const { return uri_; }
    const std::string& str()        const { return uri_.uri(); }
    const std::string  scheme_str() const { return uri_.scheme(); }
    const std::string  ssp()        const { return uri_.ssp(); }
    Scheme*            scheme()     const { return scheme_; }
    bool               valid()      const { return valid_; }
    bool               is_pattern() const { return is_pattern_; }
    const char*        c_str()      const { return uri_.uri().c_str(); } 
    const char*        data()       const { return uri_.uri().data(); }
    size_t             length()     const { return uri_.uri().length(); }
    ///@}

protected:
    /**
     * Extract and look up the scheme and ssp.
     *
     * @return true if the string is a valid endpoint id, false if not.
     */
    bool validate();

    URI uri_;                   /* endpoint URI */

    Scheme* scheme_;            /* the scheme class (if known) */

    bool valid_;                /* true iff the endpoint id is valid */
    bool is_pattern_;           /* true iff this is an EndpointIDPattern */
};

/**
 * A Distinct class for endpoint patterns (i.e. those containing some
 * form of wildcarding) as opposed to basic endpoint IDs to help keep
 * it straight in the code.
 */
class EndpointIDPattern : public EndpointID {
public:
    /**
     * Default constructor
     */
    EndpointIDPattern() : EndpointID()
    {
        is_pattern_ = true;
        uri_.set_validate(false);
    }

    /**
     * Construct the endpoint id pattern from the given string.
     */
    EndpointIDPattern(const std::string& str) : EndpointID()
    {
        is_pattern_ = true;
        uri_.set_validate(false);
        assign(str);
    }

    /**
     * Construct the endpoint id pattern from another.
     */
    EndpointIDPattern(const EndpointIDPattern& other)
         : EndpointID(other) {}

    /**
     * Construct the endpoint id pattern from another that is not
     * necessarily a pattern.
     */
    EndpointIDPattern(const EndpointID& other) : EndpointID(other)
    {
        is_pattern_ = true;
        uri_.set_validate(false);
        validate();
    }

    /**
     * Shortcut to the matching functionality implemented by the
     * scheme.
     */
    bool match(const EndpointID& eid) const;

    /**
     * Return the special wildcard Endpoint ID. This functionality is
     * not in the bundle spec, but is used internally to this
     * implementation.
     */
    inline static const EndpointIDPattern& WILDCARD_EID();
};

/**
 * A (serializable) vector of endpoint ids.
 */
class EndpointIDVector : public oasys::SerializableVector<EndpointID> {};

/**
 * Helper class to store the global EIDs so that printing an
 * EndpointID within gdb doesn't result in an infinite recursion.
 */
struct GlobalEndpointIDs {
    static EndpointID        null_eid_;
    static EndpointIDPattern wildcard_eid_;
};

//----------------------------------------------------------------------
inline const EndpointID&
EndpointID::NULL_EID()
{
    if (GlobalEndpointIDs::null_eid_.scheme_ == NULL) {
        GlobalEndpointIDs::null_eid_.assign("dtn:none");
    }
    return GlobalEndpointIDs::null_eid_;
}

//----------------------------------------------------------------------
inline const EndpointIDPattern&
EndpointIDPattern::WILDCARD_EID()
{
    if (GlobalEndpointIDs::wildcard_eid_.scheme_ == NULL) {
        GlobalEndpointIDs::wildcard_eid_.assign("*:*");
    }
    
    return GlobalEndpointIDs::wildcard_eid_;
}

} // namespace dtn

#endif /* _ENDPOINT_ID_H_ */
