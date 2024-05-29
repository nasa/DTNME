#ifndef __BYTESTREAM_H__
#define __BYTESTREAM_H__

#include <sys/types.h>
#include "../debug/DebugUtils.h"

namespace oasys {

/*!
 * ABC for output byte streams. Note we punt error describing to the
 * subclasses. All function at least return 0 on no error.
 */
class OutByteStream {
public:
    virtual ~OutByteStream() {}
    virtual int begin()                              = 0;
    virtual int write(const u_char* buf, size_t len) = 0;
    virtual int end()                                = 0;
};

/*!
 * ABC for input byte streams.
 */
class InByteStream {
public:
    virtual ~InByteStream() {}
    virtual int begin()                             = 0;
    virtual int read(u_char* buf, size_t len) const = 0;
    virtual int end()                               = 0;
};

} // namespace oasys

#endif /* __BYTESTREAM_H__ */
