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
# Makefile for oasys
#

#
# Source lists
#

COMPAT_SRCS :=					\
	compat/getopt_long.c			\
	compat/inet_aton.c			\
	compat/rpc_compat.c			\
	compat/xdr_int64_compat.c		\
	compat/cfmakeraw.c			\
	compat/cfsetspeed.c			\

COMPAT_EXTLIB_SRCS :=				\
	compat/editline_compat.c		\

DEBUG_SRCS :=					\
	debug/DebugUtils.cc			\
	debug/DebugDumpBuf.cc			\
	debug/FatalSignals.cc			\
	debug/Formatter.cc			\
	debug/Log.cc				\
	debug/StackTrace.cc			\
	debug/vfprintf.c			\

IO_SRCS :=					\
	io/BufferedIO.cc			\
	io/FdIOClient.cc			\
	io/FileIOClient.cc			\
	io/FileUtils.cc				\
	io/IO.cc				\
	io/IPClient.cc				\
	io/IPSocket.cc				\
	io/MmapFile.cc				\
	io/NetUtils.cc				\
	io/PrettyPrintBuffer.cc			\
	io/RateLimitedSocket.cc			\
	io/TCPClient.cc				\
	io/TCPServer.cc				\
	io/TTY.cc				\
	io/UDPClient.cc				\

AX25_SRCS :=				\
	ax25/AX25Socket.cc			\
	ax25/AX25Client.cc			\
	ax25/AX25ConnectedModeClient.cc	\
	ax25/AX25ConnectedModeServer.cc	\

BLUEZ_SRCS :=					\
	bluez/Bluetooth.cc			\
	bluez/BluetoothSDP.cc			\
	bluez/BluetoothSocket.cc		\
	bluez/BluetoothInquiry.cc		\
	bluez/BluetoothClient.cc		\
	bluez/BluetoothServer.cc		\
	bluez/RFCOMMClient.cc			\

MEMORY_SRCS :=                                  \
	memory/Memory.cc          

SERIALIZE_SRCS :=				\
	serialize/BufferedSerializeAction.cc	\
	serialize/DebugSerialize.cc		\
	serialize/KeySerialize.cc		\
	serialize/MarshalSerialize.cc		\
	serialize/Serialize2Hash.cc		\
	serialize/SQLSerialize.cc		\
	serialize/Serialize.cc			\
	serialize/StreamSerialize.cc		\
	serialize/StringSerialize.cc		\
	serialize/TclListSerialize.cc		\
	serialize/TextSerialize.cc		\
	serialize/XMLSerialize.cc		\
	serialize/XercesXMLSerialize.cc		\
	serialize/StringPairSerialize.cc	\

SMTP_SRCS :=                                    \
	smtp/BasicSMTP.cc          		\
	smtp/SMTP.cc          			\
	smtp/SMTPClient.cc    			\
	smtp/SMTPServer.cc     			\
	smtp/SMTPUtils.cc     			\

STORAGE_SRCS :=					\
	storage/BerkeleyDBStore.cc		\
    storage/ODBCMySQL.cc            \
    storage/ODBCSQLite.cc           \
    storage/ODBCStore.cc                \
	storage/CheckedLog.cc			\
	storage/DurableStore.cc                 \
	storage/DurableStoreImpl.cc		\
	storage/StoreDetail.cc			\
	storage/FileBackedObject.cc		\
	storage/FileBackedObjectStore.cc	\
	storage/FileBackedObjectStream.cc	\
	storage/FileSystemStore.cc		\
	storage/MemoryStore.cc                  \
	storage/DS.cc				\
	storage/DataStore.cc			\
	storage/DataStoreProxy.cc		\
	storage/DataStoreServer.cc		\
	storage/ExternalDurableTableIterator.cc	\
	storage/ExternalDurableStore.cc		\
	storage/ExternalDurableTableImpl.cc	\

TCLCMD_SRCS :=					\
	tclcmd/ConsoleCommand.cc		\
	tclcmd/DebugCommand.cc			\
	tclcmd/GettimeofdayCommand.cc		\
	tclcmd/HelpCommand.cc			\
	tclcmd/IdleTclExit.cc			\
	tclcmd/LogCommand.cc			\
	tclcmd/TclCommand.cc			\
	tclcmd/tclreadline.c			\

THREAD_SRCS :=					\
	thread/Atomic-mutex.cc			\
	thread/LockDebugger.cc			\
	thread/Mutex.cc				\
	thread/NoLock.cc			\
	thread/Notifier.cc			\
	thread/OnOffNotifier.cc			\
	thread/SpinLock.cc			\
	thread/Thread.cc			\
	thread/Timer.cc				\

