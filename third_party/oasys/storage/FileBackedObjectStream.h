#ifndef __FILEBACKEDOBJECTSTREAM_H__
#define __FILEBACKEDOBJECTSTREAM_H__

#include "../io/ByteStream.h"

namespace oasys {

class FileBackedObject;

/*!
 * File backed object adaptors for streaming.
 */
class FileBackedObjectOutStream : public OutByteStream {
public:
    FileBackedObjectOutStream(FileBackedObject* obj, 
                              size_t offset = 0);

    // virtual from OutByteStream
    int begin();
    int write(const u_char* buf, size_t len);
    int end();


private:
    FileBackedObject* object_;
    size_t offset_;
};

/*!
 * File backed object adaptors for streaming.
 */
class FileBackedObjectInStream : public InByteStream {
public:
    FileBackedObjectInStream(FileBackedObject* obj, 
                             size_t offset = 0);

    // virtual from InByteStream
    int begin();
    int read(u_char* buf, size_t len) const;
    int end();

private:
    FileBackedObject* object_;
    mutable size_t offset_;
};


} // namespace oasys

#endif /* __FILEBACKEDOBJECTSTREAM_H__ */
