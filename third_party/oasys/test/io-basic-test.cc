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

#include "io/IO.h"
#include "io/TCPClient.h"
#include "io/TCPServer.h"
#include "thread/Notifier.h"
#include "thread/Thread.h"

#include "util/Random.h"
#include "util/UnitTest.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace oasys;

#define TESTBUF_SIZE (10*1024*1024)
char* g_testbuf;
char* g_scratchbuf;

void
generate_lengths(size_t* length_array, int array_size,
                 size_t total_bytes, int seed = 0) 
{
    Random::seed(seed);

    int* sizes = static_cast<int*>(calloc(array_size, sizeof(int)));
    
    size_t total = 0;
    for (int i=0; i<array_size; ++i) {
        sizes[i] = Random::rand(99 + 1);
        total += sizes[i];
    }
    
    size_t length_total = 0;
    for (int i=0; i<array_size; ++i) {
        length_array[i] = (sizes[i] * total_bytes)/total;
        length_total += length_array[i];
    }
    
    ASSERT(length_total <= total_bytes);
    length_array[Random::rand(array_size)] += total_bytes - length_total;

    free(sizes);

    /*
    for (int i=0; i<array_size; ++i) {
        printf("%u ", length_array[i]);
    }
    printf("\n"); */
}

void
reset_data() {
    for (size_t i=0; i<TESTBUF_SIZE; ++i) {
        g_testbuf[i] = Random::rand();
    }
    
    memset(g_scratchbuf, 0, TESTBUF_SIZE);
}

// need to drain the pipe sometimes if a thread gets blocked
void
drain_pipe(int fd) {
    char buf[1024];
    int  cc;

    IO::set_nonblocking(fd, true);

    do {
        cc = read(fd, buf, 1024);
    } while(cc > 0 || (cc < 0 && errno != EAGAIN));
}

int
writeall(int fd, const void* buf, size_t length) 
{
    const char* ptr = (const char*) buf;
    size_t total = length;
    
    while (length > 0) {
        int cc = write(fd, ptr, length);
        if (cc < 0 && (errno == EINTR || errno == EAGAIN)) { continue; }
        if (cc <= 0) { return cc; }
        
        length -= cc;
        ptr    += cc;
    }

    return total;
}

int
verify_readall(int fd, size_t length, size_t data_offset) 
{
    while (length > 0) {
        int cc = read(fd, g_scratchbuf, length);
        if (cc < 0 && (errno == EINTR || errno == EAGAIN)) { continue; }
        if (cc <= 0) { return cc; }

        if (memcmp(g_testbuf + data_offset, g_scratchbuf, cc) != 0) {
            return -1;
        }
        data_offset += cc;
        length      -= cc;
    }

    return data_offset;
}

class IOTester {
public:
    IOTester() : status_(0) {}
    virtual ~IOTester() {}

    void run() {
        Writer w(this);
        Reader r(this);

        w.start();
        r.start();
        
        w.join();
        r.join();
    }

    int status() { return status_; }
    void fail()  { status_ = UNIT_TEST_FAILED; }

protected:
    virtual void run_reader() = 0;
    virtual void run_reader_pre()  {}
    virtual void run_reader_post() {}

    virtual void run_writer() = 0;
    virtual void run_writer_pre()  {}
    virtual void run_writer_post() {}
    

    struct Writer : public Thread {
        Writer(IOTester* target) 
            : Thread("Writer", CREATE_JOINABLE), target_(target) {}

        void run() { 
            target_->run_writer_pre();
            target_->run_writer(); 
            target_->run_writer_post();
        }

        IOTester* target_;
    };

    struct Reader : public Thread {
        Reader(IOTester* target) 
            : Thread("Reader", CREATE_JOINABLE),target_(target) {}

        void run() { 
            target_->run_reader_pre();
            target_->run_reader(); 
            target_->run_reader_post();
        }

        IOTester* target_;
    };

    int status_;
};

class PipeIOTester : public IOTester {
public:
    PipeIOTester() {
        int err = pipe(fds_);
        ASSERT(err == 0);        
    }

    ~PipeIOTester() {
        close(fds_[0]);
        close(fds_[1]);
    }

    void run_reader_post() {
        drain_pipe(fds_[0]);
    }
    
protected:
    int fds_[2];
};