UTIL_SRCS :=					\
	util/App.cc				\
	util/Base16.cc				\
	util/CRC32.cc				\
	util/Daemonizer.cc			\
	util/ExpandableBuffer.cc		\
	util/Getopt.cc				\
	util/Glob.cc				\
	util/HexDumpBuffer.cc			\
	util/InitSequencer.cc			\
	util/MD5.cc				\
	util/OptParser.cc			\
	util/Options.cc				\
	util/ProgressPrinter.cc			\
	util/Random.cc				\
	util/RateEstimator.cc			\
	util/RefCountedObject.cc		\
	util/Regex.cc				\
	util/Singleton.cc			\
	util/StreamBuffer.cc			\
	util/StringAppender.cc			\
	util/StringBuffer.cc			\
	util/StringUtils.cc			\
	util/TextCode.cc			\
	util/Time.cc                            \
	util/TokenBucket.cc                     \
	util/TokenBucketLeaky.cc                \
    util/URI.cc				\
	util/jenkins_hash.c			\
	util/jenkins_hash.cc			\
	util/md5-rsa.c				\

XML_SRCS :=					\
	xml/ExpatXMLParser.cc			\
	xml/XMLDocument.cc			\
	xml/XMLObject.cc			\

SRCS := \
	oasys-version.c				\
	$(COMPAT_SRCS) 				\
	$(COMPAT_EXTLIB_SRCS)			\
	$(DEBUG_SRCS) 				\
	$(IO_SRCS) 				\
	$(AX25_SRCS)			\
	$(BLUEZ_SRCS) 				\
	$(MEMORY_SRCS)                          \
	$(SERIALIZE_SRCS)			\
	$(SMTP_SRCS)				\
	$(STORAGE_SRCS) 			\
	$(TCLCMD_SRCS)				\
	$(THREAD_SRCS)				\
	$(UTIL_SRCS)				\
	$(XML_SRCS)				\

OBJS := $(SRCS:.cc=.o)
OBJS := $(OBJS:.c=.o)

COMPAT_OBJS := $(COMPAT_SRCS:.c=.o) oasys-version.o

ALLSRCS := $(SRCS)

CPPS := $(SRCS:.cc=.E)
CPPS := $(CPPS:.c=.E)

TOOLS	:= \
	tools/md5chunks				\
	tools/oasys_tclsh			\
	tools/proc-watcher			\
	tools/randfile				\
	tools/zsize				\


all: checkconfigure libs $(TOOLS)

#
# If srcdir/builddir aren't set by some other makefile, 
# then default to .
#
ifeq ($(SRCDIR),)
SRCDIR	 := .
BUILDDIR := .
endif

#
# Special override rules for objects that can't use the default build options
#
compat/xdr_int64_compat.o: compat/xdr_int64_compat.c
	$(CC) $(CPPFLAGS) $(CFLAGS_NOWARN) -c $< -o $@

debug/vfprintf.o: debug/vfprintf.c
	$(CC) $(CPPFLAGS) $(CFLAGS_NOWARN) -c $< -o $@

tclcmd/tclreadline.o: tclcmd/tclreadline.c
	$(CC) $(CPPFLAGS) $(CFLAGS_NOWARN) -c $< -o $@

#
# Include the Makefile for tests
#
include $(SRCDIR)/test/Makefile

#
# Define other clean rules
CLEAN_OTHER = toolsclean

#
# Include the common rules
#
include Rules.make

#
# Based on configuration options, select the libraries to build
#
LIBFILES := lib/liboasys.a lib/liboasyscompat.a
LIBFILES += lib/liboasys-$(OASYS_VERSION).a
LIBFILES += lib/liboasyscompat-$(OASYS_VERSION).a

ifeq ($(SHLIBS),yes)
LIBFILES += lib/liboasys-$(OASYS_VERSION).$(SHLIB_EXT) 
LIBFILES += lib/liboasyscompat-$(OASYS_VERSION).$(SHLIB_EXT)
LIBFILES += lib/liboasys.$(SHLIB_EXT) lib/liboasyscompat.$(SHLIB_EXT)
endif

.PHONY: libs
libs: $(LIBFILES)

