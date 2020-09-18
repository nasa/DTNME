#
# System.make: settings extracted from the oasys configuration
#
# System.make.  Generated from System.make.in by configure.

#
# Programs
#
AR		= ar
RANLIB		= ranlib
DEPFLAGS	= -MMD -MP -MT "$*.o $*.E"
INSTALL 	= /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA 	= ${INSTALL} -m 644
PYTHON		= no
PYTHON_BUILD_EXT= 
XSD_TOOL	= xsd

#
# System-wide compilation flags including the oasys-dependent external
# libraries.
#
SYS_CFLAGS          = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
SYS_EXTLIB_CFLAGS   = 
SYS_EXTLIB_LDFLAGS  =  -ldl -lm  -ltcl8.5 -lexpat -lz  -ldb-5.3 -lpthread 

#
# Library-specific compilation flags
TCL_LDFLAGS     = -ltcl8.5

