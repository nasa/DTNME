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
# Makefile for DTNME/test
#

#
# Make sure SRCDIR is set.
#
ifeq ($(SRCDIR),)
SRCDIR   := ..
BUILDDIR := ..
endif

#
# Default target is to build all unit tests
#
dtn-tests: unit_tests
all: dtn-tests

BINFILES :=					\
	unit_tests/bundle-list-test		\
	unit_tests/bundle-list-intmap-test	\
	unit_tests/bundle-list-strmap-test	\
	unit_tests/bundle-list-strmultimap-test \
	unit_tests/bundle-payload-test		\
	unit_tests/bundle-protocol-test		\
	unit_tests/bundle-timestamp-test	\
	unit_tests/endpoint-id-test		\
	unit_tests/gbofid-test			\
	unit_tests/route-multigraph-test	\
	unit_tests/route-table-test		\
	unit_tests/sdnv-test			\
	unit_tests/sequence-id-test		\
	unit_tests/ecdh-test			\
	unit_tests/ipnd-sb-tlv-test		\

unit_tests: $(BINFILES)

#
# Include the servlib Makefile to get the list of servlib objects
#
SERVLIB_MAKEFILE_INCLUDE := true
include $(SRCDIR)/servlib/Makefile
SERVLIB_OBJS := $(patsubst %,$(SRCDIR)/servlib/%,$(SERVLIB_OBJS))

echoobjs:
	@echo $(SERVLIB_OBJS)

COMPONENT_LIBS := \
	$(BUILDDIR)/applib/libdtnapisrv.a 	\
	$(BUILDDIR)/servlib/libdtnserv.a 	\

#
# Default pattern rule to build the tests
#
.PRECIOUS: unit_tests/%-test.o
unit_tests/%-test.o: unit_tests/%-test.cc
	@rm -f $@; mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

unit_tests/%-test: unit_tests/%-test.o $(COMPONENT_LIBS)
	@rm -f $@; mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $< $(COMPONENT_LIBS) -o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS)

#
# As one exception, the bundle-timestamp-test can't have -Wcast-align
#
unit_tests/bundle-timestamp-test.o: unit_tests/bundle-timestamp-test.cc
	@rm -f $@; mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Wno-cast-align -c $< -o $@	

#
# Include the common rules
#
include ../Rules.make