DECLARE_TEST(Init) {
    Random::seed(0);

    g_testbuf    = static_cast<char*>(malloc(TESTBUF_SIZE));
    g_scratchbuf = static_cast<char*>(malloc(TESTBUF_SIZE));

    reset_data();

    return UNIT_TEST_PASSED;
}

struct ReadRunner : public PipeIOTester {
    static const int OPS = 20;

    ReadRunner() {
        generate_lengths(rlengths, OPS, 10000, 1);
        generate_lengths(wlengths, OPS, 10000, 2);
    }
    
    void run_reader() {
        size_t offset = 0;

        for (int i=0; i<OPS; ++i) {
            int cc = IO::read(fds_[0], g_scratchbuf, rlengths[i], 0, "/read");
            
            if (cc <= 0) { 
                logf("/read", LOG_ERR, "read on fd %d error %d %s", 
                     fds_[0], cc, strerror(errno));
                fail(); 
            } else {
                if (memcmp(g_testbuf + offset, g_scratchbuf, cc) != 0) {
                    logf("/read", LOG_ERR, "read memory not equal");
                    fail();
                }
                offset += cc;
            }
        }
    }   

    void run_writer() {
        size_t offset = 0;
        
        for (int i=0; i<OPS; ++i) {
            int cc = writeall(fds_[1], g_testbuf + offset, wlengths[i]);
            if (cc <= 0) { 
                logf("/write", LOG_ERR, "write() on fd %d failed %s", 
                     fds_[1], strerror(errno));
                fail(); 
            }
            
            offset += wlengths[i];
        }
    }
    size_t rlengths[OPS];
    size_t wlengths[OPS];
};

DECLARE_TEST(Read) {
    reset_data();    
    ReadRunner r;
    r.run();

    return r.status();
}

struct ReadVRunner : public PipeIOTester {
    static const int total = 10000;
    
    void run_reader() {
        size_t iovec_lengths[10];
        generate_lengths(iovec_lengths, 10, 10000);
        
        struct iovec iov[10];
        size_t offset = 0;
        for (int i=0; i<10; ++i) {
            iov[i].iov_base = g_scratchbuf + offset;
            iov[i].iov_len  = iovec_lengths[i];
            offset += iovec_lengths[i];
        }

        int cc = IO::readv(fds_[0], iov, 10, 0, "/read");
        if (cc <= 0) { fail(); return; }
        if (memcmp(g_testbuf, g_scratchbuf, cc) != 0) {
            logf("/read", LOG_ERR, "read memory not equal");
            fail();
        }
    }   

    void run_writer() {
        int err = writeall(fds_[1], g_testbuf, 10000);
        ASSERT(err > 0);
    }
};


DECLARE_TEST(ReadV) {
    reset_data();
    ReadVRunner r;
    r.run();

    return r.status();
}

struct ReadIntrRunner : public PipeIOTester {
    static const int total = 10000;

    ReadIntrRunner()
        : intr("ReadIntrRunner")
    {
        int err = pipe(fds_);
        ASSERT(err == 0);
    }
    
    void run_reader() {
        for (int i=0; i<2; ++i) {
            int cc = IO::read(fds_[0], g_scratchbuf, 200, &intr, "/read");
            if (cc == IOINTR) { break; }
            if (cc < 0)       { fail(); break; }
            
            if (memcmp(g_testbuf, g_scratchbuf, cc) != 0) {
                logf("/read", LOG_ERR, "read memory not equal");
                fail();
            }
        }
    }   

    void run_writer() {
        int err = writeall(fds_[1], g_testbuf, 100);
        ASSERT(err > 0);

        usleep(1000000);
        intr.notify();
    }
    Notifier intr;
};

DECLARE_TEST(ReadIntr) {
    reset_data();
    ReadIntrRunner r;
    r.run();

    return r.status();
}

struct ReadTimeoutRunner : public PipeIOTester {
    static const int total = 10000;

    void run_reader() {
        int cc;

        for (int i=0; i<2; ++i) {
            cc = IO::timeout_read(fds_[0], g_scratchbuf, 200, 1000, 0, "/read");
            
            if (cc == IOTIMEOUT) { break; }
            if (cc < 0)          { fail(); break; }
            
            if (memcmp(g_testbuf, g_scratchbuf, cc) != 0) {
                logf("/read", LOG_ERR, "read memory not equal");
                fail();
            }           
        }
        if (cc != IOTIMEOUT) { fail(); }
    }   

