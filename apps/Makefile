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

SINGLE_SOURCE_APPS := 			\
	dtncat/dtncat			\
	dtncp/dtncp			\
	dtncpd/dtncpd			\
	dtnping/dtnping			\
	dtnping/dtntraceroute		\
	dtnping/dtnreporter		\
	dtnperf/dtnperf-server		\
	dtnpublish/dtnpublish		\
	dtnrecv/dtnrecv			\
	dtnpeek/dtnpeek			\
	dtnecho/dtnecho			\
	dtnquery/dtnquery   		\
	dtnrespond/dtnrespond  		\
	dtnsink/dtnsink			\
	num2sdnv/num2sdnv		\
	dtpc_apps/dtpc_send		\
	dtpc_apps/dtpc_recv		\

OTHER_APPS :=				\
	dtnmoteproxy/dtnmoteproxy	\
	dtntest/dtntest			\
	dtntunnel/dtntunnel		\
	dtnperf/dtnperf-client		\
	dtnsend/dtnsend			\
	dtnsource/dtnsource		\
	num2sdnv/sdnv2num		\

APPS := $(SINGLE_SOURCE_APPS) $(OTHER_APPS)

all: $(DTN_LIBS++) $(DTN_LIBS_STATIC) $(APPS) $(MANPAGES)
include ../Rules.make
include $(SRCDIR)/dtn-version.mk

DTN_LIBS_STATIC := ../applib/libdtnapi.a ../applib/libdtnapi++.a
DTN_LIBS := ../applib/libdtnapi.a $(OASYS_COMPAT_LDFLAGS)
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
# For dtnmoteproxy, don't worry about cast alignment
#
dtnmoteproxy/dtnmoteproxy.o: dtnmoteproxy/dtnmoteproxy.c
	@rm -f $@; mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Wno-cast-align $(CFLAGS) -c $< -o $@

dtnmoteproxy/serialsource.o: dtnmoteproxy/serialsource.c
	@rm -f $@; mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Wno-cast-align $(CFLAGS) -c $< -o $@

dtnmoteproxy/dtnmoteproxy: dtnmoteproxy/dtnmoteproxy.o \
			   dtnmoteproxy/serialsource.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS)

#
# Rules for dtntest
#
dtntest/dtntest: dtntest/dtntest.o  $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS++)
#
# Rules for dtnsend
#
dtnsend/dtnsend: dtnsend/dtnsend.o  $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS++)

#
# Rules for dtnsource
#
dtnsource/dtnsource: dtnsource/dtnsource.o  $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS++)

#
# num2sdnv and sdnv2num are the same app
#
num2sdnv/sdnv2num: num2sdnv/num2sdnv
	cp $< $@

#
# Rules for dtntunnel
#
dtntunnel/dtntunnel: dtntunnel/DTNTunnel.o \
		     dtntunnel/TCPTunnel.o \
		     dtntunnel/UDPTunnel.o \
		     $(DTN_LIBS_STATIC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS++)


#
# Rules for dtnperf-client
#
dtnperf/dtnperf-client: dtnperf/utils.o \
			dtnperf/bundle_tools.o \
			dtnperf/dtnperf-client.o \
		    $(DTN_LIBS_STATIC)

	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS++)


#
# Default rule for all single source apps
#
$(SINGLE_SOURCE_APPS): %: %.o $(DTN_LIBS_STATIC)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(DTN_LIBS)
