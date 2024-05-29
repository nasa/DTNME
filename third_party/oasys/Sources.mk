#
# Source lists
#

COMPAT_SRCS :=					    \
	compat/fpclassify.c			    \
	compat/getopt_long.c			\
	compat/inet_aton.c			    \

DEBUG_SRCS :=					    \
	debug/DebugUtils.cc			    \
	debug/DebugDumpBuf.cc			\
	debug/FatalSignals.cc			\
	debug/Formatter.cc			    \
	debug/Log.cc				    \
	debug/StackTrace.cc			    \
	debug/gdtoa-dmisc.c			    \
	debug/gdtoa-dtoa.c			    \
	debug/gdtoa-gdtoa.c			    \
	debug/gdtoa-gmisc.c			    \
	debug/gdtoa-misc.c			    \

IO_SRCS :=					        \
	io/BufferedIO.cc			    \
	io/FdIOClient.cc			    \
	io/FileIOClient.cc			    \
	io/FileUtils.cc				    \
	io/IO.cc				        \
	io/IPClient.cc				    \
	io/IPSocket.cc				    \
	io/NetUtils.cc				    \
	io/PrettyPrintBuffer.cc			\
	io/TCPClient.cc				    \
	io/TCPServer.cc				    \
	io/UDPClient.cc				    \

BLUEZ_SRCS :=					    \
	bluez/Bluetooth.cc			    \
	bluez/BluetoothSDP.cc			\
	bluez/BluetoothSocket.cc		\
	bluez/BluetoothInquiry.cc		\
	bluez/BluetoothClient.cc		\
	bluez/BluetoothServer.cc		\
	bluez/RFCOMMClient.cc			\

MEMORY_SRCS :=                      \
	memory/Memory.cc          

SERIALIZE_SRCS :=				    \
	serialize/BufferedSerializeAction.cc	\
	serialize/KeySerialize.cc		\
	serialize/MarshalSerialize.cc	\
	serialize/SQLSerialize.cc		\
	serialize/Serialize.cc			\
	serialize/StringSerialize.cc	\
	serialize/TclListSerialize.cc	\
	serialize/TextSerialize.cc		\
	serialize/XMLSerialize.cc       \
	serialize/XercesXMLSerialize.cc \

SMTP_SRCS :=                        \
	smtp/BasicSMTP.cc          		\
	smtp/SMTP.cc          			\
	smtp/SMTPClient.cc    			\
	smtp/SMTPServer.cc     			\
	smtp/SMTPUtils.cc     			\

STORAGE_SRCS :=						\
	storage/BerkeleyDBStore.cc		\
	storage/DurableStore.cc         \
	storage/DurableStoreImpl.cc		\
	storage/FileSystemStore.cc		\
	storage/MemoryStore.cc          \
    storage/ODBCMySQL.cc			\
    storage/ODBCSQLite.cc			\
    storage/ODBCStore.cc            \

TCLCMD_SRCS :=					    \
	tclcmd/ConsoleCommand.cc		\
	tclcmd/DebugCommand.cc			\
	tclcmd/HelpCommand.cc			\
	tclcmd/LogCommand.cc			\
	tclcmd/TclCommand.cc			\

THREAD_SRCS :=					    \
	thread/Mutex.cc				    \
	thread/NoLock.cc			    \
	thread/Notifier.cc			    \
	thread/SpinLock.cc			    \
	thread/Thread.cc			    \
	thread/Timer.cc				    \

UTIL_SRCS :=					    \
	util/CRC32.cc				    \
	util/ExpandableBuffer.cc		\
	util/Getopt.cc				    \
	util/HexDumpBuffer.cc			\
	util/InitSequencer.cc			\
	util/MD5.cc				        \
	util/OptParser.cc			    \
	util/Options.cc				    \
	util/ProgressPrinter.cc			\
	util/Random.cc				    \
	util/RateEstimator.cc			\
	util/RefCountedObject.cc		\
	util/Regex.cc				    \
	util/StreamBuffer.cc			\
	util/StringBuffer.cc			\
	util/StringUtils.cc			    \
	util/TextCode.cc			    \
	util/URI.cc				        \
	util/jenkins_hash.c			    \
	util/jenkins_hash.cc			\
	util/md5-rsa.c				    \

SRCS := \
	version.c				        \
	$(COMPAT_SRCS) 				    \
	$(DEBUG_SRCS) 				    \
	$(IO_SRCS) 				        \
	$(BLUEZ_SRCS) 				    \
	$(MEMORY_SRCS)                  \
	$(SERIALIZE_SRCS)			    \
	$(SMTP_SRCS)				    \
	$(STORAGE_SRCS) 			    \
	$(TCLCMD_SRCS)				    \
	$(THREAD_SRCS)				    \
	$(UTIL_SRCS)				    \
