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
# Top level makefile for DTNME.
#

#
# Define a set of dispatch rules that simply call make on the
# constituent subdirectories. Note that the test directory isn't built
# by default.
#

SUBDIRS := third_party/oasys third_party/tinycbor applib servlib daemon apps ehsrouter

all: checkconfigure $(SUBDIRS)

#
# If srcdir/builddir aren't set by some other makefile, 
# then default to .
#
ifeq ($(SRCDIR),)
SRCDIR	 := .
BUILDDIR := .
endif

#
# Include the common rules
#
include Rules.make
include $(SRCDIR)/dtn-version.mk


# Check if we should include DTPC apps
include $(SRCDIR)/servlib/dtpc.make
DTPC_APPS :=
ifeq ($(DTPC_ENABLED),1)
DTPC_APPS := apps/dtpc_apps/dtpc_send apps/dtpc_apps/dtpc_recv
endif


#third_party/oasys: third_party/oasys/oasys-version.o
third_party/oasys/oasys-version.o: third_party/oasys/oasys-version.h 
	pushdir ./third_party/oasys
	./configure
	$(MAKE)
	popdir

#third_party/tinycbor: third_party/tinycbor/src/lib/libtinycbor.a
third_party/tinycbor/src/lib/libtinycbor.a:
	pushdir ./third_party/tinycbor
	$(MAKE)
	popdir


#
# Dependency rules between subdirectories needed for make -j
#
applib servlib ehsrouter: third_party/oasys third_party/tinycbor dtn-version.o
daemon: applib servlib
apps: applib servlib

#
# Rules for the version files
#
dtn-version.o: dtn-version.c
dtn-version.c: dtn-version.h
dtn-version.h: dtn-version.h.in dtn-version.dat   FORCE
	$(OASYS_ETCDIR)/tools/subst-version $(SRCDIR)/dtn-version.dat \
		< $(SRCDIR)/dtn-version.h.in > dtn-version.h

.PHONY: FORCE
FORCE:

vpath dtn-version.h.in $(SRCDIR)
vpath dtn-version.h    $(SRCDIR)
vpath dtn-version.c    $(SRCDIR)
vpath dtn-version.dat  $(SRCDIR)

#
# Installation rules
#
install: installdirs installbin installlib installetc

installdirs:
	for dir in $(DESTDIR)$(localstatedir)/dtn \
		   $(DESTDIR)$(localstatedir)/dtn/bundles \
		   $(DESTDIR)$(localstatedir)/dtn/db ; do \
	    (mkdir -p $$dir; chmod 777 $$dir; \
		[ x$(DTN_USER) = x ] || chown $(DTN_USER) $$dir); \
	done

	[ -d $(DESTDIR)$(bindir) ] || \
	    (mkdir -p $(DESTDIR)$(bindir); chmod 777 $(DESTDIR)$(bindir))

	[ -d $(DESTDIR)$(libdir) ] || \
	    (mkdir -p $(DESTDIR)$(libdir); chmod 777 $(DESTDIR)$(libdir))

installbin: installdirs
	for prog in daemon/dtnme \
                    apps/deliver_me/deliver_me \
                    apps/echo_me/echo_me \
                    apps/dtpc_send_me/dtpc_send_me \
                    apps/dtpc_recv_me/dtpc_recv_me \
                    apps/ping_me/ping_me \
                    apps/recv_me/recv_me \
                    apps/report_me/report_me \
                    apps/sdnv_convert_me/sdnv_convert_me \
                    apps/send_me/send_me \
                    apps/sink_me/sink_me \
                    apps/source_me/source_me \
                    apps/trace_me/trace_me \
                    apps/tunnel_me/tunnel_me \
                    ehsrouter/test_ehsrouter; do \
	    ($(INSTALL_PROGRAM) $$prog $(DESTDIR)$(bindir)) ; \
	done

	[ x$(DTN_USER) = x ] || chown -R $(DTN_USER) $(DESTDIR)$(bindir)

installlib: installdirs
	[ x$(SHLIBS) = x ] || \
	for lib in applib/libdtnapi-$(DTN_VERSION).$(SHLIB_EXT) \
	           applib/libdtnapi++-$(DTN_VERSION).$(SHLIB_EXT) \
	           applib/tcl/libdtntcl-$(DTN_VERSION).$(SHLIB_EXT) ; do \
	    ($(INSTALL_PROGRAM) $$lib $(DESTDIR)$(libdir)) ; \
	done

	for lib in libdtnapi libdtnapi++ libdtntcl ; do \
		(cd $(DESTDIR)$(libdir) && rm -f $$lib.$(SHLIB_EXT) && \
		 ln -s $$lib-$(DTN_VERSION).$(SHLIB_EXT) $$lib.$(SHLIB_EXT)) \
	done


installetc: installdirs
	[ -d $(DESTDIR)$(sysconfdir) ] || mkdir -p $(DESTDIR)$(sysconfdir)
		$(INSTALL_DATA) daemon/dtn.conf $(DESTDIR)$(sysconfdir)/dtn.conf; \

#
# And a rule to make sure that configure has been run recently enough.
#
.PHONY: checkconfigure
checkconfigure: Rules.make

Rules.make: $(SRCDIR)/configure $(OASYS_ETCDIR)/Rules.make.in 
	@[ ! x`echo "$(MAKECMDGOALS)" | grep clean` = x ] || \
	(echo "$@ is out of date, need to rerun configure" && \
	exit 1)

$(SRCDIR)/configure $(OASYS_ETCDIR)/Rules.make.in:
	@echo SRCDIR: $(SRCDIR)
	@echo error -- Makefile did not set SRCDIR properly
	@exit 1

CFGFILES = Rules.make System.make 
