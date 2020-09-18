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

SUBDIRS := applib servlib daemon apps sim ehsrouter

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

#
# Dependency rules between subdirectories needed for make -j
#
applib servlib ehsrouter: dtn-version.o
daemon: applib servlib
apps: applib servlib
sim: servlib

#
# Rules for the version files
#
dtn-version.o: dtn-version.c
dtn-version.c: dtn-version.h
dtn-version.h: dtn-version.h.in dtn-version.dat
	$(OASYS_ETCDIR)/tools/subst-version $(SRCDIR)/dtn-version.dat \
		< $(SRCDIR)/dtn-version.h.in > dtn-version.h

vpath dtn-version.h.in $(SRCDIR)
vpath dtn-version.h    $(SRCDIR)
vpath dtn-version.c    $(SRCDIR)
vpath dtn-version.dat  $(SRCDIR)

bump-version:
	cd $(SRCDIR) && tools/bump-version dtn-version.dat

#
# Test rules
#
test: tests
tests: 
	$(MAKE) all
	$(MAKE) -C test

dtn-tests:
	$(MAKE) -C test

#
# Installation rules
#
install: installdirs installbin installlib installetc

installdirs:
	for dir in $(DESTDIR)$(localstatedir)/dtn \
		   $(DESTDIR)$(localstatedir)/dtn/bundles \
		   $(DESTDIR)$(localstatedir)/dtn/db ; do \
	    (mkdir -p $$dir; chmod 755 $$dir; \
		[ x$(DTN_USER) = x ] || chown $(DTN_USER) $$dir); \
	done

	[ -d $(DESTDIR)$(bindir) ] || \
	    (mkdir -p $(DESTDIR)$(bindir); chmod 755 $(DESTDIR)$(bindir))

	[ -d $(DESTDIR)$(libdir) ] || \
	    (mkdir -p $(DESTDIR)$(libdir); chmod 755 $(DESTDIR)$(libdir))

installbin: installdirs
	for prog in daemon/dtnd \
		    tools/dtnd-control \
		    apps/dtncat/dtncat \
		    apps/dtncp/dtncp \
		    apps/dtncpd/dtncpd \
		    apps/dtnhttpproxy/dtnhttpproxy \
		    apps/dtnpeek/dtnpeek \
		    apps/dtnperf/dtnperf-client \
		    apps/dtnperf/dtnperf-server \
		    apps/dtnping/dtnping \
		    apps/dtnping/dtntraceroute \
		    $(DTPC_APPS) \
		    apps/dtnpublish/dtnpublish \
		    apps/dtnsource/dtnsource \
		    apps/dtnrecv/dtnrecv \
		    apps/dtnsend/dtnsend \
		    apps/dtnsink/dtnsink \
		    apps/dtntunnel/dtntunnel \
		    apps/num2sdnv/num2sdnv \
		    apps/num2sdnv/sdnv2num \
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
	if [ -z $(PYTHON_BUILD_EXT) ]; then \
	cd applib && make perlapi_install; \
	else \
	cd applib && make pythonapi_install && make perlapi_install; \
	fi


installetc: installdirs
	[ -d $(DESTDIR)$(sysconfdir) ] || mkdir -p $(DESTDIR)$(sysconfdir)
	if [ -f $(DESTDIR)$(sysconfdir)/dtn.conf ]; then \
		echo "WARNING: $(DESTDIR)$(sysconfdir)/dtn.conf exists -- not overwriting"; \
	else \
		$(INSTALL_DATA) daemon/dtn.conf $(DESTDIR)$(sysconfdir)/dtn.conf; \
	fi

	$(INSTALL_DATA) servlib/routing/router.xsd $(DESTDIR)$(sysconfdir)/router.xsd
	$(INSTALL_DATA) servlib/conv_layers/clevent.xsd $(DESTDIR)$(sysconfdir)/clevent.xsd

xsdbindings:
	$(MAKE) -C servlib xsdbindings

#
# Build a TAGS database. Note this includes all the sources so it gets
# header files as well.
#
.PHONY: TAGS tags
tags TAGS:
	cd $(SRCDIR) && \
	find . -name \*.h -or -name \*.c -or -name \*.cc | \
		xargs etags -l c++
	find . -name \*.h -or -name \*.c -or -name \*.cc | \
		xargs ctags 

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

CFGDIRS  := oasys/include oasys 
CFGFILES = Rules.make System.make oasys/share oasys/lib oasys/include/oasys
