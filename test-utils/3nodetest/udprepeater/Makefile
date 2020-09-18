
.SUFFIXES: .class .java

# NOTE: the JAVA_HOME environment variable MUST be set by the
# user BEFORE running the make.  
#
JAVA = ${JAVA_HOME}/bin/java
JAVAC = ${JAVA_HOME}/bin/javac
JAR = ${JAVA_HOME}/bin/jar

#
# platform specific parameters
#
##ifeq ($(shell uname -m),x86_64)
##JFLAGS = -Xlint:deprecation -Xlint:unchecked -d64
##else
##JFLAGS = -Xlint:deprecation -Xlint:unchecked -d32
##endif

# java jar command flags
JARFLAGS = -cvmf



PACKAGENAME = `basename ${PWD}`

#
# rules
#
.java.class:
	CLASSPATH=${CLASSPATH}:${PWD}/..; export CLASSPATH; \
	$(JAVAC) $(JFLAGS) $<

CLASSES = \
	SCPacket.class \
	SCPacketPool.class \
	UdpRptrAbstractSocket.class \
	UdpRptrApp.class \
	UdpRptrCoMainFrame.class \
	UdpRptrControllerBase.class \
	UdpRptrDisplayRefreshTimerTask.class \
	UdpRptrMulticastSocket.class \
	UdpRptrPacketCollector.class \
	UdpRptrResourceFile.class \
	UdpRptrStringTokenizer.class \
	UdpRptrThreadPools.class \
	UdpRptrUiConstants.class \
	UdpRptrUiMainFrame.class \
	UdpRptrUnicastSocket.class \
	UdpRptrViewInterface.class \
	UdpRptrVwMainFrame.class

.NO_CONFIG_REC: SCPacket.class
.NO_CONFIG_REC: SCPacketPool.class
.NO_CONFIG_REC: UdpRptrAbstractSocket.class
.NO_CONFIG_REC: UdpRptrApp.class
.NO_CONFIG_REC: UdpRptrCoMainFrame.class
.NO_CONFIG_REC: UdpRptrControllerBase.class
.NO_CONFIG_REC: UdpRptrDisplayRefreshTimerTask.class
.NO_CONFIG_REC: UdpRptrMulticastSocket.class
.NO_CONFIG_REC: UdpRptrPacketCollector.class
.NO_CONFIG_REC: UdpRptrResourceFile.class
.NO_CONFIG_REC: UdpRptrStringTokenizer.class
.NO_CONFIG_REC: UdpRptrThreadPools.class
.NO_CONFIG_REC: UdpRptrUiConstants.class
.NO_CONFIG_REC: UdpRptrUiMainFrame.form
.NO_CONFIG_REC: UdpRptrUiMainFrame.class
.NO_CONFIG_REC: UdpRptrUnicastSocket.class
.NO_CONFIG_REC: UdpRptrViewInterface.class
.NO_CONFIG_REC: UdpRptrVwMainFrame.class

.NO_CONFIG_REC: ${PACKAGENAME}.jar

TARGETS = ${PACKAGENAME}.jar

all::
	@for target in $(TARGETS); do \
		echo ; echo "building $${target}"; echo ; \
		CLASSPATH=${CLASSPATH}:${PWD}/..; export CLASSPATH; \
		make $${target}; \
	done
	@echo
	@echo "*** build is complete ***"
	@echo

udprepeater.jar::	Manifest $(CLASSES)
	cd ../; $(JAR) $(JARFLAGS) $(PACKAGENAME)/Manifest \
	$(PACKAGENAME)/$@ $(PACKAGENAME)/*.class 

#install::	all
#	$(INSTALL) ${PACKAGENAME}.jar $(INSTALL_BIN)

clean:: 
	-rm -f $(CLASSES) *.class
	-rm -f $(TARGETS)
	-rm -f *.java~
	-rm -rf *contrib*
	-rm -rf *~
	-rm -rf #*
	-rm -rf .nb*

