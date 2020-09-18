#
# Makefile to set version bindings
#

EXTRACT_VERSION := $(OASYS_ETCDIR)/tools/extract-version

DTN_VERSION	  := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat version)
DTN_VERSION_MAJOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat major)
DTN_VERSION_MINOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat minor)
DTN_VERSION_PATCH := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat patch)