    void run_writer() {
        int err = writeall(fds_[1], g_testbuf, 100);
        ASSERT(err > 0);
    }
};

DECLARE_TEST(ReadTimeout) {    
    reset_data();
    ReadTimeoutRunner r;
    r.run();

    return r.status();
}


struct ReadTimeoutIntrRunner : public PipeIOTester {
    ReadTimeoutIntrRunner()
        : intr("ReadTimeoutIntrRunner") {}
    
    static const int total = 10000;
    
    void run_reader() {
        int cc;

        for (int i=0; i<2; ++i) {
            cc = IO::timeout_read(fds_[0], g_scratchbuf, 200, 1000, &intr, "/read");

            if (cc == IOINTR) {
                if(i != 0) { fail(); }
                continue;
            }
            if (cc == IOTIMEOUT)        { break; }
            if (cc < 0)                 { fail(); break; }
        }
        if (cc != IOTIMEOUT) { fail(); }
    }   

    void run_writer() {
        usleep(500000);
        intr.notify();
    }
    Notifier intr;
};

DECLARE_TEST(ReadTimeoutIntr) {
    reset_data();
    ReadTimeoutIntrRunner r;
    r.run();

    return r.status();
}

struct ReadAllRunner : public PipeIOTester {
    static const int OPS = 20;

    ReadAllRunner() {
        generate_lengths(rlengths, OPS, 10000, 1);
        generate_lengths(wlengths, OPS, 10000, 2);
    }
    
    void run_reader() {
        size_t offset = 0;

        for (int i=0; i<OPS; ++i) {
            int cc = IO::readall(fds_[0], g_scratchbuf, rlengths[i], 0, "/read");
            
            if (cc <= 0) { 
                logf("/read", LOG_ERR, "read on fd %d error %d %s", 
                     fds_[0], cc, strerror(errno));
                fail(); 
            } else {
                if (cc != (int)rlengths[i]) { fail(); }
                if (memcmp(g_testbuf + offset, g_scratchbuf, cc) != 0) {
                    logf("/read", LOG_ERR, "read memory not equal");
                    fail();
                }
                offset += cc;
            }
        }
    }   

    void run_writer() {
        size_t offset = 0;
        
        for (int i=0; i<OPS; ++i) {
            int cc = writeall(fds_[1], g_testbuf + offset, wlengths[i]);
            if (cc <= 0) { 
                logf("/write", LOG_ERR, "write() on fd %d failed %s", 
                     fds_[1], strerror(errno));
                fail(); 
            }
            
            offset += wlengths[i];
        }
    }
    size_t rlengths[OPS];
    size_t wlengths[OPS];
};

DECLARE_TEST(ReadAll) {
    reset_data();
    ReadAllRunner r;
    r.run();
    
    return r.status();
}

struct ReadVAllRunner : public PipeIOTester {
    void run_reader() {
        size_t offset      = 0;
        size_t read_offset = 0;
                
        for (int j=0; j<10; ++j) 
        {
            size_t iovec_lengths[10];
            generate_lengths(iovec_lengths, 10, 10000);
            
            offset = 0;
            struct iovec iov[10];
            for (int i=0; i<10; ++i) {
                iov[i].iov_base = g_scratchbuf + offset;
                iov[i].iov_len  = iovec_lengths[i];
                offset += iovec_lengths[i];
            }
            
            int cc = IO::readvall(fds_[0], iov, 10, 0, "/read");

            if (cc <= 0)     { fail(); return; }
            if (cc != 10000) { fail(); }
            
            if (memcmp(g_testbuf + read_offset, g_scratchbuf, cc) != 0) {
                logf("/read", LOG_ERR, "read memory not equal");
                fail();
            } else {
                logf("/read", LOG_DEBUG, "read memory is equal");
            }
            read_offset += cc;
        }
    }

    void run_writer() {
        size_t offset = 0;
        size_t wlengths[40];
        
        generate_lengths(wlengths, 40, 100000);

        for (int i=0; i<40; ++i) {
            int cc = writeall(fds_[1], g_testbuf + offset, wlengths[i]);
            usleep(10000);
            if (cc <= 0) { 
                logf("/write", LOG_ERR, "write() on fd %d failed %s", 
                     fds_[1], strerror(errno));
                fail(); 
            }
            
            offset += wlengths[i];
        }
    }
};

