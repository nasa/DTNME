#ifndef __IOCLIENTSTREAM_H__
#define __IOCLIENTSTREAM_H__

namespace oasys {

/*!
 * Wrapper for IOClient to be used as an output stream.
 */
class IOClientOutStream : public OutByteStream {
public:
    // XXX/bowei -- TODO
};

/*!
 * Wrapper for IOClient to be used as an input stream.
 */
class IOClientInStream : public InByteStream {
public:
    // XXX/bowei -- TODO
};

} // namespace oasys

#endif /* __IOCLIENTSTREAM_H__ */
