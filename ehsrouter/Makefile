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

### NOTE: ehsrouter/router.cc & .h are links to servlib/router/router.cc & .h
### NOTE: ehsrouter/SDNV.cc & .h are links to servlib/bundling/SDNV.cc & .h
EHSROUTER_SRCS := 			   \
	EhsBundle.cc			   \
	EhsBundleTree.cc           \
	EhsDtnNode.cc			   \
	EhsEventHandler.cc		   \
	EhsExternalRouter.cc       \
	EhsExternalRouterImpl.cc   \
	EhsLink.cc			       \
	EhsRouter.cc			   \
	EhsSrcDstKeys.cc           \
	EhsSrcDstWildBoolMap.cc    \
	router-custom.cc		   \
	router-wrap.cc			   \
	SDNV.cc				       \

EHSROUTER_OBJS := $(EHSROUTER_SRCS:.cc=.o)
EHSROUTER_OBJS := $(EHSROUTER_OBJS:.c=.o)
EHSROUTER_OBJS := $(EHSROUTER_OBJS) ../dtn-version.o

TESTEHSROUTER_SRCS :=		\
	test_ehsrouter.cc	\


TESTEHSROUTER_OBJS := $(TESTEHSROUTER_SRCS:.cc=.o)
TESTEHSROUTER_OBJS := $(TESTEHSROUTER_OBJS:.c=.o)
TESTEHSROUTER_OBJS := $(TESTEHSROUTER_OBJS) 

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
all: symlinks $(LIBFILES) $(BINFILES)


ehsrouter: symlinks libehsrouter.a $(BINFILES)

libehsrouter.a: $(EHSROUTER_OBJS)
	rm -f $@
	$(AR) ruc $@ $^
	$(RANLIB) $@ || true


test_ehsrouter: symlinks $(TESTEHSROUTER_OBJS) $(LIBFILES)
	$(CXX) $(CXXFLAGS) $(TESTEHSROUTER_OBJS) $(LIBFILES) \
		-o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS) $(LIBS)

symlinks: SDNV.cc SDNV.h ln_router.h

SDNV.cc:
	ln -s ../servlib/bundling/SDNV.cc SDNV.cc

SDNV.h:
	ln -s ../servlib/bundling/SDNV.h SDNV.h

ln_router.h:
	ln -s ../servlib/routing/router.h ln_router.h

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