#
# And a special rule to build the command-init-tcl.c file from command.tcl
#
tclcmd/TclCommand.o: tclcmd/command-init-tcl.c
tclcmd/command-init-tcl.c: $(SRCDIR)/tclcmd/command-init.tcl
	rm -f $@
	echo "static const char* INIT_COMMAND = " > $@;
	cat $^ | sed 's|\\|\\\\|g' |\
		 sed 's|"|\\"|g' | \
		 sed 's|^|"|g' | \
		 sed "s|$$|\\\\n\"|g" >> $@;
	echo ";">> $@

#
# Rules for the version files
#
oasys-version.o: oasys-version.c
oasys-version.c: oasys-version.h
oasys-version.h: oasys-version.h.in oasys-version.dat
	tools/subst-version oasys-version.dat <  \
	   $(SRCDIR)/oasys-version.h.in > oasys-version.h

vpath oasys-version.h.in $(SRCDIR)
vpath oasys-version.h    $(SRCDIR)
vpath oasys-version.c    $(SRCDIR)
vpath oasys-version.dat  $(SRCDIR)

bump-version:
	cd $(SRCDIR) && tools/bump-version oasys-version.dat

#
# Rule to generate the doxygen documentation
#
doxygen:
	doxygen doc/doxyfile

#
# And a rule to make sure that configure has been run recently enough.
#
.PHONY: checkconfigure
checkconfigure: Rules.make

Rules.make: $(SRCDIR)/Rules.make.in $(SRCDIR)/configure
	@[ ! x`echo "$(MAKECMDGOALS)" | grep clean` = x ] || \
	(echo "$@ is out of date, need to rerun configure" && \
	exit 1)

#
# Make sure the clean rule knows about which files need cleaning
#
CFGFILES := Rules.make oasys-config.h config.cache config.log config.status \
CFGDIRS  := 
$(SRCDIR)/Rules.make.in:
	@echo SRCDIR: $(SRCDIR)
	@echo error -- Makefile did not set SRCDIR properly
	@exit 1

lib/liboasys-$(OASYS_VERSION).a: $(OBJS)
	@rm -f $@; mkdir -p $(@D)
	$(AR) ruc $@ $^
	$(RANLIB) $@ || true

lib/liboasys-$(OASYS_VERSION).$(SHLIB_EXT): $(OBJS)
	@rm -f $@; mkdir -p $(@D)
	$(CXX) $^ $(LDFLAGS_SHLIB) $(LDFLAGS) $(EXTLIB_LDFLAGS) -o $@

lib/liboasyscompat-$(OASYS_VERSION).a: $(COMPAT_OBJS)
	@rm -f $@; mkdir -p $(@D)
	$(AR) ruc $@ $^
	$(RANLIB) $@ || true

lib/liboasyscompat-$(OASYS_VERSION).$(SHLIB_EXT): $(COMPAT_OBJS)
	@rm -f $@; mkdir -p $(@D)
	$(CXX) $^ $(LDFLAGS_SHLIB) $(LDFLAGS) -o $@

# Rules for symlinks
lib/%.a: lib/%-$(OASYS_VERSION).a
	rm -f $@
	cd `dirname $@` && ln -s `basename $<` `basename $@`

lib/%.$(SHLIB_EXT): lib/%-$(OASYS_VERSION).$(SHLIB_EXT)
	rm -f $@
	cd `dirname $@` && ln -s `basename $<` `basename $@`

# Rules for linking tools
tools/md5chunks: tools/md5chunks.o $(LIBFILES)
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS)

tools/oasys_tclsh: tools/oasys_tclsh.o $(LIBFILES)
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS)

tools/proc-watcher: tools/proc-watcher.o
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS)

tools/randfile: tools/randfile.o
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS) $(OASYS_LDFLAGS)  $(EXTLIB_LDFLAGS)

tools/zsize: tools/zsize.o $(LIBFILES)
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS) $(OASYS_LDFLAGS) $(EXTLIB_LDFLAGS)

.PHONY: toolsclean
toolsclean:
	for prog in $(TOOLS) ; do \
	    (rm -f $$prog) ; \
	done


#
# Installation rules
#
.PHONY: install
install: installdirs installbin installinclude installlib installdata

INCDIRS := compat debug io bluez debug memory serialize \
	   smtp storage tclcmd thread util xml 

installdirs:
	for dir in $(bindir) $(libdir) \
		   $(datadir)/oasys/aclocal $(datadir)/oasys/tools ; do \
	[ -d $(DESTDIR)$$dir ] || \
	    (mkdir -p $(DESTDIR)$$dir; chmod 755 $(DESTDIR)$$dir) \
        done

	for subdir in $(INCDIRS) ; do \
        dir=$(includedir)/oasys/$$subdir ; \
	[ -d $(DESTDIR)$$dir ] || \
	    (mkdir -p $(DESTDIR)$$dir; chmod 755 $(DESTDIR)$$dir) \
        done

