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
# Makefile for DTNME/apps
#

ifeq ($(SRCDIR),)
BUILDDIR := ..
SRCDIR := ..
endif

SINGLE_SOURCE_APPS := 						\
        ping_me/ping_me                 	\
        recv_me/recv_me                 	\
        report_me/report_me                	\
        trace_me/trace_me               	\
        sink_me/sink_me                 	\
        echo_me/echo_me                 	\
        sdnv_convert_me/sdnv_convert_me 	\
        dtpc_send_me/dtpc_send_me			\
        dtpc_recv_me/dtpc_recv_me			\

OTHER_APPS :=			    	        	\
        send_me/send_me         			\
        source_me/source_me                	\
        tunnel_me/tunnel_me             	\
        deliver_me/deliver_me               \

APPS := $(SINGLE_SOURCE_APPS) $(OTHER_APPS)

all: $(DTN_LIBS++) $(DTN_LIBS_STATIC) $(APPS) $(MANPAGES)
include ../Rules.make
include $(SRCDIR)/dtn-version.mk

DTN_LIBS_STATIC := ../applib/libdtnapi.a ../applib/libdtnapi++.a
DTN_LIBS := ../applib/libdtnapi.a $(OASYS_COMPAT_LDFLAGS) $(EXTLIB_LDFLAGS)
DTN_LIBS++ := $(DTN_LIBS) ../applib/libdtnapi++.a $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS)

# Fake build target in case the api lib doesn't exist, but allows us
# to include it as a dependency above so the apps get properly rebuilt
# for the 'all' target
$(DTN_LIBS_STATIC):
	@echo "ERROR: $@ must be built before the apps can"
	exit 1

CFLAGS += -I$(SRCDIR)/applib
CXXFLAGS += -I$(SRCDIR)/applib
LDFLAGS += -L../applib -lrt

BINFILES += $(APPS)

#
# Rules for send_me
#
send_me/send_me: send_me/send_me.o  $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS++)


#
# Rules for dtntunnel
#
tunnel_me/tunnel_me: tunnel_me/Tunnel_Me.o \
		     tunnel_me/TCPTunnel.o \
		     tunnel_me/UDPTunnel.o \
		     $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS++)

#
# Rules for source_me
#
source_me/source_me: source_me/source_me.o  $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS++)

#
# Rules for deliver_me
#
deliver_me/deliver_me: deliver_me/deliver_me.o \
                         $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS++)

# Default rule for all single source apps
#
$(SINGLE_SOURCE_APPS): %: %.o $(DTN_LIBS_STATIC)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS) -lpthread
