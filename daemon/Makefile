#
#    Copyright 2004-2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

#
# Makefile for DTNME/daemon
#

#
# Make sure SRCDIR is set (.. by default)
#
ifeq ($(SRCDIR),)
SRCDIR   := ..
BUILDDIR := ..
endif

DAEMON_SRCS :=			\
	dtnd.cc

DAEMON_OBJS := $(DAEMON_SRCS:.cc=.o)

#
# Default target is to build the daemon
#
BINFILES := dtnd
all: $(BINFILES)

COMPONENT_LIBS := \
	../applib/libdtnapisrv.a 	\
	../servlib/libdtnserv.a 	\

dtnd: $(DAEMON_OBJS) $(COMPONENT_LIBS)
	$(CXX) $(CXXFLAGS) $(DAEMON_OBJS) $(COMPONENT_LIBS) \
		-o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS) $(LIBS)

#
# Include the common rules
#
include ../Rules.make
