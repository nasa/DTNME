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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "BasicSMTP.h"

namespace oasys {

//----------------------------------------------------------------------------
BasicSMTPSender::BasicSMTPSender(const std::string& helo_domain,
                                 BasicSMTPMsg* msg)
    : helo_domain_(helo_domain), msg_(msg)
{
}

//----------------------------------------------------------------------------
void
BasicSMTPSender::get_HELO_domain(std::string* domain)
{
    domain->assign(helo_domain_);
}

//----------------------------------------------------------------------------
void
BasicSMTPSender::get_MAIL_from(std::string* from)
{
    from->assign(msg_->from_);
}

//----------------------------------------------------------------------------
void
BasicSMTPSender::get_RCPT_list(std::vector<std::string>* to)
{
    to->insert(to->begin(), msg_->to_.begin(), msg_->to_.end());
}

//----------------------------------------------------------------------------
void
BasicSMTPSender::get_DATA(const std::string** data)
{
    *data = &msg_->msg_;
}

//----------------------------------------------------------------------------
int
BasicSMTPSender::smtp_error(int code)
{
    log_err_p("/oasys/smtp-sender", "unexpected error %d", code);
    return -1;
}

//----------------------------------------------------------------------------
BasicSMTPHandler::BasicSMTPHandler()
{
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_HELO(const char* domain)
{
    (void)domain;
    return 250;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_MAIL(const char* from)
{
    if (strlen(from) == 0) {
        return 501;
    }

    cur_msg_.from_ = from;
    return 250;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_RCPT(const char* to)
{
    if (strlen(to) == 0) {
        return 501;
    }

    cur_msg_.to_.push_back(to);
    return 250;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_RSET()
{
    return 250;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_QUIT()
{
    return 221;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_DATA_begin()
{
    ASSERT(cur_msg_.msg_.size() == 0);
    return 0;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_DATA_line(const char* line)
{
    cur_msg_.msg_.append(line);
    cur_msg_.msg_.append("\r\n");

    return 0;
}

//----------------------------------------------------------------------------
int
BasicSMTPHandler::smtp_DATA_end()
{
    if (cur_msg_.valid()) {
        message_recvd(cur_msg_);
    }
    cur_msg_.clear();

    return 250;
}

} // namespace oasys