installbin: installdirs
	for prog in tools/oasys_tclsh ; do \
	    ($(INSTALL_PROGRAM) $$prog $(DESTDIR)$(bindir)) ; \
	done

installinclude: installdirs
	for f in $(SRCDIR)/*.h ; do \
	    ($(INSTALL_PROGRAM) -c $$f $(DESTDIR)$(includedir)/oasys) ; \
	done

	for f in $(BUILDDIR)/*.h ; do \
	    ($(INSTALL_PROGRAM) -c $$f $(DESTDIR)$(includedir)/oasys) ; \
	done

	for dir in $(INCDIRS) ; do \
        for f in $(SRCDIR)/$$dir/*.h ; do \
	    $(INSTALL_PROGRAM) -c $$f $(DESTDIR)$(includedir)/oasys/$$dir ; \
	done \
	done

	for dir in $(INCDIRS) ; do \
        if test ! -z "`ls $(SRCDIR)/$$dir/*.tcc 2>/dev/null`" ; then \
	for f in $(SRCDIR)/$$dir/*.tcc ; do \
	    $(INSTALL_PROGRAM) -c $$f $(DESTDIR)$(includedir)/oasys/$$dir ; \
	done \
	fi \
	done

	cp -rp $(SRCDIR)/ext $(DESTDIR)$(includedir)/oasys

installlib: installdirs
	[ $(DESTDIR) = . ] || \
	[ x$(SHLIBS) = x ] || \
	for lib in lib/liboasys-$(OASYS_VERSION).$(SHLIB_EXT) \
	           lib/liboasyscompat-$(OASYS_VERSION).$(SHLIB_EXT) ; do \
	    ($(INSTALL_PROGRAM) $$lib $(DESTDIR)$(libdir)) ; \
	done

	[ $(DESTDIR) = . ] || \
	for lib in liboasys liboasyscompat ; do \
		(cd $(DESTDIR)$(libdir) && rm -f $$lib.$(SHLIB_EXT) && \
		 ln -s $$lib-$(OASYS_VERSION).$(SHLIB_EXT) $$lib.$(SHLIB_EXT)) \
	done

installdata: installdirs
	for f in $(SRCDIR)/Rules.make.in \
		 System.make \
		 $(SRCDIR)/oasys-version.dat ; do \
	    ($(INSTALL_PROGRAM) $$f $(DESTDIR)$(datadir)/oasys) ; \
        done

	for f in $(SRCDIR)/aclocal/* ; do \
	    ($(INSTALL_PROGRAM) $$f $(DESTDIR)$(datadir)/oasys/aclocal) ; \
	done

	for f in $(SRCDIR)/tools/subst-version \
	         $(SRCDIR)/tools/extract-version \
	         $(SRCDIR)/tools/bump-version \
	         $(SRCDIR)/tools/print-leaked-refs \
	         ; do \
	    ($(INSTALL_PROGRAM) $$f $(DESTDIR)$(datadir)/oasys/tools) ; \
	done

.PHONY: cpps
cpps: $(CPPS)

# build tests
.PHONY: test tests testfiles
TESTS := $(patsubst %,test/%,$(TESTS))
TESTFILES := $(patsubst %,test/%,$(TESTFILES))
test tests: all $(TESTS) $(TESTFILES)


# tags
.PHONY: tag tags
tag tags:
	find . -name "*.cc" -o -name "*.h" | xargs ctags
	find . -name "*.cc" -o -name "*.h" | xargs etags


# the --cxx-prologue and --cxx-epilogue add a guard to the generated c++ code
XSD_TOOL_ARGS := \
	--generate-serialization \
	--hxx-suffix .h \
	--cxx-suffix .cc \
	--root-element-all \
	--namespace-map =dsmessage \
	--cxx-prologue '\#include <config.h>' \
	--cxx-prologue '\#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DS_ENABLED)' \
	--cxx-epilogue '\#endif' \

xsdbindings: storage/DS.xsd
ifdef XSD_TOOL
	$(XSD_TOOL) cxx-tree $(XSD_TOOL_ARGS) --output-dir `dirname $<` $<
else
	@echo "WARNING: configure was unable to find the xsd tool needed to"
	@echo "         regenerate DS.h."
	@echo "         Use the --with-xsd-tool=(name) option with configure"
	@echo "         specify the location of this tool."
endif
