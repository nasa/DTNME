/*
 *    Copyright 2007 Intel Corporation
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

#ifndef _OASYS_URI_H_
#define _OASYS_URI_H_

#include <string>

#include "../serialize/Serialize.h"

namespace oasys {

enum uri_parse_err_t {
    URI_PARSE_OK,             /* valid URI */
    URI_PARSE_NO_URI,         /* no URI in object */
    URI_PARSE_NO_SEP,         /* missing seperator char ':' */
    URI_PARSE_BAD_PERCENT,    /* invalid percent-encoded character */
    URI_PARSE_BAD_IP_LITERAL, /* invalid IP-literal encoding */
    URI_PARSE_BAD_IPV6,       /* invalid IPv6 address */
    URI_PARSE_BAD_SCHEME,     /* invalid scheme name */
    URI_PARSE_BAD_USERINFO,   /* invalide userinfo subcomponent */
    URI_PARSE_BAD_HOST,       /* invalid host subcomponent */
    URI_PARSE_BAD_PORT,       /* invalid port subcomponent */
    URI_PARSE_BAD_PATH,       /* invalid path component */
    URI_PARSE_BAD_QUERY,      /* invalid query component */
    URI_PARSE_BAD_FRAGMENT,   /* invalid fragment component */
    URI_PARSE_NO_PARAM_VAL,   /* missing value for cgi query parameter */
};

/**
 * Simple class for managing generic URIs based on RFC 3986.
 */
class URI : public SerializableObject {
public:
    /**
     * Default constructor.
     */
    URI() : port_num_(0),
            parse_err_(URI_PARSE_NO_URI),
            validate_(true),
            normalize_(true) {}

    /**
     * Constructs a URI from the given string.
     */
    URI(const std::string& uri, bool validate = true, bool normalize = true):
        uri_(uri),
        port_num_(0),
        validate_(validate),
        normalize_(normalize)
    {
        parse();
    }

    /**
     * Deep copy constructor.
     */
    URI(const URI& uri):
        SerializableObject(uri),
        uri_(uri.uri_),
        scheme_(uri.scheme_),
        ssp_(uri.ssp_),
        authority_(uri.authority_),
        path_(uri.path_),
        query_(uri.query_),
        fragment_(uri.fragment_),
        userinfo_(uri.userinfo_),
        host_(uri.host_),
        port_(uri.port_),
        port_num_(uri.port_num_),
        parse_err_(uri.parse_err_),
        validate_(uri.validate_),
        normalize_(uri.normalize_) {}

    /**
     * Destructor.
     */
    ~URI() {}

    /**
     * Set the URI to be the given std::string.
     */
    void assign(const std::string& str) {
        clear();
        uri_.assign(str);
        parse();
    }

    /**
     * Set the URI to be the given character string.
     */
    void assign(const char* str, size_t len) {
        clear();
        uri_.assign(str, len);
        parse();
    }

    /**
     * Set the URI to be the same as the given URI.
     */
    void assign(const URI& other) {
        clear();

        uri_          = other.uri_;
        scheme_       = other.scheme_;
        ssp_          = other.ssp_;
        authority_    = other.authority_;
        path_         = other.path_;
        query_        = other.query_;
        fragment_     = other.fragment_;
        userinfo_     = other.userinfo_;
        host_         = other.host_;
        port_         = other.port_;
        port_num_     = other.port_num_;
        parse_err_    = other.parse_err_;
        validate_     = other.validate_;
        normalize_    = other.normalize_;
    }

    /**
     * Clear the URI components, and main URI string if not flagged otherwise.
     */
    void clear(bool clear_uri = true) {
        if (clear_uri) {
            uri_.erase();
        }
        parse_err_ = URI_PARSE_NO_URI;
        
        scheme_.clear();
        ssp_.clear();
        authority_.clear();
        path_.clear();
        query_.clear();
        fragment_.clear();
        userinfo_.clear();
        host_.clear();
        port_.clear();
        port_num_ = 0;
    }

    /**
     * Determines if the given URI is contained within this URI.
     */
    bool subsume(const URI& other) const;

    /**
     * Operator overload for equality operator.
     */
    bool operator==(const URI& other) const
    { return (uri_ == other.uri_); }

    /**
     * Operator overload for inequality operator.
     */
    bool operator!=(const URI& other) const
    { return (uri_ != other.uri_); }

    /**
     * Operator overload for less-than operator.
     */
    bool operator<(const URI& other) const
    { return (uri_ < other.uri_); }

    /**
     * Three-way lexographical comparison
     */
    int compare(const URI& other) const
    {
        return uri_.compare(other.uri_);
    }

    /**
     * Virtual from SerializableObject. 
     */
    void serialize(SerializeAction* a);

    /**
     * Set validate and normalize flags.
     */
    void set_validate(bool validate = true)   { validate_ = validate; }
    void set_normalize(bool normalize = true) { normalize_ = normalize; }

