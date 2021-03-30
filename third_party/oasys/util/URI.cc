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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "URI.h"
#include "debug/Log.h"

namespace oasys {

static const char* URI_LOG = "/oasys/util/uri/";

//----------------------------------------------------------------------
void
URI::serialize(SerializeAction* a)
{
    a->process("uri", &uri_);

    if (a->action_code() == oasys::Serialize::UNMARSHAL) {
        parse();
    }
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::parse()
{
    (void)URI_LOG;

    clear(false);

    if (uri_.empty()) {
        log_debug_p(URI_LOG, "URI::parse: empty URI string");
        return (parse_err_ = URI_PARSE_NO_URI);
    }

    size_t scheme_len;
    if ((scheme_len = uri_.find(':')) == std::string::npos) {
        log_debug_p(URI_LOG, "URI::parse: no semicolon");
        return (parse_err_ = URI_PARSE_NO_SEP);
    }

    if (scheme_len == 0) {
        log_debug_p(URI_LOG, "URI::parse: empty scheme name");
        return (parse_err_ = URI_PARSE_BAD_SCHEME);
    }

    scheme_.offset_ = 0;
    scheme_.length_ = scheme_len;
    ssp_.offset_    = scheme_len + 1; // skip colon
    ssp_.length_    = uri_.length() - ssp_.offset_;

    uri_parse_err_t err;
    if (((err = parse_generic_ssp()) != URI_PARSE_OK) ||
        ((err = parse_authority()) != URI_PARSE_OK)) {
        return (parse_err_ = err);
    }

    // need to store the fact that the parsing phase has completed
    // since the validation part may end up needing to call one of the
    // setter functions
    parse_err_ = URI_PARSE_OK;
    
    if (validate_) {
        if ((err = validate()) != URI_PARSE_OK) {
            return (parse_err_ = err);
        }
    }

    if (normalize_) {
        normalize();
    }

    return (parse_err_ = URI_PARSE_OK);
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::parse_generic_ssp()
{
    if (ssp_.length_ == 0) {
        log_debug_p(URI_LOG, "URI::parse_generic_ssp: empty ssp");

        // make sure all the components also reflect the empty ssp
        // (i.e. they need a correct offset_ and 0 length)
        authority_ = path_ = query_ = fragment_ = ssp_;
        userinfo_ = host_ = port_ = ssp_;
        return URI_PARSE_OK;
    }

    size_t curr_pos = ssp_.offset_;

    // check for presence of authority component
    if ((ssp_.length_ >= 2) &&
        (uri_.at(curr_pos) == '/') && (uri_.at(curr_pos + 1) == '/'))
    {
        size_t authority_end = uri_.find_first_of("/?#", curr_pos + 2);
        if (authority_end == std::string::npos) {
            authority_end = uri_.length();
        }

        size_t authority_start = curr_pos;
        size_t authority_len   = authority_end - authority_start;
        ASSERT(authority_len > 0);

        authority_.offset_ = authority_start;
        authority_.length_ = authority_len;
        
        curr_pos = authority_end;        
    } else {
        authority_.offset_ = curr_pos;
        authority_.length_ = 0;
    }

    // path component is required (although it may be empty)
    if (curr_pos != uri_.length()) {
        size_t path_end = uri_.find_first_of("?#", curr_pos);
        if (path_end == std::string::npos) {
            path_end = uri_.length();
        }

        size_t path_start = curr_pos;
        size_t path_len   = path_end - path_start;

        path_.offset_ = path_start;
        path_.length_ = path_len;

        curr_pos = path_end;
    } else {
        path_.offset_ = curr_pos;
        path_.length_ = 0;
    }
    
    // check for presence of query component
    if ((curr_pos != uri_.length()) && (uri_.at(curr_pos) == '?')) {
        size_t query_end = uri_.find('#', curr_pos);
        if (query_end == std::string::npos) {
            query_end = uri_.length();
        }

        size_t query_start = curr_pos;
        size_t query_len   = query_end - query_start;
        ASSERT(query_len > 0);

        query_.offset_ = query_start;
        query_.length_ = query_len;
        curr_pos = query_end;
    } else {
        query_.offset_ = curr_pos;
        query_.length_ = 0;
    }

    // check for presence of fragment component
    if ((curr_pos != uri_.length()) && (uri_.at(curr_pos) == '#')) {
        size_t fragment_start = curr_pos;
        size_t fragment_len   = uri_.length() - fragment_start;
        ASSERT(fragment_len > 0);
        
        fragment_.offset_ = fragment_start;
        fragment_.length_ = fragment_len;
        curr_pos = uri_.length();
    } else {
        fragment_.offset_ = curr_pos;
        fragment_.length_ = 0;
    }

    ASSERT(curr_pos == uri_.length());

    return URI_PARSE_OK;
}
 
//----------------------------------------------------------------------
uri_parse_err_t
URI::parse_authority()
{
    if (authority_.length_ == 0) {
        // make sure the offset and length is properly set for the
        // subcomponents
        userinfo_ = host_ = port_ = authority_;
        return URI_PARSE_OK;
    }

    // XXX/demmer this and some other parse/validate routines could be
    // more efficient by not necessarily requiring the local temporary
    // var but instead working with the uri_ string directly
    const std::string& authority = this->authority();
    
    ASSERT(authority.length() >= 2);
    ASSERT(authority.substr(0, 2) == "//");

    size_t curr_pos = 2; // skip initial "//" characters

    // check for presence of user information subcomponent
    size_t userinfo_end = authority.find('@', curr_pos);
    if (userinfo_end != std::string::npos) {
        size_t userinfo_len = (userinfo_end + 1) - curr_pos; // includes '@'
        
        userinfo_.offset_ = authority_.offset_ + curr_pos;
        userinfo_.length_ = userinfo_len;

        curr_pos = userinfo_end + 1; // skip the @ character
    } else {
        userinfo_.offset_ = authority_.offset_ + curr_pos;
        userinfo_.length_ = 0;
    }

    // host subcomponent is required (although it may be empty)
    if (curr_pos != authority.length()) {
        size_t host_end;
        if (authority.at(curr_pos) == '[') {
            host_end = authority.find(']', curr_pos);
            if (host_end == std::string::npos) {
                log_debug_p(URI_LOG, "URI::parse_authority: "
                            "literal host component must end with ']'");
                return URI_PARSE_BAD_IP_LITERAL;
            }
            host_end++; // include '[' character
        } else {
            // Special case for ethernet uri
            if(this->scheme() == "eth") {
                host_end = authority.length();
            } else {
                host_end = authority.find(':', curr_pos);
                if (host_end == std::string::npos) {
                    host_end = authority.length();
                }
            }
        }

        size_t host_len = host_end - curr_pos;
        host_.offset_ = authority_.offset_ + curr_pos;
        host_.length_ = host_len;

        curr_pos = host_end;
    } else {
        host_.offset_ = authority_.offset_ + curr_pos;
        host_.length_ = 0;
    }

    // check for presence of port subcomponent
    if (curr_pos != authority.length()) {
        if (authority.at(curr_pos) != ':') {
            log_debug_p(URI_LOG, "URI::parse_authority: "
                        "semicolon expected prior to port");
            return URI_PARSE_BAD_PORT;
        }
        
        size_t port_start = curr_pos + 1; // skip ':' character
        size_t port_len   = authority.length() - port_start;

        port_.offset_ = authority_.offset_ + port_start;
        port_.length_ = port_len;
        
        if (port_len > 0) {
            // assign port_num here, though the validity of the string
            // isn't checked until validate_port
            port_num_ = atoi(port().c_str());
        }

        curr_pos = authority.length();
    } else {
        port_.offset_ = authority_.offset_ + curr_pos;
        port_.length_ = 0;
    }

    ASSERT(curr_pos == authority.length());

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate()
{
    ASSERT(validate_);
    
    uri_parse_err_t err;
    if (((err = validate_scheme_name()) != URI_PARSE_OK) ||
        ((err = validate_userinfo()) != URI_PARSE_OK) ||
        ((err = validate_host()) != URI_PARSE_OK) ||
        ((err = validate_port()) != URI_PARSE_OK) ||
        ((err = validate_path()) != URI_PARSE_OK) ||
        ((err = validate_query()) != URI_PARSE_OK) ||
        ((err = validate_fragment()) != URI_PARSE_OK)) {
        return (parse_err_ = err);
    }
    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
void
URI::normalize()
{
    ASSERT(normalize_);

    normalize_scheme();
    normalize_authority();
    normalize_path();
    normalize_query();
    normalize_fragment();

    log_debug_p("/oasys/util/uri/", "URI::normalize: "
                "normalized URI %s", uri_.c_str());
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_scheme_name() const
{
    const std::string& scheme = this->scheme();
    
    std::string::const_iterator iter = scheme.begin();

    if (!isalpha(*iter)) {
        log_debug_p(URI_LOG, "URI::validate_scheme_name: "
                    "first character is not a letter %c", (*iter));
        return URI_PARSE_BAD_SCHEME;
    }
    ++iter;

    for(; iter != scheme.end(); ++iter) {
        char c = *iter;
        if (isalnum(c) || (c == '+') || (c == '-') || (c == '.'))
            continue;

        log_debug_p(URI_LOG, "URI::validate_scheme_name: "
                    "invalid character in scheme name %c", c);
        return URI_PARSE_BAD_SCHEME;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
void
URI::normalize_scheme()
{
    // alpha characters should be lowercase within the scheme name
    for (unsigned int i = 0; i < scheme_.length_; ++i) {
        char c = uri_.at(scheme_.offset_ + i);
        if (isalpha(c) && isupper(c)) {
            uri_.replace(scheme_.offset_ + i, 1, 1, tolower(c));
        }
    }
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_userinfo() const
{
    if (userinfo_.length_ == 0) {
        return URI_PARSE_OK;
    }

    const std::string& userinfo = this->userinfo();

    ASSERT(userinfo.at(userinfo.length() - 1) == '@');

    // check for valid characters
    for (unsigned int i = 0; i < (userinfo.length() - 1); ++i) {
        char c = userinfo.at(i);
        if (is_unreserved(c) ||
            is_sub_delim(c)  ||
            (c == ':')) {
            continue;
        }

        // check for percent-encoded characters
        if (c == '%') {
            if (i + 2 >= (userinfo.length() - 1)) {
                log_debug_p(URI_LOG, "URI::validate_userinfo: "
                            "invalid percent-encoded length in userinfo");
                return URI_PARSE_BAD_PERCENT;
            }

            if (!is_hexdig(userinfo.at(i + 1)) ||
                !is_hexdig(userinfo.at(i + 2))) {
                log_debug_p(URI_LOG, "URI::validate_userinfo: "
                            "invalid percent-encoding in userinfo");
                return URI_PARSE_BAD_PERCENT;
            }

            i += 2;
            continue;
        }

        log_debug_p(URI_LOG, "URI::validate_userinfo: "
                    "invalid character in userinfo %c", c);
        return URI_PARSE_BAD_USERINFO;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_host() const
{
    std::string host = this->host();
    
    if (host.empty()) {
        return URI_PARSE_OK;
    }

    // check for IP-literal
    if (host.at(0) == '[') {
        ASSERT(host.at(host.length() - 1) == ']');
        return validate_ip_literal(host.substr(1, host.length() - 2));
    }

    // check for ethernet literal
    if(this->scheme() == "eth") {
        return validate_eth_literal(host);
    }

    // check for valid characters
    for (unsigned int i = 0; i < host.length(); ++i) {
        char c = host.at(i);
        if (is_unreserved(c) ||
            is_sub_delim(c)) {
            continue;
        }

        // check for percent-encoded characters
        if (c == '%') {
            if (i + 2 >= host.length()) {
                log_debug_p(URI_LOG, "URI::validate_host: "
                            "invalid percent-encoded length in host");
                return URI_PARSE_BAD_PERCENT; 
            }

            if (!is_hexdig(host.at(i + 1)) ||
                !is_hexdig(host.at(i + 2))) {
                log_debug_p(URI_LOG, "URI::validate_host: "
                            "invalid percent-encoding in host");
                return URI_PARSE_BAD_PERCENT;
            }

            i += 2;
            continue;
        }

        log_debug_p(URI_LOG, "URI::validate_host: "
                    "invalid character in host %c", c);
        return URI_PARSE_BAD_HOST;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_port() const
{
    if (port_.length_ == 0) {
        return URI_PARSE_OK;
    }
    
    const std::string& port = this->port();

    // check for valid characters
    for (unsigned int i = 0; i < port.length(); ++i) {
        char c = port.at(i);
        if (isdigit(c)) {
            continue;
        }

        log_debug_p(URI_LOG, "URI::validate_port: "
                    "invalid character in port %c", c);
        return URI_PARSE_BAD_PORT;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
void
URI::normalize_authority()
{
    decode_authority();

    // alpha characters should be lowercase within the host subcomponent
    for (unsigned int i = 0; i < host_.length_; ++i) {
        char c = uri_.at(host_.offset_ + i);

        if (c == '%') { 
            i += 2;
            continue;
        }
        
        if (isalpha(c) && isupper(c)) {
            uri_.replace(host_.offset_ + i, 1, 1, tolower(c));
        }
    }
}

//----------------------------------------------------------------------
void
URI::decode_authority()
{
    // decode percent-encoded characters within userinfo subcomponent 
    size_t p = 0, curr_pos = 0;
    const std::string& userinfo = this->userinfo();
    std::string decoded_userinfo;
    while ((p < userinfo.length()) &&
          ((p = userinfo.find('%', p)) != std::string::npos)) {

        ASSERT((p + 2) < userinfo.length());
        std::string hex_string = userinfo.substr(p + 1, 2);

        int hex_value;
        sscanf(hex_string.c_str(), "%x", &hex_value);
        char c = (char)hex_value;
        
        decoded_userinfo.append(userinfo, curr_pos, p - curr_pos);
        
        // skip "unallowed" characters
        if (!is_unreserved(c) &&
            !is_sub_delim(c)  && 
            (c != ':')) {

            decoded_userinfo.append(1, '%');

            c = userinfo.at(p + 1);
            if (isalpha(c) && islower(c)) {
                decoded_userinfo.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_userinfo.append(1, c);
            }
                
            c = userinfo.at(p + 2);
            if (isalpha(c) && islower(c)) {
                decoded_userinfo.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_userinfo.append(1, c);
            }
            
        } else {
            // change "allowed" characters from hex value to alpha character
            decoded_userinfo.append(1, c);
        }

        p += 3;
        curr_pos = p;
    }

    if (!decoded_userinfo.empty()) {
        ASSERT(curr_pos <= userinfo.length());
        decoded_userinfo.append(userinfo, curr_pos,
                                userinfo.length() - curr_pos);
        set_userinfo(decoded_userinfo);
    }   

    // decode percent-encoded characters within host subcomponent 
    p = 0; curr_pos = 0;
    const std::string& host = this->host();
    std::string decoded_host;
    while ((p < host.length()) &&
          ((p = host.find('%', p)) != std::string::npos)) {

        ASSERT((p + 2) < host.length());
        std::string hex_string = host.substr(p + 1, 2);

        int hex_value;
        sscanf(hex_string.c_str(), "%x", &hex_value);
        char c = (char)hex_value;
        
        decoded_host.append(host, curr_pos, p - curr_pos);
        
        // skip "unallowed" characters
        if (!is_unreserved(c) &&
            !is_sub_delim(c)) {

            decoded_host.append(1, '%');

            c = host.at(p + 1);
            if (isalpha(c) && islower(c)) {
                decoded_host.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_host.append(1, c);
            }
                
            c = host.at(p + 2);
            if (isalpha(c) && islower(c)) {
                decoded_host.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_host.append(1, c);
            }
        } else {
            // change "allowed" characters from hex value to alpha character
            decoded_host.append(1, c);
        }

        p += 3;
        curr_pos = p;
    }

    if (!decoded_host.empty()) {
        ASSERT(curr_pos <= host.length());
        decoded_host.append(host, curr_pos, host.length() - curr_pos);
        set_host(decoded_host);
    }
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_ip_literal(const std::string& host) const
{
    // XXX/demmer this should move into another utility function in NetUtils
    // or something like that.
    if (host.empty()) {
        log_debug_p(URI_LOG, "URI::validate_ip_literal: empty host");
        return URI_PARSE_BAD_IP_LITERAL;
    }

    size_t curr_pos = 0;

    if ((host.at(curr_pos) == 'v') || (host.at(curr_pos) == 'V')) {
        ++curr_pos; // skip version character

        if ((curr_pos == host.length()) || !is_hexdig(host.at(curr_pos))) {
            log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                        "hexidecimal version expected");
            return URI_PARSE_BAD_IP_LITERAL;
        }
        ++curr_pos; // skip first hexidecimal character

        for ( ; curr_pos != host.length(); ++curr_pos) {
            if (!is_hexdig(host.at(curr_pos))) {
                break;
            }
        }

        if ((curr_pos == host.length()) || (host.at(curr_pos) != '.')) {
            log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                        "period character expected");
            return URI_PARSE_BAD_IP_LITERAL;
        }
        ++curr_pos; // skip period character

        if (curr_pos == host.length()) {
            log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                        "additional character expected");
            return URI_PARSE_BAD_IP_LITERAL;
        }

        for ( ; curr_pos < host.length(); ++curr_pos) {
            char c = host.at(curr_pos);
            if (is_unreserved(c) ||
                is_sub_delim(c)  ||
                (c == ':')) {
                continue;
            }

            log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                        "invalid character in IP literal %c", c);
            return URI_PARSE_BAD_IP_LITERAL;
        }

        ASSERT(curr_pos == host.length());
        return URI_PARSE_OK;
    }

    int num_pieces = 0;
    bool double_colon = false, prev_double_colon = false;
    size_t decimal_start;

    while (true) {
        decimal_start = curr_pos;
        unsigned int num_hexdig;
        for (num_hexdig = 0;
             num_hexdig < 4 && curr_pos < host.length();
             ++num_hexdig, ++curr_pos) {
            if (!is_hexdig(host.at(curr_pos))) {
                break;
            }
        }
        ++num_pieces;

        if (curr_pos == host.length()) {
            if (num_hexdig == 0) {
                if (!prev_double_colon) {
                    log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                                "ip literal must not end in single colon");
                    return URI_PARSE_BAD_IPV6;
                }
                --num_pieces;
            }
            break;
        }

        prev_double_colon = false;

        if (host.at(curr_pos) == ':') {
            if (num_hexdig == 0) {
                if (num_pieces == 1) {
                    if (((curr_pos + 1) == host.length()) ||
                        (host.at(curr_pos + 1) != ':')) {
                        log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                                    "double colon or hexidecimal "
                                    "character expected");
                        return URI_PARSE_BAD_IPV6;
                    }
                    ++curr_pos; // skip first colon character
                }

                if (double_colon) {
                    log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                                "multiple double colon's not allowed");
                    return URI_PARSE_BAD_IPV6;
                }

                double_colon = true;
                prev_double_colon = true;
            }

            ++curr_pos; // skip colon character

        } else if (host.at(curr_pos) == '.') {
            if (num_hexdig == 0) {
                log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                            "period must only follow decimal character");
                return URI_PARSE_BAD_IPV6;
            }
            --num_pieces;
            break;

        } else {
            log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                        "colon or period character expected");
            return URI_PARSE_BAD_IPV6;
        }
    }

    if (curr_pos != host.length()) {
        if ((num_pieces <= 0) ||
            (double_colon && (num_pieces > 6)) ||
            (!double_colon && (num_pieces != 6))) {
            log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                        "invalid number of hexidecimal encoded pieces, "
                        "cannot read IPv4 address");
            return URI_PARSE_BAD_IPV6;
        }

        curr_pos = decimal_start;

        for (unsigned int i = 0; i < 4; ) {

            char digit[4] = {0};
            unsigned int num_digit = 0;
            for ( ; num_digit < 3 && curr_pos < host.length();
                    ++num_digit, ++curr_pos) {
                if (!isdigit(host.at(curr_pos))) {
                    break;
                }
                digit[num_digit] = host.at(curr_pos);
            }
            digit[num_digit] = '\0';

            if (num_digit == 0) {
                log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                            "decimal character expected");
                return URI_PARSE_BAD_IPV6;
            }

            if ((num_digit > 1) && (digit[0] == '0')) {
                log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                            "leading zeros not permitted");
                return URI_PARSE_BAD_IPV6;
            }

            int num = atoi(digit);
            if ((num < 0) || (num > 255)) {
                log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                            "invalid decimal octet");
                return URI_PARSE_BAD_IPV6;
            }

            if (++i == 4) {
                if (curr_pos != host.length()) {
                    log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                                "end of host expected");
                    return URI_PARSE_BAD_IPV6;
                }
                break;
            }

	    if ((curr_pos == host.length()) || (host.at(curr_pos) != '.')) {
                log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                            "period character expected");
                return URI_PARSE_BAD_IPV6;
            }
            ++curr_pos; // skip period character
        }

        num_pieces += 2;
    }

    ASSERT(curr_pos == host.length());

    if ((num_pieces <= 0) ||
        (double_colon && (num_pieces > 8)) ||
        (!double_colon && (num_pieces != 8))) {
        log_debug_p(URI_LOG, "URI::validate_ip_literal: "
                    "invalid number of hexidecimal encoded pieces %d",
                    num_pieces);
        return URI_PARSE_BAD_IPV6;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_path() const
{
    if (path_.length_ == 0) {
        return URI_PARSE_OK;
    }
    
    const std::string& path = this->path();

    //dzdebug if (!authority_.length_ == 0) {
    if (authority_.length_ != 0) {
        ASSERT(path.at(0) == '/');
    }

    if (authority_.length_ == 0 && (path.length() >= 2)) {
        ASSERT(path.substr(0, 2) != "//");
    }

    // check for valid characters
    for (unsigned int i = 0; i < path.length(); ++i) {
        char c = path.at(i);
        if (is_unreserved(c) ||
            is_sub_delim(c)  ||
            (c == '/')       ||
            (c == ':')       ||
            (c == '@')) {
            continue;
        }

        // check for percent-encoded characters
        if (c == '%') {
            if (i + 2 >= path.length()) {
                log_debug_p(URI_LOG, "URI::validate_path: "
                            "invalid percent-encoded length in path");
                return URI_PARSE_BAD_PERCENT;
            }

            if (!is_hexdig(path.at(i + 1)) ||
                !is_hexdig(path.at(i + 2))) {
                log_debug_p(URI_LOG, "URI::validate_path: "
                            "invalid percent-encoding in path");
                return URI_PARSE_BAD_PERCENT;
            }

            i += 2;
            continue;
        }

        log_debug_p(URI_LOG, "URI:validate_path: "
                    "invalid character in path component %c", c);
        return URI_PARSE_BAD_PATH;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
uri_parse_err_t URI::validate_eth_literal(const std::string& host) const
{
    if(host.length() != 17) {
        log_debug_p(URI_LOG, "URI::validate_eth_literal: bad host");
        return URI_PARSE_BAD_HOST;
    }

    for(size_t i = 0; i < host.length(); i++) {
        if((i + 1) % 3) {
            if(is_hexdig(host.at(i)))
                continue;
        } else {
            if(host.at(i) == ':')
                continue;
        }

        return URI_PARSE_BAD_HOST;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
void
URI::normalize_path()
{
    decode_path();

    std::string path = this->path();

    // resolve '.' and '..' path segments
    bool modified = false;

    size_t checkpoint = 0, dot_segment;
    while ((dot_segment = path.find("./", checkpoint)) != std::string::npos) {
        modified = true;
        
        if ((dot_segment == 0) || (path.at(dot_segment - 1) == '/')) {
            path.erase(dot_segment, 2);
            continue;
        }

        ASSERT(dot_segment >= 1);
        if (path.at(dot_segment - 1) == '.') {
            if (dot_segment == 1) {
                path.erase(dot_segment - 1, 3);
                continue;
            }

            ASSERT(dot_segment >= 2);
            if (path.at(dot_segment - 2) == '/') {
                if (dot_segment == 2) {
                    path.erase(dot_segment - 1, 3);
                    continue;
                }

                ASSERT(dot_segment >= 3);
                size_t prev_seg = path.find_last_of('/', dot_segment - 3);
                if (prev_seg == std::string::npos) {
                    prev_seg = 0;
                }

                size_t erase_length = (dot_segment + 1) - prev_seg;
                path.erase(prev_seg, erase_length);
                checkpoint = prev_seg;
                continue;
            }
        }

        checkpoint = dot_segment + 2;
    }

    if ((path.length() == 1) && (path.at(0) == '.')) {
        modified = true;
        path.erase(0, 1);

    } else if ((path.length() == 2) && (path.substr(0, 2) == "..")) {
        modified = true;
        path.erase(0, 2);

    } else if ((path.length() >= 2) &&
               (path.substr(path.length() - 2, 2) == "/.")) {
        modified = true;
        path.erase(path.length() - 1, 1);

    } else if ((path.length() >= 3) &&
               (path.substr(path.length() - 3, 3) == "/..")) {
        modified = true;
        if (path.length() == 3) {
            path.erase(path.length() - 2, 2);
        } else {
            size_t prev_seg = path.find_last_of('/', path.length() - 4);
            if (prev_seg == std::string::npos) {
                prev_seg = 0;
            }

            size_t erase_length = path.length() - prev_seg;
            path.erase(prev_seg, erase_length);
            path.append(1, '/');
        }
    }
    
    if (modified) {
        set_path(path);
    }
}

//----------------------------------------------------------------------
void
URI::decode_path()
{
    const std::string& path = this->path();
    std::string decoded_path;

    size_t p = 0, curr_pos = 0;
    while ((p < path.length()) &&
          ((p = path.find('%', p)) != std::string::npos)) {

        ASSERT((p + 2) < path.length());
        std::string hex_string = path.substr(p + 1, 2);

        int hex_value;
        sscanf(hex_string.c_str(), "%x", &hex_value);
        char c = (char)hex_value;

        decoded_path.append(path, curr_pos, p - curr_pos);
        
        // skip "unallowed" character
        if (!is_unreserved(c) &&
            !is_sub_delim(c)  && 
            (c != ':')        && 
            (c != '@')) {

            decoded_path.append(1, '%');

            c = path.at(p + 1);
            if (isalpha(c) && islower(c)) {
                decoded_path.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_path.append(1, c);
            }
                
            c = path.at(p + 2);
            if (isalpha(c) && islower(c)) {
                decoded_path.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_path.append(1, c);
            }

        } else {
            // change "allowed" character from hex value to alpha character
            decoded_path.append(1, c);
        }

        p += 3;
        curr_pos = p;
    }

    if (!decoded_path.empty()) {
        ASSERT(curr_pos <= path.length());
        decoded_path.append(path, curr_pos, path.length() - curr_pos);
        set_path(decoded_path);
    }
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_query() const
{
    if (query_.length_ == 0) {
        return URI_PARSE_OK;
    }

    const std::string& query = this->query();
    ASSERT(query.at(0) == '?');

    // check for valid characters
    unsigned int i = 1; // skip initial question mark character
    for ( ; i < query.length(); ++i) {
        char c = query.at(i);
        if (is_unreserved(c) ||
            is_sub_delim(c)  ||
            (c == ':')       ||
            (c == '@')       ||
            (c == '/')       ||
            (c == '?')) {
            continue;
        }

        // check for percent-encoded characters
        if (c == '%') {
            if (i + 2 >= query.length()) {
                log_debug_p(URI_LOG, "URI::validate_query: "
                            "invalid percent-encoded length in query");
                return URI_PARSE_BAD_PERCENT;
            }

            if (!is_hexdig(query.at(i + 1)) ||
                !is_hexdig(query.at(i + 2))) {
                log_debug_p(URI_LOG, "URI::validate_query: "
                            "invalid percent-encoding in query");
                return URI_PARSE_BAD_PERCENT;
            }

            i += 2;
            continue;
        }

        log_debug_p(URI_LOG, "URI::validate_query: "
                    "invalid character in query component %c", c);
        return URI_PARSE_BAD_QUERY;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
void
URI::normalize_query()
{
    decode_query();
}

//----------------------------------------------------------------------
void
URI::decode_query()
{
    const std::string& query = this->query();
    std::string decoded_query;

    size_t p = 0, curr_pos = 0;
    while ((p < query.length()) &&
          ((p = query.find('%', p)) != std::string::npos)) {

        ASSERT((p + 2) < query.length());
        std::string hex_string = query.substr(p + 1, 2);

        int hex_value;
        sscanf(hex_string.c_str(), "%x", &hex_value);
        char c = (char)hex_value;

        decoded_query.append(query, curr_pos, p - curr_pos);
        
        // skip "unallowed" character
        if (!is_unreserved(c) &&
            !is_sub_delim(c)  && 
            (c != ':')        && 
            (c != '@')        && 
            (c != '/')        && 
            (c != '?')) {

            decoded_query.append(1, '%');

            c = query.at(p + 1);
            if (isalpha(c) && islower(c)) {
                decoded_query.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_query.append(1, c);
            }
                
            c = query.at(p + 2);
            if (isalpha(c) && islower(c)) {
                decoded_query.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_query.append(1, c);
            }
            
        } else {
            // change "allowed" character from hex value to alpha character
            decoded_query.append(1, c);
        }

        p += 3;
        curr_pos = p;
    }

    if (!decoded_query.empty()) {
        ASSERT(curr_pos <= query.length());
        decoded_query.append(query, curr_pos, query.length() - curr_pos);
        set_query(decoded_query);
    }
}

//----------------------------------------------------------------------
uri_parse_err_t
URI::validate_fragment() const
{
    if (fragment_.length_ == 0) {
        return URI_PARSE_OK;
    }

    const std::string& fragment = this->fragment();

    ASSERT(fragment.at(0) == '#');

    // check for valid characters
    unsigned int i = 1; // skip initial number character    
    for ( ; i < fragment.length(); ++i) {
        char c = fragment.at(i);
        if (is_unreserved(c) ||
            is_sub_delim(c)  ||
            (c == ':')       ||
            (c == '@')       ||
            (c == '/')       ||
            (c == '?')) {
            continue;
        }

        // check for percent-encoded characters
        if (c == '%') {
            if (i + 2 >= fragment.length()) {
                log_debug_p(URI_LOG, "URI::validate_fragment: "
                            "invalid percent-encoded length in fragment");
                return URI_PARSE_BAD_PERCENT;
            }

            if (!is_hexdig(fragment.at(i + 1)) ||
                !is_hexdig(fragment.at(i + 2))) {
                log_debug_p(URI_LOG, "URI::validate_fragment: "
                            "invalid percent-encoding in fragment");
                return URI_PARSE_BAD_PERCENT;
            }

            i += 2;
            continue;
        }

        log_debug_p(URI_LOG, "URI::validate_fragment: "
                    "invalid character in fragment component %c", c);
        return URI_PARSE_BAD_FRAGMENT;
    }

    return URI_PARSE_OK;
}

//----------------------------------------------------------------------
void
URI::normalize_fragment()
{
    decode_fragment();
}

//----------------------------------------------------------------------
void
URI::decode_fragment()
{
    const std::string& fragment = this->fragment();
    
    std::string decoded_fragment;

    size_t p = 0, curr_pos = 0;
    while ((p < fragment.length()) &&
          ((p = fragment.find('%', p)) != std::string::npos)) {

        ASSERT((p + 2) < fragment.length());
        std::string hex_string = fragment.substr(p + 1, 2);

        int hex_value;
        sscanf(hex_string.c_str(), "%x", &hex_value);
        char c = (char)hex_value;

        decoded_fragment.append(fragment, curr_pos, p - curr_pos);
        
        // skip unallowed character
        if (!is_unreserved(c) &&
            !is_sub_delim(c)  && 
            (c != ':')        && 
            (c != '@')        && 
            (c != '/')        && 
            (c != '?')) {

            decoded_fragment.append(1, '%');

            c = fragment.at(p + 1);
            if (isalpha(c) && islower(c)) {
                decoded_fragment.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_fragment.append(1, c);
            }
                
            c = fragment.at(p + 2);
            if (isalpha(c) && islower(c)) {
                decoded_fragment.append(1, static_cast<char>(toupper(c)));
            } else {
                decoded_fragment.append(1, c);
            }
            
        } else {
            // change "allowed" character from hex value to alpha character
            decoded_fragment.append(1, c);
        }

        p += 3;
        curr_pos = p;
    }

    if (!decoded_fragment.empty()) {
        ASSERT(curr_pos <= fragment.length());
        decoded_fragment.append(fragment, curr_pos,
                                fragment.length() - curr_pos);
        set_fragment(decoded_fragment);
    }
}

//----------------------------------------------------------------------
bool
URI::subsume(const URI& other) const
{
    // both URIs must be valid
    if (!valid() || !other.valid()) {
        return false;
    }

    // determine if this URI contains (from the start) the other URI
    if (uri_.find(other.uri_, 0) != 0) {
        return false;
    }
    ASSERT(uri_.length() >= other.uri_.length());

    // if they're not equal, check for proper delimiter, which must be
    // a path, query, or fragment boundary
    if (uri_.length() > other.uri_.length())
    {
        char c = uri_.at(other.uri_.length());
        if ((c == '/') || (c == '?') || (c == '#')) {
            return true;
        }

        c = uri_.at(other.uri_.length() - 1);
        if ((c == '/') || (c == '?') || (c == '#')) {
            return true;
        }

        return false;
    }
    
    return true;
}

//----------------------------------------------------------------------
bool
URI::is_unreserved(char c)
{
    return (isalnum(c) ||
            (c == '-') ||
            (c == '.') ||
            (c == '_') ||
            (c == '~'));
}

//----------------------------------------------------------------------
bool
URI::is_sub_delim(char c)
{
    return ((c == '!') ||
            (c == '$') ||
            (c == '&') ||
            (c == '\'') ||
            (c == '(') ||
            (c == ')') ||
            (c == '*') ||
            (c == '+') ||
            (c == ',') ||
            (c == ';') ||
            (c == '='));
}

//----------------------------------------------------------------------
bool
URI::is_hexdig(char c)
{
    return (isdigit(c) ||
            (c == 'a') || (c == 'A') ||
            (c == 'b') || (c == 'B') ||
            (c == 'c') || (c == 'C') ||
            (c == 'd') || (c == 'D') ||
            (c == 'e') || (c == 'E') ||
            (c == 'f') || (c == 'F'));
}

//----------------------------------------------------------------------
inline void
URI::Component::adjust_offset(int diff)
{
    if (offset_ == 0)
        return;

    if (diff > 0) {
        offset_ += diff;
    } else {
        ASSERT(offset_ >= (size_t)-diff);
        offset_ -= static_cast<size_t>(-diff);
    }
}

//----------------------------------------------------------------------
inline void
URI::Component::adjust_length(int diff)
{
    if (diff > 0) {
        length_ += diff;
    } else {
        ASSERT(length_ >= (size_t)-diff);
        length_ -= static_cast<size_t>(-diff);
    }
}

//----------------------------------------------------------------------
void
URI::set_scheme(const std::string& scheme)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(scheme_.offset_, scheme_.length_, scheme);
    
    int diff = scheme.length() - scheme_.length_;
    if (diff != 0) {
        scheme_.adjust_length(diff);
        
        ssp_.adjust_offset(diff);
        authority_.adjust_offset(diff);
        userinfo_.adjust_offset(diff);
        host_.adjust_offset(diff);
        port_.adjust_offset(diff);
        path_.adjust_offset(diff);
        query_.adjust_offset(diff);
        fragment_.adjust_offset(diff);
    }
}

//----------------------------------------------------------------------
void
URI::set_ssp(const std::string& ssp)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    // just reparse the whole thing 
    uri_.replace(ssp_.offset_, ssp_.length_, ssp);
    parse();
}

//----------------------------------------------------------------------
void
URI::set_authority(const std::string& authority)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(authority_.offset_, authority_.length_, authority);
    
    int diff = authority.length() - authority_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        authority_.adjust_length(diff);
        
        path_.adjust_offset(diff);
        query_.adjust_offset(diff);
        fragment_.adjust_offset(diff);
    }

    parse_authority();
}

//----------------------------------------------------------------------
void
URI::set_userinfo(const std::string& userinfo)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(userinfo_.offset_, userinfo_.length_, userinfo);
    
    int diff = userinfo.length() - userinfo_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        authority_.adjust_length(diff);
        userinfo_.adjust_length(diff);
        
        host_.adjust_offset(diff);
        port_.adjust_offset(diff);
        path_.adjust_offset(diff);
        query_.adjust_offset(diff);
        fragment_.adjust_offset(diff);
    }
}

//----------------------------------------------------------------------
void
URI::set_host(const std::string& host)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(host_.offset_, host_.length_, host);
    
    int diff = host.length() - host_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        authority_.adjust_length(diff);
        host_.adjust_length(diff);
        
        port_.adjust_offset(diff);
        path_.adjust_offset(diff);
        query_.adjust_offset(diff);
        fragment_.adjust_offset(diff);
    }
}

//----------------------------------------------------------------------
void
URI::set_port(const std::string& port)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    // XXX/demmer this will not clear the port if it was previously
    // set since it doesn't remove the : character from the uri
    
    uri_.replace(port_.offset_, port_.length_, port);
    
    int diff = port.length() - port_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        authority_.adjust_length(diff);
        port_.adjust_length(diff);
                
        path_.adjust_offset(diff);
        query_.adjust_offset(diff);
        fragment_.adjust_offset(diff);
    }
    
