/*
 *    Copyright 2006 Intel Corporation
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

#ifndef __LOGSINK_H__
#define __LOGSINK_H__

namespace oasys {

//----------------------------------------------------------------------------
/*! 
 * Sink class for logging, implements the standard file descriptor
 * style logging, as well as ring buffering for time of crash
 * logging.
 */
class LogSink {
public:
    virtual ~LogSink() {}
    
    virtual void rotate()       = 0;
    virtual void log(char* str) = 0;
};

//----------------------------------------------------------------------------
/*!
 * Fork sink for having more than one sink being signaled.
 */
class ForkSink : public LogSink {
public:
    ForkSink(LogSink* a, LogSink* b)
        : a_(a), b_(b) {}

    void rotate() { 
        a->rotate(); 
        b->rotate(); 
    }

    void log(const char* str) {
        a->log(str);
        b->log(str);
    }
};

//----------------------------------------------------------------------------
/*!
 * Sink for writing to file descriptor logging targets.
 */
class FileLogSink : public LogSink {
public:
    /*! 
     * Open FileLogSink. 
     * @param filename "-" means standard out
     */
    FileLogSink(const char* filename);
    ~FileLogSink();

    void rotate();
    void log(const char* str);

private:
    char filename_[256];
    int  fd_;
};

//----------------------------------------------------------------------------
class RingBufferLogSink : public LogSink {
public:
    RingBufferLogSink();

    void rotate() {}
    void log(char* str);

    static const int NUM_BUFFERS = 32;
    static const int BUF_LEN     = 512;

private:
    int   cur_buf_;
    char* buf_;
    char  static_buf_[NUM_BUFFERS * BUF_LEN + 16];

    size_t buf(int i) {
        return &buf_[i * BUF_LEN];
    }
};

} // namespace oasys

#endif /* __LOGSINK_H__ */
