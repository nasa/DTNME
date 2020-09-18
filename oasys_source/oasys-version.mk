#
# Makefile to set version bindings
#

EXTRACT_VERSION := $(SRCDIR)/tools/extract-version

OASYS_VERSION	    := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys-version.dat version)
OASYS_VERSION_MAJOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys-version.dat major)
OASYS_VERSION_MINOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys-version.dat minor)
OASYS_VERSION_PATCH := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys-version.dat patch)
