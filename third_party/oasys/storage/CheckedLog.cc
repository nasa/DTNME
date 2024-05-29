#ifdef HAVE_CONFIG_H
#include <oasys-config.h>
#endif

#include "../util/ExpandableBuffer.h"
#include "../util/CRC32.h"
#include "../storage/FileBackedObject.h"

#include "CheckedLog.h"

namespace oasys {

//----------------------------------------------------------------------------
CheckedLogWriter::CheckedLogWriter(FdIOClient* fd)
    : fd_(fd)
{
    lseek(fd_->fd(), 0, SEEK_END);
}

//----------------------------------------------------------------------------
void
CheckedLogWriter::write_record(const char* buf, u_int32_t len)
{
    char ignore = '*';
    CRC32 crc;
    
    char len_buf[4];
    len_buf[0] = (len >> 24) & 0xFF;
    len_buf[1] = (len >> 16) & 0xFF;
    len_buf[2] = (len >> 8)  & 0xFF;
    len_buf[3] = len         & 0xFF;

    crc.update(len_buf, 4);
    crc.update(buf, len);

    char crc_buf[4];
    crc_buf[0] = (crc.value() >> 24) & 0xFF;
    crc_buf[1] = (crc.value() >> 16) & 0xFF;
    crc_buf[2] = (crc.value() >> 8)  & 0xFF;
    crc_buf[3] = crc.value()         & 0xFF;

    fd_->write(&ignore, 1);
    fd_->write(crc_buf, 4);
    fd_->write(len_buf, 4);
    fd_->write(buf, len);
}

//----------------------------------------------------------------------------
void 
CheckedLogWriter::force()
{
    fd_->sync();
}

//----------------------------------------------------------------------------
CheckedLogReader::CheckedLogReader(FdIOClient* fd)
    : fd_(fd),
      cur_offset_(0)
{}

//----------------------------------------------------------------------------
int 
CheckedLogReader::read_record(ExpandableBuffer* buf)
{
    struct stat stat_buf;

    fstat(fd_->fd(), &stat_buf);
    if (cur_offset_ == stat_buf.st_size)
    {
        return END;
    }
    
    char ignore;
    char crc_buf[sizeof(CRC32::CRC_t)];
    char len_buf[sizeof(u_int32_t)];
    
    int cc = fd_->read(&ignore, 1);
    if (cc != 1)
    {
        return BAD_CRC;
    }
    ++cur_offset_;

    cc = fd_->read(crc_buf, sizeof(CRC32::CRC_t));
    if (cc != sizeof(CRC32::CRC_t))
    {
        return BAD_CRC;
    }
    cur_offset_ += sizeof(CRC32::CRC_t);

    cc = fd_->read(len_buf, sizeof(u_int32_t));
    if (cc != sizeof(u_int32_t))
    {
        return BAD_CRC;
    }
    cur_offset_ += 4;

    off_t len = (len_buf[0] << 24) | (len_buf[1] << 16) | 
                (len_buf[2] << 8) | len_buf[3];

    // sanity check so we don't run out of memory due to corruption
    if (len > stat_buf.st_size - cur_offset_)
    {
        return BAD_CRC;
    }
    
    buf->reserve(len);
    cc = fd_->read(buf->raw_buf(), len);
    cur_offset_ += cc;

    if (cc != static_cast<int>(len))
    {
        return BAD_CRC;
    }

    CRC32 crc;
    crc.update(len_buf, 4);
    crc.update(buf->raw_buf(), len);
    
    if (crc.value() != CRC32::from_bytes(reinterpret_cast<u_char*>(crc_buf)))
    {
        return BAD_CRC;
    }    
    return (ignore == '!') ? IGNORE : 0;
}

} // namespace oasys
