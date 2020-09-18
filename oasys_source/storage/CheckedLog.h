#ifndef __CHECKEDLOG_H__
#define __CHECKEDLOG_H__

#include "../io/FdIOClient.h"

namespace oasys {

class FdIOClient;

/*!
 * A CRC protected log to guard against short writes.
 *
 * Each record is [CRC, length, data]. The CRC is on on the length and
 * data.
 */
class CheckedLogWriter {
public:
    /*!
     * Interpret the object as a checked log and write to it.
     */
    CheckedLogWriter(FdIOClient* fd);

    /*!
     * Write a single record to the log file. This does _not_ force
     * the log file to disk.
     *
     * @return handle to the record for later manipulation.
     */
    void write_record(const char* buf, u_int32_t len);

    /*!
     * For all log files written thus far to the disk.
     */ 
    void force();

private:
    FdIOClient* fd_;
};

/*!
 * Read in the log file record by record.
 */
class CheckedLogReader {
public:
    enum {
        NO_ERR  = 0,
        END     = -1,
        BAD_CRC = -2,
        IGNORE  = -3,
    };

    /*!
     * Interpret the object as a checked log and write to it.
     */
    CheckedLogReader(FdIOClient* fd);

    /*!
     * Read a record from the log.
     *
     * @return 0 on no error, see enum above.
     */
    int read_record(ExpandableBuffer* buf);

private:
    FdIOClient* fd_;
    
    off_t cur_offset_;
};

} // namespace oasys

#endif /* __CHECKEDLOG_H__ */
