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

#ifndef _OASYS_BASIC_SMTP_H_
#define _OASYS_BASIC_SMTP_H_

#include "SMTP.h"
#include "util/StringUtils.h"

namespace oasys {

//----------------------------------------------------------------------------
/*!
 * Simple struct for a mail message
 */
class BasicSMTPMsg {
public:
    std::string              from_;
    std::vector<std::string> to_;
    std::string              msg_;

    BasicSMTPMsg() {}
    BasicSMTPMsg(const std::string& from,
                 const std::string& to,
                 const std::string& msg)
    {
        from_ = from;
        tokenize(to, ", ", &to_);
        msg_  = msg;
    }
    
    bool valid() {
        return (from_.size() > 0 &&
                to_.size()   > 0 &&
                msg_.size()  > 0);
    }
    
    void clear() {
        from_.clear();
        to_.clear();
        msg_.clear();
    }
};

//----------------------------------------------------------------------------
/*!
 * Implements a simple smtp sender that works with the data in the
 * BasicSMTPMsg.
 */
class BasicSMTPSender : public SMTPSender {
public:
    BasicSMTPSender(const std::string& helo_domain, BasicSMTPMsg* msg);
    virtual ~BasicSMTPSender() {}
    
protected:
    //! @{ virtual from SMTPSender
    void get_HELO_domain(std::string* domain);
    void get_MAIL_from(std::string* from);
    void get_RCPT_list(std::vector<std::string>* to);
    void get_DATA(const std::string** data);
    int smtp_error(int code);
    //! @}

    std::string   helo_domain_;
    BasicSMTPMsg* msg_;
};

//----------------------------------------------------------------------------
/*!
 * Implements a not-so-efficient email consumer
 */
class BasicSMTPHandler : public SMTPHandler {
public:
    BasicSMTPHandler();

    //! @{ virtual from SMTPHandler
    int smtp_HELO(const char* domain);
    int smtp_MAIL(const char* from);
    int smtp_RCPT(const char* to);
    int smtp_DATA_begin();
    int smtp_DATA_line(const char* line);
    int smtp_DATA_end();
    int smtp_RSET();
    int smtp_QUIT();
    //! @}

    virtual void message_recvd(const BasicSMTPMsg& msg) = 0;

private:
    //! Current message that is being worked on
    BasicSMTPMsg cur_msg_;
};

} // namespace oasys 

#endif /* _OASYS_BASIC_SMTP_H_ */