    /// @{ Accessors
    uri_parse_err_t   parse_status() const { return parse_err_; }
    bool              valid()        const { return (parse_err_ == URI_PARSE_OK); }
    const std::string& uri()         const { return uri_; }
    const char*       c_str()        const { return uri_.c_str(); }
    const std::string scheme()       const { return uri_.substr(scheme_.offset_,
                                                                scheme_.length_); }
    const std::string ssp()          const { return uri_.substr(ssp_.offset_,
                                                                ssp_.length_); }
    const std::string authority()    const { return uri_.substr(authority_.offset_,
                                                                authority_.length_); }
    const std::string userinfo()     const { return uri_.substr(userinfo_.offset_,
                                                                userinfo_.length_); }
    const std::string host()         const { return uri_.substr(host_.offset_,
                                                                host_.length_); }
    const std::string port()         const { return uri_.substr(port_.offset_,
                                                                port_.length_); }
    const std::string path()         const { return uri_.substr(path_.offset_,
                                                                path_.length_); }
    const std::string query()        const { return uri_.substr(query_.offset_,
                                                                query_.length_); }
    const std::string fragment()     const { return uri_.substr(fragment_.offset_,
                                                                fragment_.length_); }
    unsigned int      port_num()     const { return port_num_; }
    /// @}

    /// @{ Setters
    void set_scheme(const std::string& scheme);
    void set_ssp(const std::string& ssp);
    void set_authority(const std::string& authority);
    void set_userinfo(const std::string& userinfo);
    void set_host(const std::string& host);
    void set_port(const std::string& port);
    void set_path(const std::string& path);
    void set_query(const std::string& query);
    void set_fragment(const std::string& fragment);
    /// @}

    /**
     * Return the value for the specified parameter of the query
     * string or "" if there is no such parameter.
     *
     * e.g. for scheme://authority/path?foo=bar;bar=baz
     *      query_value("foo") -> "bar"
     *      query_value("bar") -> "baz"
     *      query_value("baz") -> ""
     */
    const std::string query_value(const std::string& param) const;

private:
    /**
     * Parse the internal URI into its components, validate the URI
     * (if flagged to do so), and normalize the URI (if flagged to
     * do so).
     *
     * @return status code
     */
    uri_parse_err_t parse();
    
    /**
     * Parse URI scheme-specific parts (SSP) based on generic rules
     * defined in RFC 3986.
     *
     * @return status code
     */
    uri_parse_err_t parse_generic_ssp();
    uri_parse_err_t parse_authority();

    /**
     * Validate the URI components based on generic rules defined in RFC 3986.
     *
     * @return status code
     */
    uri_parse_err_t validate();
    uri_parse_err_t validate_scheme_name() const;
    uri_parse_err_t validate_userinfo() const;
    uri_parse_err_t validate_host() const;
    uri_parse_err_t validate_port() const;
    uri_parse_err_t validate_path() const;
    uri_parse_err_t validate_query() const;
    uri_parse_err_t validate_fragment() const;
    uri_parse_err_t validate_ip_literal(const std::string& host) const;
    uri_parse_err_t validate_eth_literal(const std::string& host) const;
 
    /**
     * Normalize the URI to the standard format as described in RFC 3986
     * so that equivalency is more accurately determined.
     *
     * These methods should be called only when the uri_ string has been
     * successfully parsed and found to be valid. The uri_ and component
     * strings may be modified by these methods.
     */
    void normalize();
    void normalize_scheme();
    void normalize_authority();
    void normalize_path();
    void normalize_query();
    void normalize_fragment();

    /**
     * Decode percent-encoded characters in the URI SSP components.
     *
     * These methods should be called only when the uri_ string has been
     * successfully parsed and found to be valid. The uri_ and component
     * strings may be modified by these methods.
     */
    void decode_authority();
    void decode_path();
    void decode_query();
    void decode_fragment();

    /**
     * @return true if the character is an "unreserved" character as
     * defined by RFC 3986, false if not.
     */
    static bool is_unreserved(char c);

    /**
     * @return true if the character is a "sub-delims" character as
     * defined by RFC 3986, false if not.
     */
    static bool is_sub_delim(char c);

    /**
     * @return true if the character is a hexidecimal character, false if not.
     */
    static bool is_hexdig(char c);

    /*
     * Data Members. For efficiency's sake, and to make inspection via
     * gdb simpler, we store a single std::string buffer containing
     * the whole URI and offset/length pairs for the subcomponents.
     */
    
    struct Component {
        Component() : offset_(0), length_(0) {}

        void clear()
        {
            offset_ = 0;
            length_ = 0;
        }

        void adjust_offset(int diff);
        void adjust_length(int diff);

        size_t offset_;
        size_t length_;
    };
    
    std::string uri_;          // URI string

    Component   scheme_;       // scheme component (not including ':')
    Component   ssp_;          // scheme-specific part (SSP) components
    Component   authority_;    // authority component
    Component   path_;         // path component
    Component   query_;        // query component
    Component   fragment_;     // fragment component
    Component   userinfo_;     // userinfo subcomponent of authority
    Component   host_;         // host subcomponent of authority
    Component   port_;         // port subcomponent of authority
    
    unsigned int    port_num_; // port as integer; 0 if no port

    uri_parse_err_t parse_err_;// parsing status

    bool            validate_; // true if URI is to be validated
    bool            normalize_;// true if URI is to be normalized
};

} // namespace oasys

#endif /* _OASYS_URI_H_ */

