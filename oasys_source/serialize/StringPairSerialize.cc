/*
 *    Copyright XXX
 */

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "StringPairSerialize.h"
#include "debug/DebugUtils.h"
#include "debug/Log.h"
#include "../util/StringUtils.h"

using namespace std;

namespace oasys {

/******************************************************************************
 *
 * Base class
 *
 *****************************************************************************/

/**
 * Constructor.
 */
StringPairSerialize::StringPairSerialize(action_t action, 
                                         vector<StringPair> *rep,
                                         context_t context) :
    SerializeAction(action, context)
{
    ASSERT(action == MARSHAL || action == UNMARSHAL || action == INFO);
    ASSERT(rep != 0);
    rep_ = rep;
    return;
}

// destructor is here so we get the vtbl generated
StringPairSerialize::~StringPairSerialize()
{
    // nothing...
}

/**
 * start
 */
void 
StringPairSerialize::begin_action() 
{
    // nothing...
}

/**
 * finish
 */
void 
StringPairSerialize::end_action() 
{
    // nothing...
}

void 
StringPairSerialize::process(const char* , u_char** , u_int32_t* , int)
{
    NOTIMPLEMENTED;
}

/******************************************************************************
 *
 * Marshal
 *
 *****************************************************************************/

/**
 * Constructor.
 */
StringPairSerialize::Marshal::Marshal(vector<StringPair> *rep) :
    StringPairSerialize(MARSHAL, rep, Serialize::CONTEXT_LOCAL)

{
    // nothing...
}

// destructor is here so we get the vtbl generated
StringPairSerialize::Marshal::~Marshal()
{
    // nothing...
}

void 
StringPairSerialize::Marshal::process(const char* name, u_int64_t* i)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%llu", U64FMT(*i));
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, u_int32_t* i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", *i);
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, u_int16_t* i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", *i);
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, u_int8_t* i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", *i);
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, int32_t* i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", static_cast<int>(*i));
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, int16_t* i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", *i);
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, int8_t* i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", *i);
    rep_->push_back(StringPair(string(name), string(buf)));
}

void 
StringPairSerialize::Marshal::process(const char* name, bool* b)
{
    const char *buf = (*b) ? "TRUE" : "FALSE";
    rep_->push_back(StringPair(string(name), string(buf)));
}


void 
StringPairSerialize::Marshal::process(const char* name, std::string* s) 
{
    rep_->push_back(StringPair(string(name), *s));
}

void 
StringPairSerialize::Marshal::process(const char* name, u_char* bp, u_int32_t len)
{
    
    rep_->push_back(StringPair(string(name), hex2str(bp, len)));
}

void 
StringPairSerialize::Marshal::process(const char* name, 
                                      BufferCarrier<unsigned char>* buf)
{
    rep_->push_back(StringPair(string(name), hex2str(buf->buf(), buf->len())));
}

void
StringPairSerialize::Marshal::process(const char* name, 
                                      BufferCarrier<unsigned char>* buf, 
                                      u_char terminator)
{
    unsigned char *p = (unsigned char *)strchr((char *)buf->buf(), (int)terminator);
    buf->set_len(p != 0 ? (p - buf->buf()) : 0);
    process(name, buf);
}

/******************************************************************************
 *
 * Info
 *
 *****************************************************************************/

/**
 * Constructor.
 */

StringPairSerialize::Info::Info(vector<StringPair> *rep) :
    StringPairSerialize(INFO, rep, Serialize::CONTEXT_LOCAL)
{
    // nothing
}

// destructor is here so we get the vtbl generated
StringPairSerialize::Info::~Info()
{
    // nothing...
}

// Virtual functions inherited from SerializeAction
void 
StringPairSerialize::Info::process(const char* name, u_int64_t*)
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}

void 
StringPairSerialize::Info::process(const char* name, u_int32_t*)
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}

void 
StringPairSerialize::Info::process(const char* name, u_int16_t*)
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}

void
StringPairSerialize::Info::process(const char* name, u_int8_t*) 
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}
 
void 
StringPairSerialize::Info::process(const char* name, int32_t*)
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}

void 
StringPairSerialize::Info::process(const char* name, int16_t*)
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}

void
StringPairSerialize::Info::process(const char* name, int8_t*) 
{
    rep_->push_back(pair<string,string>(string(name), string("integer")));
}
 
void 
StringPairSerialize::Info::process(const char* name, bool*)
{
    rep_->push_back(pair<string,string>(string(name), string("boolean")));
}

void 
StringPairSerialize::Info::process(const char* name, std::string*) 
{
    rep_->push_back(pair<string,string>(string(name), string("string")));

}

void 
StringPairSerialize::Info::process(const char* name, u_char*, u_int32_t)
{
    rep_->push_back(pair<string,string>(string(name), string("string")));
}

void
StringPairSerialize::Info::process(const char* name, BufferCarrier<unsigned char>* )
{
    rep_->push_back(pair<string,string>(string(name), string("string")));
}