    port_num_ = atoi(port.c_str());
}

//----------------------------------------------------------------------
void
URI::set_path(const std::string& path)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(path_.offset_, path_.length_, path);
    
    int diff = path.length() - path_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        path_.adjust_length(diff);
        
        query_.adjust_offset(diff);
        fragment_.adjust_offset(diff);
    }
}

//----------------------------------------------------------------------
void
URI::set_query(const std::string& query)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(query_.offset_, query_.length_, query);
    
    int diff = query.length() - query_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        query_.adjust_length(diff);

        fragment_.adjust_offset(diff);
    }
}

//----------------------------------------------------------------------
void
URI::set_fragment(const std::string& fragment)
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    uri_.replace(fragment_.offset_, fragment_.length_, fragment);
    
    int diff = fragment.length() - fragment_.length_;
    
    if (diff != 0) {
        ssp_.adjust_length(diff);
        fragment_.adjust_length(diff);
    }
}

//----------------------------------------------------------------------
const std::string
URI::query_value(const std::string& param) const
{
    ASSERT(parse_err_ == URI_PARSE_OK);
    
    if (query_.length_ == 0) {
        return "";
    }

    ASSERT(uri_.at(query_.offset_) == '?');
    size_t curr_pos = query_.offset_;
    while (curr_pos != std::string::npos &&
           curr_pos < query_.offset_ + query_.length_)
    {
        size_t param_start = curr_pos + 1;
        size_t param_end = uri_.find('=', param_start);

        if (param_end == std::string::npos) { 
            return "";
        }
        
        if (param_end > query_.offset_ + query_.length_) {
            return "";
        }
        size_t param_length = param_end - param_start;

        // check whether this parameter matches the one we're looking for
        if (uri_.compare(param_start, param_length, param) != 0) {
            curr_pos = uri_.find_first_of(";", param_start);
            continue;
        }

        // make sure there's a value
        if (uri_.at(param_end) != '=') {
            return "";
        }

        size_t value_start = param_end + 1;
        size_t value_end = uri_.find_first_of(";#", value_start);
        if (value_end == std::string::npos) {
            value_end = uri_.length();
        }
        std::string ret = uri_.substr(value_start, value_end - value_start);
        return uri_.substr(value_start, value_end - value_start);
    }

    return "";
}

} // namespace oasys
