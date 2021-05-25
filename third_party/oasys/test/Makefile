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

TESTS += \
	atomic-test				\
	base16-test				\
	berkeley-db-test			\
	buffer-test				\
	cache-test				\
	checked-log-test			\
	durable-cache-test			\
	file-obj-store-test			\
	filesys-db-test				\
	functor-test				\
	io-basic-test				\
	iterator-test				\
	log-test				\
	log-profile-test			\
	marshal-test				\
	memory-store-test			\
	msg-queue-test				\
	open-fd-cache-test                      \
	options-test				\
	optparser-test				\
	regex-test				\
	sample-test				\
	serialize-stream-test			\
	serialize-test				\
	smtp-test				\
	sparse-array-test			\
	sparse-bitmap-test			\
	spin-lock-test				\
	stack-trace-test			\
	static-buffer-test			\
	stream-serialize-test			\
	string-appender-test			\
	string-hash-test			\
	string-tokenize-test			\
	text-code-test				\
	timer-test				\
	token-bucket-test			\
	type-collection-test		        \
	updatable-priority-queue-test		\
	uri-test				\
	util-test				\
	xml-test				\
	
#
# Tests that need to be explicitly enable (add them above yourself)
#
## ax25-test - you should edit the source first and set callsigns appropriately!
DISABLED_TESTS += \
	ax25-test				\

#
# Tests that need to be fixed
#
BROKEN_TESTS +=  \
	alist-test				\
	ptr-cache-test				\

#
# Tests that need to be converted to the new framework
#
OLD_TESTS :=					\
	buffered-io-test			\
	hexdump-test				\
	md5-test				\
	memory-test				\
	mutex-test				\
	tcp-socket-test				\
	tcp-timeout-read-test			\
	tcp-writeall-test			\
	thread-test				\

TEST_SRCS := $(patsubst %,test/%.cc,$(TESTS))
ALLSRCS += $(TEST_SRCS)

#
# Make sure test executables are included in 'make clean'
#
BINFILES += $(TESTS)

#
# default rule for tests
#
test/%test: test/%test.cc lib/liboasys.a
	@mkdir -p test
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(OASYS_LDFLAGS_STATIC) $(EXTLIB_LDFLAGS)

test/%Test: test/%Test.cc lib/liboasys.a
	@mkdir -p test
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(OASYS_LDFLAGS_STATIC) $(EXTLIB_LDFLAGS)

#
# Override rules for stack-trace-test to disable the cast-align warnings
#
test/stack-trace-test: test/stack-trace-test.cc lib/liboasys.a
	@mkdir -p test
	$(CXX) $(CXXFLAGS) $< -o $@ -Wno-cast-align $(LDFLAGS) $(OASYS_LDFLAGS_STATIC) $(EXTLIB_LDFLAGS)

#
# special rules for the smtp support scripts used in cases where
# the build directory is not the source directory
#
TESTFILES += \
	smtp-test-send.py \
	smtp-test-send.tcl

test/smtp-test-send.py test/smtp-test-send.tcl:
	rm -f $@
	ln -s $(SRCDIR)/$@ $@
