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
# Makefile for DTNME/ehsrouter
#

#
# Source and object rules
#

### NOTE: ehsrouter/SDNV.cc & .h are links to servlib/bundling/SDNV.cc & .h
EHSROUTER_SRCS :=			\
	EhsBundle.cc			\
	EhsBundleTree.cc		\
	EhsDtnNode.cc			\
	EhsEventHandler.cc		\
	EhsExternalRouter.cc		\
	EhsExternalRouterImpl.cc	\
	EhsLink.cc			\
	EhsRouter.cc			\
	EhsSrcDstKeys.cc		\
	EhsSrcDstWildBoolMap.cc		\


EHSROUTER_DEPENDENT_OBJS :=			\
	../dtn-version.o \
	../servlib/bundling/CborUtil.o \
	../servlib/bundling/FormatUtils.o \
	../servlib/bundling/SDNV.o \
	../servlib/routing/ExternalRouterIF.o \
	../servlib/routing/ExternalRouterClientIF.o \

EHSROUTER_OBJS := $(EHSROUTER_SRCS:.cc=.o)
EHSROUTER_OBJS := $(EHSROUTER_OBJS:.c=.o)
EHSROUTER_OBJS := $(EHSROUTER_OBJS) $(EHSROUTER_DEPENDENT_OBJS)


#EHSROUTER_OBJS := $(EHSROUTER_OBJS) ../dtn-version.o ../servlib/bundling/CborUtil.o ../servlib/routing/ExternalRouterIF.o ../servlib/routing/ExternalRouterClientIF.o ../servlib/bundling/SDNV.o

TESTEHSROUTER_SRCS :=		\
	test_ehsrouter.cc	\


TESTEHSROUTER_OBJS := $(TESTEHSROUTER_SRCS:.cc=.o)
TESTEHSROUTER_OBJS := $(TESTEHSROUTER_OBJS:.c=.o)
TESTEHSROUTER_OBJS := $(TESTEHSROUTER_OBJS) $(EHSROUTER_OBJS)

##../servlib/bundling/CborUtil.o ../servlib/routing/ExternalRouterIF.o ../servlib/routing/ExternalRouterClientIF.o ../servlib/bundling/SDNV.o

BINFILES := test_ehsrouter

ALLSRCS := $(EHSROUTER_SRCS) $(TESTEHSROUTER_SRCS)

#
# Other makefiles include this one so they can link with the object
# files directly and not use the servlib, but in that case, all we
# want are the object lists, not the rules below
#
ifeq ($(EHSROUTER_MAKEFILE_INCLUDE),)

#
# Default target is to build the library
#
LIBFILES := libehsrouter.a
all: $(LIBFILES) $(BINFILES)


ehsrouter: libehsrouter.a $(BINFILES)

libehsrouter.a: $(EHSROUTER_OBJS)
	rm -f $@
	$(AR) ruc $@ $^
	$(RANLIB) $@ || true


test_ehsrouter: $(TESTEHSROUTER_OBJS) $(LIBFILES)
	$(CXX) $(CXXFLAGS) $(TESTEHSROUTER_OBJS) $(LIBFILES) \
		-o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS) $(LIBS)


#
# Make sure SRCDIR is set (.. by default)
#
ifeq ($(SRCDIR),)
SRCDIR   := ..
BUILDDIR := ..
endif

#
# Include the common rules.
#
include ../Rules.make

endif
