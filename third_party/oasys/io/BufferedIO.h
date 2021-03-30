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

#ifndef _OASYS_BUFFERED_IO_H_
#define _OASYS_BUFFERED_IO_H_

#include "../debug/Logger.h"
#include "../io/IOClient.h"
#include "../util/StreamBuffer.h"

namespace oasys {

/**
 * Wrapper class for an IOClient that includes an in-memory
 * buffer for reading and/or writing.
 */
class BufferedInput : public Logger {
public:
    BufferedInput(IOClient* client, const char* logbase = "/BufferedInput");
    ~BufferedInput();
    
    /** 
     * Read in a line of input, newline characters included. 
     * 
     * @param nl  	character string that defines a newline
     * @param buf 	output parameter containing a pointer to the buffer
     * 			with the line, valid until next call to read_line
     * @param timeout 	timeout value for read
     
     * @return		length of line, including nl characters. <0 on error,
     *                  0 on eof.
     */
    int read_line(const char* nl, char** buf, int timeout = -1);

    /**
     * Read len bytes. Blocking until specified amount of bytes is
     * read.
     *
     * @param len  	length to read
     * @param buf 	output parameter containing a pointer to the buffer
     * 			with the line, valid until next call to read_line
     * @param timeout 	timeout value for read
     *
     * @return 		length of segment read. <0 upon error, 0 on eof.
     *                  Return  will only be < len if eof is reached before 
     * 			fully read len bytes.
     */
    int read_bytes(size_t len, char** buf, int timeout = -1);


    /**
     * Read some bytes. 
     *
     * @param buf 	output parameter containing a pointer to the buffer
     * 			with the line, valid until next call to read_line
     * @param timeout 	timeout value for read
     * @return 		length of segment read. <0 upon error, 0 on eof.
     */
    int read_some_bytes(char** buf, int timeout = -1);

    /**
     * Read in a single character from the protocol stream. Returns 0
     * if at the end of the stream or error.
     */
    char get_char(int timeout = -1);

    /**
     * Returns true if at the end of file.
     */
    bool eof();

private:    
    /**
     * Read in len bytes into the buffer. If there are enough bytes
     * already present in buf_, no call to read will occur.
     *
     * \param len The amount to read
     * \param timeout_ms Timeout to the read call. UNIMPLEMENTED
     * \return Bytes available, can be less than len.
     */
    int internal_read(size_t len = 0, int timeout_ms = -1);

    /**
     * \return Index of the start of the sequence of the newline
     * character string
     */
    int find_nl(const char* nl);
    
    IOClient*    client_;
    StreamBuffer buf_;

    bool seen_eof_;

    static const size_t READ_AHEAD = 256;  //! Amount to read when buffer is full
    static const size_t MAX_LINE   = 4096; //! Maximum line length
};

class BufferedOutput : public Logger {
public:
    BufferedOutput(IOClient* client, const char* logbase = "/BufferedOutput");

    /**
     * Write len bytes from bp. Output may be buffered. If len is zero,
     * calls strlen() to determine the length
     *
     * \return the number of bytes successfully written.
     */
    int write(const char* bp, size_t len = 0);

    /**
     * Clears the buffer contents without writing.
     */
    void clear_buf();

    /**
     * Fills the buffer via printf style args, returning the length or
     * -1 if there's an error.
     */
    int format_buf(const char* format, ...) PRINTFLIKE(2, 3);
    int vformat_buf(const char* format, va_list args);
        
    /**
     * Writes the full buffer, potentially in multiple calls to write.
     * \return <0 on error, otherwise the number of bytes written
     */
    int flush();
    
    /**
     * If the buffer reaches size > limit, then the buffer is
     * automatically flushed. If the limit is 0, then it never
     * auto-flushes.
     */
    void set_flush_limit(size_t limit);
    
    /**  
     * Do format_buf() and flush() in one call.
     */
    int printf(const char* format, ...) PRINTFLIKE(2, 3);

private:
    IOClient*    client_;
    StreamBuffer buf_;

    size_t flush_limit_;

    static const size_t DEFAULT_FLUSH_LIMIT = 256;
};

} // namespace oasys

#endif /* _OASYS_BUFFERED_IO_H_ */
