/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef __OASYS_SMTP_H__
#define __OASYS_SMTP_H__

#include "../debug/Logger.h"
#include "../io/BufferedIO.h"
#include "../io/NetUtils.h"

namespace oasys {

class BasicSMTPMsg;
class SMTPHandler;
class SMTPSender;

//----------------------------------------------------------------------------
/*!
 * Basic class that implements the SMTP protocol.
 */
class SMTP : public Logger {
public:
    struct Config {
        /// Default config constructor
        Config()
            : addr_(htonl(INADDR_LOOPBACK)),
              port_(25),
              timeout_(-1),
              domain_("default.domain.com") {}

        /// Specific config constructor
        Config(in_addr_t addr, u_int16_t port,
               int timeout, const std::string& domain)
            : addr_(addr), port_(port),
              timeout_(timeout),
              domain_(domain) {}

        in_addr_t   addr_;      // listening address
        u_int16_t   port_;      // listening port
        int         timeout_;   // timeout used for IO
        std::string domain_;    // domain for HELO requests
    };

    static Config DEFAULT_CONFIG;

    SMTP(BufferedInput*  in,
         BufferedOutput* out,
         const Config&   config,
         const char*     logpath);

    int client_session(SMTPSender*  sender, bool first_session);
    int server_session(SMTPHandler* handler);

private:
    static const char* nl_; // newline char

    BufferedInput*  in_;
    BufferedOutput* out_;
    Config          config_;

    //! Send sign on message
    int send_signon();

    //! Process a command.
    int process_cmd(SMTPHandler* handler);

    //! Process a response.
    int process_response(int expected_code);
    
    //! Send back a response
    int send_response(int code);

    //! Response code may include a %s for the domain name
    const char* response_code(int code) const;
};

//----------------------------------------------------------------------------
/*!
 * Interface for class to send an outgoing SMTP message.
 * XXX/demmer 
 */
class SMTPSender {
public:
    virtual ~SMTPSender() {}
    
    //! @{ @return -1 to disconnect, otherwise error code given in
    //!    response code.
    virtual void get_HELO_domain(std::string* domain) = 0;
    virtual void get_MAIL_from(std::string* from) = 0;
    virtual void get_RCPT_list(std::vector<std::string>* to) = 0;
    virtual void get_RECEIVED(std::string* received) {(void)received;}
    virtual void get_DATA(const std::string** data) = 0;
    //! @}

    //! handle unexpected return code from server
    virtual int smtp_error(int code) = 0;
};

//----------------------------------------------------------------------------
/*!
 * Interface for a handler to process incoming SMTP messages.
 */
class SMTPHandler {
public:
    virtual ~SMTPHandler() {}

    //! @{ @return -1 to disconnect, otherwise error code given in
    //!    response code.
    virtual int smtp_HELO(const char* domain) = 0;
    virtual int smtp_MAIL(const char* from)   = 0;
    virtual int smtp_RCPT(const char* to)     = 0;
    virtual int smtp_RSET() = 0;
    virtual int smtp_QUIT() = 0;
    //! @}

    //! @{
    /*!
     * @{ @return -1 to disconnect, 0 on no error, otherwise error
     * code given in response code. smtp_DATA_end always returns a
     * response code, never 0.
     */
    virtual int smtp_DATA_begin()                = 0;
    virtual int smtp_DATA_line(const char* line) = 0;
    virtual int smtp_DATA_end()                  = 0;
    //! @}
};

} // namespace oasys

#endif /* __OASYS_SMTP_H__ */