void
StringPairSerialize::Info::process(const char* name, 
                                   BufferCarrier<unsigned char>* ,
                                   u_char )
{
    rep_->push_back(pair<string,string>(string(name), string("string")));
}
    
/******************************************************************************
 *
 * Unmarshal
 *
 *****************************************************************************/

/**
 * Constructor.
 */

StringPairSerialize::Unmarshal::Unmarshal(vector<StringPair> *fields) :
    StringPairSerialize(UNMARSHAL, fields, Serialize::CONTEXT_LOCAL)
{
    // nothing...
}

// destructor is here so we get the vtbl generated
StringPairSerialize::Unmarshal::~Unmarshal()
{
    // nothing...
}

// find this element name in the list
size_t
StringPairSerialize::Unmarshal::find(const char *name)
{
    string n = name;
    size_t i;
    for (i = 0; i < rep_->size(); ++i) {
	if (rep_->at(i).first == n) {
	    break;
	}
    }
    return i;			// return after-the-end if not found
}

void
StringPairSerialize::Unmarshal::process(const char *name, u_int64_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    *i = atoll(rep_->at(idx).second.c_str());
    if (log_) logf(log_, LOG_DEBUG, "<=int64(%llu)", U64FMT(*i));
}

void
StringPairSerialize::Unmarshal::process(const char *name, u_int32_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    *i = atoi(rep_->at(idx).second.c_str());
    if (log_) logf(log_, LOG_DEBUG, "<=int32(%d)", *i);
}

void 
StringPairSerialize::Unmarshal::process(const char *name, u_int16_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    *i = atoi(rep_->at(idx).second.c_str());
    if (log_) logf(log_, LOG_DEBUG, "<=int16(%d)", *i);
}

void 
StringPairSerialize::Unmarshal::process(const char *name, u_int8_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    // XXX????
    *i = atoi(rep_->at(idx).second.c_str());
    if (log_) logf(log_, LOG_DEBUG, "<=int8(%d)", *i);
}

void
StringPairSerialize::Unmarshal::process(const char *name, int32_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    *i = atoi(rep_->at(idx).second.c_str());

    if (log_) logf(log_, LOG_DEBUG, "<=int32(%d)", static_cast<int>(*i));
}

void 
StringPairSerialize::Unmarshal::process(const char *name, int16_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    *i = atoi(rep_->at(idx).second.c_str());
    if (log_) logf(log_, LOG_DEBUG, "<=int16(%d)", *i);
}

void 
StringPairSerialize::Unmarshal::process(const char *name, int8_t* i)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    // XXX????
    *i = atoi(rep_->at(idx).second.c_str());
    if (log_) logf(log_, LOG_DEBUG, "<=int8(%d)", *i);
}

void 
StringPairSerialize::Unmarshal::process(const char *name, bool* b)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    const char* buf = rep_->at(idx).second.c_str();

    if (buf == NULL) return;

    switch(buf[0]) {
    case 'T':
    case 't':
    case '1':
    case '\1':
        *b = true;
        break;
    case 'F':
    case 'f':
    case '0':
    case '\0':
        *b = false;
        break;
    default:
        logf("/sql", LOG_ERR, "unexpected value '%s' for boolean column", buf);
        signal_error();
        return;
    }
    
    if (log_) logf(log_, LOG_DEBUG, "<=bool(%c)", *b ? 'T' : 'F');
}

void 
StringPairSerialize::Unmarshal::process(const char *name, std::string* s)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    *s = rep_->at(idx).second.c_str();
}

void 
StringPairSerialize::Unmarshal::process(const char* name, u_char* bp, u_int32_t len)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    string s = rep_->at(idx).second.c_str();
    size_t amt = (len < s.length() ? len : s.length());
    str2hex(s, bp, amt);

    if (log_) {
        std::string s;
        logf(log_, LOG_DEBUG, "<=bufc(%u: '%.*s')",
             len, (int)s.length(), s.data());
    }
}

void
StringPairSerialize::Unmarshal::process(const char* name, 
                                        BufferCarrier<unsigned char>* carrier)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    string s = rep_->at(idx).second;
    int len = s.length();
    u_char *buf = static_cast<u_char *>(malloc(sizeof(u_char) * len));
    ASSERT(buf != 0);
    str2hex(s, buf, len);
    carrier->set_buf(buf, len, true);
}

void
StringPairSerialize::Unmarshal::process(const char* name, 
                                        BufferCarrier<unsigned char>* carrier, 
                                        u_char terminator)
{
    size_t idx = find(name);
    ASSERT(idx < rep_->size());

    string s = rep_->at(idx).second;
    int len = s.length();
    u_char *buf = static_cast<u_char *>(malloc(sizeof(u_char) * (1 + len)));
    ASSERT(buf != 0);
    str2hex(s, buf, len);
    buf[len] = terminator;
    carrier->set_buf(buf, len, true);
}

} // namespace oasys
