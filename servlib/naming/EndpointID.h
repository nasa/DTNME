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

#include <memory>
#include <string>
#include <third_party/oasys/serialize/Serialize.h>
#include <third_party/oasys/serialize/SerializableSPtrVector.h>
#include <third_party/oasys/util/URI.h>

struct dtn_endpoint_id_t;

namespace dtn {

typedef oasys::URI URI;
        
class EndpointID;
class EndpointIDPattern;
class Scheme;

typedef std::shared_ptr<EndpointID>           SPtr_EID;
typedef std::shared_ptr<EndpointIDPattern>    SPtr_EIDPattern;



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
    EndpointID(const EndpointID& other) = delete;

    /**
     * Destructor.
     */
    virtual ~EndpointID() {}

public:
    /**
     * Simple equality test function
     */
    bool equals(const SPtr_EID& sptr_other) const
    {
        return uri_ == sptr_other->uri_;
    }

public:
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
     * Three way lexographical comparison
     */
    int compare(const SPtr_EID& sptr_other) const
    {
        return uri_.compare(sptr_other->uri_);
    }

public:
    /**
     * @ return true if the given EndpointID is contained within
     *   this EndpointID; otherwise false.
     */
    bool subsume(const EndpointID& other) const
             { return uri_.subsume(other.uri_); }
    bool subsume(const SPtr_EID& other) const
             { return uri_.subsume(other->uri_); }

    /**
     * @ return true if the given EndpointID is contained within
     *   this EndpointID; otherwise false.
     */
    bool subsume_ipn(const EndpointID& other) const;
    bool subsume_ipn(const SPtr_EID& other) const;

private:
    friend class EndpointIDPattern;
    friend class IPNScheme;

    /**
     * Clear the underlying URI components
     */
    void clear() { uri_.clear(); }

    /**
     * Set the string from the API type dtn_endpoint_id_t
     *
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const dtn_endpoint_id_t* eid);

    /**
     * Operator copy assignment
     */
    EndpointID& operator=(const EndpointID& other) = delete;


    /**
     * Assign this endpoint ID as a copy of the other.
     */
    bool assign(const EndpointID& other)
    {
        uri_         = other.uri_;
        scheme_      = other.scheme_;
        valid_       = other.valid_;
        is_pattern_  = other.is_pattern_;
        node_num_    = other.node_num_;
        service_num_ = other.service_num_;
        return true;
    }
        
    /**
     * Set the string and validate it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const std::string& str)
    {
        uri_.assign(str);
        node_num_ = 0;
        service_num_ = 0;
        return validate();
    }

    /**
     * Set the string and validate it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const char* str, size_t len)
    {
        uri_.assign(str, len);
        node_num_ = 0;
        service_num_ = 0;
        return validate();
    }

    /**
     * Set the string from component pieces and validate it.
     * @return true if the string is a valid id, false if not.
     */
    bool assign(const std::string& scheme, const std::string& ssp)
    {
        uri_.assign(scheme + ":" + ssp);
        node_num_ = 0;
        service_num_ = 0;
        return validate();
    }

public:
    /**
     * Typedef for the return value possibilities from eid_dest_type.
     */
    typedef enum { UNKNOWN, SINGLETON, MULTINODE } eid_dest_type_t;
    
    /**
     * Return whether or not this endpoint id is a singleton or a
     * multi-node endpoint.
     */
    eid_dest_type_t eid_dest_type() const;

    /**
     * Whether an EID is a singleton destination
     */
    bool is_singleton() const;

    /**
     * Default setting for endpoint ids in unknown schemes.
     */
    static eid_dest_type_t unknown_eid_dest_type_default_;

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
    const URI&         uri()           const { return uri_; }
    const std::string& str()           const { return uri_.uri(); }
    const std::string  scheme_str()    const { return uri_.scheme(); }
    const std::string  ssp()           const { return uri_.ssp(); }
    Scheme*            scheme()        const { return scheme_; }
    bool               valid()         const { return valid_; }
    bool               is_pattern()    const { return is_pattern_; }
    const char*        c_str()         const { return uri_.uri().c_str(); } 
    const char*        data()          const { return uri_.uri().data(); }
    size_t             length()        const { return uri_.uri().length(); }
    size_t             node_num()      const;
    size_t             service_num()   const;

    bool               is_null_eid() const { return is_null_eid_; };
    bool               is_dtn_scheme() const;
    bool               is_imc_scheme() const;
    bool               is_ipn_scheme() const;
    bool               is_cbhe_compat() const;
    bool               is_node_wildcard() const { return is_node_wildcard_; }
    bool               is_service_wildcard() const { return is_service_wildcard_; }
    ///@}

protected:
    /**
     * Extract and look up the scheme and ssp.
     *
     * @return true if the string is a valid endpoint id, false if not.
     */
    bool validate();

    /**
     * Set the node_num_ and service_num_ values for IPN and IMC Scheme EIDs (called from validate)
     */
    void set_node_num();


    URI uri_;                            ///< endpoint URI

    Scheme* scheme_ = nullptr;           ///< the scheme class (if known)

    bool valid_ = false;                 ///< true iff the endpoint id is valid
    bool is_pattern_ = false;            ///< true iff this is an EndpointIDPattern

    bool is_null_eid_ = false;           ///< whether the instatiated EID is the NULL EID (dtn:none)
    bool is_node_wildcard_ = false;      ///< whether the IPN or IMC node/group num is a pattern wildcard
    bool is_service_wildcard_ = false;   ///< whether the IPN or IMC service num is a pattern wildcard

    size_t node_num_ = 0;       ///< IPN node numder or IMC group number of the EndpointID (0 if not IPN or IMC Scheme)
    size_t service_num_ = 0;    ///< Service number of the EndpointID if IPN or IMC Scheme else 0
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
    EndpointIDPattern(const EndpointIDPattern& other) = delete;

    /**
     * Construct the endpoint id pattern from another that is not
     * necessarily a pattern.
     */
    EndpointIDPattern(const EndpointID& other) = delete;

    /**
     * Operator copy assignment
     */
    EndpointIDPattern& operator=(const EndpointIDPattern& other) = delete;

    /**
     * Operator copy assignment fron another that is  not
     * necessarily a pattern.
     */
    EndpointIDPattern& operator=(const EndpointID& other) = delete;

    /**
     * Shortcut to the matching functionality implemented by the
     * scheme.
     */
    bool match(const SPtr_EID& sptr_eid) const;

    /**
     * Shortcut to the matching functionality for an IPN node number
     */
    bool match_ipn(size_t test_node_num) const;

    /**
     * Simple equality test function
     */
    bool equals(const SPtr_EIDPattern& sptr_other) const
    {
        return uri_ == sptr_other->uri_;
    }

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
class EndpointIDVector : public oasys::SerializableSPtrVector<EndpointID> {};

} // namespace dtn

#endif /* _ENDPOINT_ID_H_ */