DECLARE_TEST(ReadVAll) {
    reset_data();
    ReadVAllRunner r;
    r.run();
    
    return r.status();
}

struct ReadVAllTimeoutRunner : public PipeIOTester {
    void run_reader() {
        size_t offset      = 0;
        size_t read_offset = 0;
                
        for (int j=0; j<10; ++j) 
        {
            size_t iovec_lengths[10];
            generate_lengths(iovec_lengths, 10, 10000);
            
            offset = 0;
            struct iovec iov[10];
            for (int i=0; i<10; ++i) {
                iov[i].iov_base = g_scratchbuf + offset;
                iov[i].iov_len  = iovec_lengths[i];
                offset += iovec_lengths[i];
            }
            
            int cc = IO::timeout_readvall(fds_[0], iov, 10, 1000, 0, "/read");

            if (cc <= 0) { 
                if (! (cc == IOTIMEOUT && j==9)) {
                    fail(); 
                }
                return; 
            }
            
            if (cc != 10000) { fail(); }
            
            if (memcmp(g_testbuf + read_offset, g_scratchbuf, cc) != 0) {
                logf("/read", LOG_ERR, "read memory not equal");
                fail();
            } else {
                logf("/read", LOG_DEBUG, "read memory is equal");
            }
            read_offset += cc;
        }
    }

    void run_writer() {
        size_t offset = 0;
        size_t wlengths[40];
        
        generate_lengths(wlengths, 40, 95000);

        for (int i=0; i<40; ++i) {
            int cc = writeall(fds_[1], g_testbuf + offset, wlengths[i]);
            usleep(10000);
            if (cc <= 0) { 
                logf("/write", LOG_ERR, "write() on fd %d failed %s", 
                     fds_[1], strerror(errno));
                fail(); 
            }
            
            offset += wlengths[i];
        }
    }
};

DECLARE_TEST(ReadVAllTimeout) {
    reset_data();
    ReadVAllTimeoutRunner r;
    r.run();

    return r.status();
}

struct WriteTestRunner : public PipeIOTester {
    static const int OPS = 20;

    void run_reader() {
        size_t lengths[OPS];
        size_t offset = 0;

        generate_lengths(lengths, OPS, 5000);

        for (int i=0; i<OPS; ++i) {
            offset = verify_readall(fds_[0], lengths[i], offset);
        }
    }
    
    void run_writer() {
        size_t lengths[OPS];
        size_t offset = 0;

        generate_lengths(lengths, OPS, 10000);

        for (int i=0; i<OPS; ++i) {
            int cc = IO::write(fds_[1], g_testbuf + offset, lengths[i]);
            if (cc <= 0) { fail(); break; }
            offset += cc;
        }        
    }
};
                
// XXX/bowei finish these tests
DECLARE_TEST(WriteTest)        { return UNIT_TEST_PASSED; }
DECLARE_TEST(WriteVTest)       { return UNIT_TEST_PASSED; }
DECLARE_TEST(WriteIntr)        { return UNIT_TEST_PASSED; }
DECLARE_TEST(WriteTimeout)     { return UNIT_TEST_PASSED; }
DECLARE_TEST(WriteTimeoutIntr) { return UNIT_TEST_PASSED; }

DECLARE_TESTER(IoBasicTester) {
    ADD_TEST(Init);

    ADD_TEST(Read);
    ADD_TEST(ReadV);
    ADD_TEST(ReadIntr);
    ADD_TEST(ReadTimeout);
    ADD_TEST(ReadTimeoutIntr);
    ADD_TEST(ReadAll);
    ADD_TEST(ReadVAll);
    ADD_TEST(ReadVAllTimeout);
    
    ADD_TEST(WriteTest);
    ADD_TEST(WriteVTest);
    ADD_TEST(WriteIntr);
    ADD_TEST(WriteTimeout);
    ADD_TEST(WriteTimeoutIntr);
    
    // XXX/bowei test socket receive fcns
}
DECLARE_TEST_FILE(IoBasicTester, "IO Basic Test");
