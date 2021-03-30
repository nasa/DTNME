/*
 *    Copyright 2007 Intel Corporation
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "Daemonizer.h"

namespace oasys {

//----------------------------------------------------------------------
void
Daemonizer::daemonize(bool wait_for_notify)
{
    /*
     * If we're running as a daemon, we fork the parent process as
     * soon as possible, and in particular, before TCL has been
     * initialized.
     *
     * Then, the parent waits on a pipe for the child to notify it as
     * to whether or not the initialization succeeded.
     */
    fclose(stdin);

    if (wait_for_notify) {
        if (pipe(pipe_) != 0) {
            fprintf(stderr, "error creating pipe for daemonize process: %s\n",
                    strerror(errno));
            exit(1);
        }
    }

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "error forking daemon process: %s\n",
                strerror(errno));
        exit(1);
    }

    if (pid > 0) {
        // if we're not waiting, the parent exits immediately
        if (! wait_for_notify) {
            exit(0);
        }

        // the parent closes the write half of the pipe, then waits
        // for the child to return its status on the read half
        close(pipe_[1]);
        
        int status;
        int count = read(pipe_[0], &status, sizeof(status));
        if (count != sizeof(status)) {
            fprintf(stderr, "error reading from daemon pipe: %s\n",
                    strerror(errno));
            exit(1);
        }
        
        close(pipe_[1]);
        exit(status);

    } else {
        // the child continues on in a new session, closing the
        // unneeded read half of the pipe
        if (wait_for_notify) {
            close(pipe_[0]);
        }
        setsid();
    }
}

//----------------------------------------------------------------------
void
Daemonizer::notify_parent(int status)
{
    write(pipe_[1], &status, sizeof(status));
    close(pipe_[1]);
}

} // namespace oasys
