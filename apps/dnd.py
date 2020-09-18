#!/usr/bin/python

#
#    Copyright 2006 Intel Corporation
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



# DTN Neighbor Discovery (over UDP Broadcast) -- A small python script
# that will propagate DTN registration information via UDP broadcasts.
#
# Written by Keith Scott, The MITRE Corporation

# I got tired of having to manually configure dtn daemons, expecially
# since the combination of the windows operating system and MITRE's
# dhcp/dynamic DNS caused machine names/addresses to change
# when least convenient.

# This script will populate the static routing tables of DTN daemons
# with registrations (and optionally routes) it hears from its peers.
# When advertising my local
# registrations, I append "/*" to the local EID and prune anything
# that would match on this route.  This way if I'm running something
# like bundlePing that generates 'transient' registrations,
# I don't end up cluttering up everybody else's tables with 'em.

# This script assumes that all machines use TCP convergence layers to
# communicate

# This script transmits UDP broadcast messages to a particular port
# (5005 by default).  You'll need to open up firewalls to let this
# traffic through.

# The UDP Messages sent are of the form:
#
#	my DTN local EID
#	my TCP CL Listen Port
#	EID1 route1 distance1 nextHopEID1
#	EID2 route2 distance2 nextHopEID2
#	...

from socket import *
from time import *
import mutex
import os
import random
import string
import thread
import re
import getopt
import sys
import struct

INFINITY = 100
THE_MULTICAST_TTL = 5

# The default address and port
_DND_PORT = 5005
_DND_ADDR = '239.0.1.99'

#sendToAddresses = [_DND_ADDR]
#sendToPort = 5005				# Port to which reg info is sent
dtnTclConsolePort = 5050			# Port on which DTN tcl interpreter is listening
rebroadcastRoutes = 1
addLocalEIDWildcard = 1				# If 1, advertise a route of the form 
						# dtn://LOCAL_EID/* in addition to any
						# registrations.  Note that the wildcard route will
						# probably override other registrations.
myRIB = []					# list of entries
myEID = ""
myPort = ""
verbose = 0

# This mutex makes sure that the various threads for receiving / processing messages
# and sending out messages don't step on each other.
messageProcessingMutex = mutex.mutex()

myListeningDTNTCPPort = ""

# Here's a list of 'default' routes.  If we have no other way to get to a particular
# destination EID, make sure these are instantiated.  If there's any other way to get
# to a destination EID, make sure these are DE-instantiated so thate we don't have
# duplicate bundles flowing over both paths.
#
# Right now, the link name "default" is special and is the only one that will work
# for default routes.
#
defaultRoutes = [["dtn://26959-pc/*", "default"],
		 ["dtn://otherDest/*", "default"]]

#
# These are the indices of the various elements of a RIB entry.
# The RIB is really just an annotated copy of the forwarding table.
#
RIB_HOST = 0					# IP address of a the next hop to the RIB_EID
RIB_PORT = 1					# The TCPCL port of the next hop to the RIB_EID
RIB_EID =  2					# The EID in question (a destination EID)
RIB_DIST = 3					# Distance to the EID in question.
RIB_TIME = 4					# Time at which this entry was last updated
RIB_LINKNAME = 5
RIB_NHEID = 6					# EID of the next hop (used to implement split-horizon)

ROUTE_TIMEOUT = 25


broadcastInterval = 10 # How often to broadcast, in seconds

#
# Send a message to the dtn tcl interpreter and return results
# Return 'None' if we couldn't talk.
#
def talktcl(sent):
	received = 0
	# print "Opening connection to dtnd tcl interpreter."
	sock = socket(AF_INET, SOCK_STREAM)
	try:
			sock.connect(("localhost", dtnTclConsolePort))
	except:
		print "Connection failed"
		sock.close()
		return None

	try:
		messlen, received = sock.send(sent), 0
		if messlen != len(sent):
			print "Failed to send complete message to tcl interpreter"
		else:
			# print "Message '",sent,"' sent to tcl interpreter."
			messlen = messlen

		data = ''
		while 1:
			promptsSeen = 0
			data += sock.recv(32)
			#sys.stdout.write(data)
			received += len(data)
			# print "Now received:", data
			# print "checking for '%' in received data stream [",received,"], ", len(data)
			for i in range(len(data)):
				if data[i]=='%':
					promptsSeen = promptsSeen + 1
				if promptsSeen>1:
					break;
			if promptsSeen>1:
				break;

		# print "talktcl received: ",data," back from tcl.\n"

	except:
		sock.close()
		return None

	# Remove up to and including the first prompt
	firstPrompt=string.find(data, "dtn% ")
	if firstPrompt==-1:
		return ''
	data = data[firstPrompt+5:]

	sock.close()
	return(data);
		
#
# Return the port on which the TCP convergence layer is listening
#
def findListeningPort():
	response = talktcl("interface list\n")
	if response==None:
		return None

	lines = string.split(response, "\n")
	for i in range(len(lines)):
		if string.find(lines[i], "Convergence Layer: tcp")>=0:
			words = string.split(lines[i+1])
			return(words[3])
	return None

#
# Munge the list 'lines' to contain only entries
# that contain (in the re.seach sense) at least
# one of the keys
#
def onlyLinesContaining(lines, keys):
	answer = []
	for theLine in lines:
		for theKey in keys:
			if re.search(theKey, theLine):
					answer += [theLine]
					break;
	return answer

#
# Generate a random string containing letters and digits
# of specified length
#
def generateRandom(length):
	chars = string.ascii_letters + string.digits
	return(''.join([random.choice(chars) for i in range(length)]))

#
# Generate a new unique link identifier of the form dnd_XXXX
# where XXXX is a string of random letters.
#
def genNewLink(linkList):
	done = False
	print "genNewLink: ", linkList
	while done==False:
		test = generateRandom(4)
		# See if the identifier is already in use
		if len(linkList)>0:
			for i in range(len(linkList)):
				words = string.split(linkList[i], " ");
				if words[4]!=test:
					done = True
					break;
		else:
			done = True
	return "dnd_" + test


#
# Return a pair of lists: the current links and the current
# routes from the DTN daemon
#
def getLinksRoutes():
	myRoutes = talktcl("route dump\n")
	if myRoutes==None:
		print "getLinksRoutes: can't talk to dtn daemon"
		return([[],[]])
	#myRoutes = string.strip(myRoutes, "dtn% ")

	# Split the response into lines
	lines = string.split(myRoutes, '\n');
	
	theRoutes = []
	theLinks = []

	# After stripping off the header (first 5 lines), 
	# the routes are the first few lines up to the first blank line
	i = 0
	for i in range(5,len(lines)):
		# If the line has a "->" in it, it's a route.
		if string.find(lines[i], "->")>=0:
			theRoutes += [lines[i]]
		if string.find(lines[i], "Long")>=0:
			break
		if string.find(lines[i], "Links")>=0:
			break
		if len(lines[i])==1:
			break

	#
	# Fix up any Long Endpoint IDs
	#
	for i in range(5,len(lines)):
		if string.find(lines[i], "Long")>=0:
			break
	for j in range(i+1, len(lines)):
		if len(lines[j])==1:
			break;
		tokens = string.split(lines[j], ' ');
		tokens[0] = tokens[0].lstrip()
		tokens[0] = tokens[0].lstrip()
		tokens[0] = tokens[0].strip(":")

		for k in range(0, len(theRoutes)):
			if theRoutes[k].find(tokens[0])>=0:
				tokens[1] = tokens[1].strip("\r")
				theRoutes[k] = theRoutes[k].replace(tokens[0], tokens[1])
	
	#
	# Find the links
	# Start by whipping through the lines agin looking for "Links:"
	#
	for i in range(5,len(lines)):
		if string.find(lines[i], "Links:")>=0:
			break

	for j in range(i+1, len(lines)):
		if len(lines[j])==1:
			break;
		theLinks += [lines[j]]

	if ( verbose > 4 ):
		print "getLinksRoutes returns: "
		print theLinks
		print theRoutes
	return([theLinks, theRoutes])

# Return the link name of an existing link, or None
# format for newLink is hot:port
# format for 'newLink is host:port'
def alreadyHaveLink(newLink):
	theLinks, theRoutes = getLinksRoutes()
	for testLink in theLinks:
		bar = string.split(newLink, ":")
		host = bar[0]
		port = bar[1]

		# If we have a complete match (host:port), we're done
		if string.find(testLink, newLink)>=0:
			testLink = string.split(testLink)
			return testLink[0]

		# If we match on the host and the link is opportunistic,
		# go ahead and call it a match
		hostThere = string.find(testLink, host)
		isOpportunistic = testLink.startswith("opportunistic")
		if ( (hostThere>0) and (isOpportunistic)):
			foo = string.split(testLink, " ")
			return foo[0]
	return None

def myBroadcast():
	answer = []
	myaddrs = os.popen("/sbin/ip addr show").read()
	myaddrs = string.split(myaddrs, "\n")

	myaddrs = onlyLinesContaining(myaddrs, ["inet.*brd"])

	for addr in myaddrs:
		words = string.split(addr)
		for i in range(len(words)):
			if words[i]=="brd":
				answer += [words[i+1]]

	return answer

#
# Called periodically to time out routes that have not been refreshed.
#
def timeOutOldRoutes(RIB):
	# print "checking RIB for timed out routes..."
	# printRIB(myRIB)
	# The whole 'done' thing handles the fact that the list traversal gets gorked when
	# you yank elements out of the list
	done = 0
	while done == 0:
		done = 1
		for entry in RIB:
			if verbose>0:
				print "RIB entry", entry, "is ", time()-entry[RIB_TIME]," seconds old"
			if time()-entry[RIB_TIME]>ROUTE_TIMEOUT:
				print "RIB entry", entry, "timed out."
				print "Using 'removeRoute "+entry[RIB_EID]
				removeRoute(entry[RIB_EID])
				if entry[RIB_DIST]==INFINITY:
					RIB.remove(entry)
				entry[RIB_TIME] = time()
				done = 0

#
# remove a route, doing 'the right thing' by re-adding routes using the 'default'
# link for destinations listed as being able to use the default link
#
def removeRoute(eid):
	talktcl("route del "+eid+"\n")
	for (theEID, theLink) in defaultRoutes:
		if ( theEID==eid ):
			talktcl("route add "+eid+" "+theLink+"\n")
			return True
	return False
#
# Return True if I have an exact routing match (no wildcards) for the given
# EID
#
def exactRouteFor(eid):
	theLinks, theRoutes = getLinksRoutes()
	if len(theRoutes)>0:
		for i in range(len(theRoutes)):
			theRoutes[i] = theRoutes[i].strip()
			nextHop = string.split(theRoutes[i])[0];
			# print "Checking",eid," against existing route:", nextHop
			if nextHop==eid:
				return True
	return False

#
# Return True if I have a current route to the destination EID, False otherwise
# Route information is extracted from the deamon, NOT the RIB
#
def haveRouteFor(eid):
	theLinks, theRoutes = getLinksRoutes()
	if ( verbose>3 ):
		print "haveRouteFor about to check eid "+eid+" against existing routes [",len(theRoutes),"]"
	if len(theRoutes)>0:
		for i in range(len(theRoutes)):
			theRoutes[i] = theRoutes[i].strip()
			nextHop = string.split(theRoutes[i])[0];
			# print "Checking",eid," against existing route:", nextHop
			foo = re.search(nextHop, eid)
			if foo!=None:
				return True
	return False

def haveNonDefaultRouteFor(eid):
	theLinks, theRoutes = getLinksRoutes()
	if ( verbose>3 ):
		print "haveRouteFor about to check eid "+eid+" against existing routes [",len(theRoutes),"]"
	if len(theRoutes)>0:
		for i in range(len(theRoutes)):
			theRoutes[i] = theRoutes[i].strip()
			nextHop = string.split(theRoutes[i])[0];
			theLink = string.split(theRoutes[i])[4];
			# print "Checking",eid," against existing route:", nextHop
			foo = re.search(nextHop, eid)
			if ( (foo!=None) & (theLink!="default") ):
				return True
	return False

#
# Remove any existing route to the eid that uses a link
# named "default"
#
# Return True if we did in fact remove such a route, False if we didn't
def removeDefaultRouteFor(eid):
	theLinks, theRoutes = getLinksRoutes()
	if len(theRoutes)>0:
		for i in range(len(theRoutes)):
			theRoutes[i] = theRoutes[i].strip()
			nextHop = string.split(theRoutes[i])[0];
			theLink = string.split(theRoutes[i])[4];
			print "removeDefaultRouteFor:: checking",eid," against existing route:", nextHop + "->"+theLink
			foo = re.search(nextHop, eid)
			if ( (foo!=None) & (theLink=="default") ) :
				# Don't call removeRoute here, we don't ever
				# want to add the default route back in while we're removing
				# it.
				print "MATCH: removing default route to EID: "+eid
				talktcl("route del "+eid+"\n")
				return True
	return False

#
# Check our list of destinations that can use the 'default'
# link and add routes for any that do not have other routes
# already in the RIB
#
def addDefaultRouteFor(eid):
	if haveRouteFor(eid):
		return(False)
	for (theEID, theLink) in defaultRoutes:
		if ( eid==theEID):
			talktcl("route add "+eid+" "+theLink+"\n")

#
# Do I have a RIB entry for this EID?
#
def haveRIBEntryForEID(RIB, eid):
	for entry in RIB:
		foo = re.search(entry[RIB_EID], eid)
		if foo!=None:
			return True
	return False

#
# Return True if I have a local registration for the given EID, False otherwise
#
def haveRegistrationForEID(eid):
	# myEID = myLocalEID()

	if string.find(myEID+"/*", eid)>=0:
		return True

	myRegistrations = getRegistrationList()
	if myRegistrations is None:
		return False
	for myReg in myRegistrations:
		if string.find(myReg+"/*", eid)>=0:
			return True
	return False

#
# Try to add a route to eid via tcp CL host:port
#
# Adds the route and a supporting link.
#
# Remove any existing route that uses a link called "default"
# and replace with a link to host:port
#
# Don't add if we've already got a matching route for
# the eid
#
# Do refresh the update time for the route
#
# Return the name of the link added or None
#
def tryAddRoute(host, port, eid):
	theLinks, theRoutes = getLinksRoutes()

	# Remove any existing route to the eid that uses a link
	# named "default"
	removeDefaultRouteFor(eid)

	if haveRouteFor(eid):
		return None

	# print "About to check eid "+eid+" against my registrations."
	if haveRegistrationForEID(eid):
		return None

	# See if there's an existing link we can glom onto
	linkName = alreadyHaveLink(host+":"+port)
	if linkName==None:
		linkName = genNewLink(theLinks)
	else:
		print "Adding route to existing link:", linkName

	# link add linkName host:port ONDEMAND tcp
	print "link add ",linkName," ",host+":"+port," ONDEMAND tcp"
	talktcl("link add "+linkName+" "+host+":"+port+" ONDEMAND tcp\n")
	
	# route add EID linkName
	print "route add",eid," ",linkName
	talktcl("route add "+eid+" "+linkName+"\n")
	return linkName

#
# Server Thread
#
# This will set up a server listening on a particular interface (bindInterface)
# for messages to a particular address (listenAddress, possibly different from
# bindInterface to support multicast), and port
#
# On message receipt this calls processMessage, so that we can have multiple
# servers if need be.
#
def doServer(bindInterface, listenAddress, port):
	# Set the socket parameters
	buf = 1024
	# addr = (listenAddress,port)
	# list = []	# My persistent list of routes (RIB)

	if bindInterface is None:
		#intf = gethostbyname(gethostname())
		bindInterface = INADDR_ANY
	
	if listenAddress is None:
		listenAddress = _DND_ADDR

	if port is None:
		port = _DND_PORT
	
	print "doServer started on interface:", bindInterface, "address: ", listenAddress, "port: ", port

	# Create socket
	try:
		UDPSock = socket(AF_INET,SOCK_DGRAM)
	except:
		print "Can't create UDP socket."
		sys.exit(0)

	#
	# Figure out if listenAddress is multicast or not
	#
	if listenAddress.startswith("239."):
		#
		# ListenAddress IS Multicast
		#
		group = ('', port)

		try:
			UDPSock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
			UDPSock.setsockopt(SOL_SOCKET, SO_REUSEPORT, 1)
		except:
			print "Can't setsockopt SO_REUSEADDR / SO_REUSEPORT in server."
			pass
			# sys.exit(0)

		UDPSock.setsockopt(SOL_IP, IP_MULTICAST_TTL, THE_MULTICAST_TTL)
		UDPSock.setsockopt(SOL_IP, IP_MULTICAST_LOOP, 1)


		try:
			UDPSock.bind(group)
		except:
			# Some versions of linux raise an exception even though
			# SO_REUSE* options have been set, so ignore it
			print "Bind to: ", group, " failed."
			pass

		try:
			UDPSock.setsockopt(SOL_IP, IP_MULTICAST_IF, inet_aton(bindInterface)+inet_aton('0.0.0.0'))
		except:
			print "Can't set IP_MULTICAST_IF on:", bindInterface
			sys.exit(0)

		try:
			UDPSock.setsockopt(SOL_IP, IP_ADD_MEMBERSHIP, inet_aton(listenAddress) + inet_aton(bindInterface))
		except:
			print "Can't set IP_ADD_MEMBERSHIP for ", listenAddress
			sys.exit(0)
	else:
		#
		# ListenAddress is NOT multicast
		#
		#try:
		#	UDPSock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
		#except:
		#	print "Can't set UDP socket for broadcast."
		#	sys.exit(0)

		UDPSock.bind((listenAddress, listenPort))

	#
	# General Processing
	#
	#myEID = myLocalEID()

	# Receive messages
	while 1:
		try:
			data,addr = UDPSock.recvfrom(buf)
		except:
			"UDP recvfrom failed."

		print "Got a message from: ", addr

		if not data:
			print "Client has exited!"
			break
		else:
			processMessage(data, addr)

	# Close socket
	UDPSock.close()

def processMessage(data, addr):

	# Returns True if we got the lock, False if we didn't
	while messageProcessingMutex.testandset() == False:
		print "ProcessMessage is sleeping waiting on mutex..."
		sleep(1)

	#
	# This try is here to give us an out if something causes this thread
	# to core, so that we don't end up holding the mutex.
	# 
	try:
		SenderAddress = addr[0]
		things = string.split(data, '\n')
		SenderEID = things[0]
		SenderListenPort = things[1]

		# myEID = myLocalEID()

		# Am I the sender of this message?
		if things[0] == myEID:
			# print "I don't process my own messages (",SenderEID,",",gethostname(),")"
			messageProcessingMutex.unlock()
			return

		if (verbose>0):
			print "Received message."

		if (verbose>1):
			print data,"' from addr:", addr, "\n"

		if (verbose>3):
			print "Before message processing, RIB is:"
			printRIB(myRIB)

		# For each destination EID in the message, see if I've
		# already got a route to it or if my route is longer than
		# the one in the message.  If either of these hold, add a route
		# via the next hop of the message.
		# Also update RIB with entry info
		for i in range(2, len(things)-1):
			if (verbose>0):
				print "Processing message element:", things[i]
			[destEID, distance, NHEID] = string.split(things[i], " ");
			distance = string.atoi(distance)+1
			if (verbose>3):
				print "Received route entry for", destEID, "from", SenderEID," ",addr[0]," ",SenderListenPort

			#
			# Split-horizon means that I shouldn't be getting advertisements of routes
			# for which I am the next hop.  We do this filtering at the receiver so that
			# we can broadcase / multicast updates
			#
			if (NHEID==myEID):
				if (verbose > 2 ):
					print "Not processing route entry due to split horizon."
				continue

			#
			# Check my RIB to see what my current distance to this destination EID is
			# Also make sure that the daemon agrees that the route exists
			#
			myDist = currentDistanceTo(myRIB, destEID)
			daemonHasRoute = haveNonDefaultRouteFor(destEID)
			if (daemonHasRoute and (myDist!=None) and (myDist<=distance)):
				# My current route is better or equal
				if (verbose>3):
					print "current distance", myDist," is better or equal to received distance:", distance

				# If the EIDs match exactly, update the RIB entry, otherwise don't
				# This could happen, for example, if we have a RIB entry and route to dtn://xxxxx/* and this
				# entry is for dtn://xxxxx/ping
				if haveExactRIBEIDMatch(myRIB, destEID):
					refreshInternalRouteList(myRIB, SenderAddress, SenderListenPort, destEID, distance, SenderEID)
			else:
				# My current route entry has a higher metric or I have no current entry
				if (myDist<99999):
					removeRoute(destEID)
					#talktcl("route del "+destEID+"\n")
					removeRIBEntry(myRIB, destEID) # In case I had an entry.
				newLinkName = tryAddRoute(SenderAddress, SenderListenPort, destEID)
				refreshInternalRouteList(myRIB, SenderAddress, SenderListenPort, destEID, distance, SenderEID)

			# Make or update a RIB entry for this route.

			if (verbose>1):
				print "After refresh RIB is:"
				printRIB(myRIB)
	except:
		print "WARNING: something went wrong in processMessage..."
		messageProcessingMutex.unlock()

	messageProcessingMutex.unlock()


def haveExactRIBEIDMatch(RIB, eid):
	for elem in RIB:
		if elem[RIB_EID]==eid:
			return True
	return False

#
# Return the current best known distance to a destination EID
#
def currentDistanceTo(RIB, destEID):
	for elem in RIB:
		if haveRIBEntryForEID(RIB, destEID):
				return elem[RIB_DIST]
		print "I don't have a RIB entry for:", destEID
		return(99999)

#
# Remove an entry from the RIB table
#
def removeRIBEntry(RIB, destEID):
	for elem in RIB:
		if (elem[RIB_EID]==destEID):
			print "Removing elem: '", elem, "' from RIB"
			RIB.remove(elem)

#
# RIB element format described above.
#
# host: the IP address from which this entry was received
# port: the port on which the sending TCPCL is listening
# EID:  A destination EID
# distance: Distance from the sender of the update to the destination EID
# NHEID: the EID of the node that sent the update
#
#
def refreshInternalRouteList(RIB, host, port, EID, distance, NHEID):
	found = 0;
	if verbose>2:
			print "Processing entry:", host," ",port," ",EID," ",distance

	# If the NHEID is us, we need to NOT process this entry
	# (split-horizon)
	if (NHEID == myEID):
		return RIB

	for elem in RIB:
		if ( verbose>3):
			print "refreshInternalRouteList: checking", elem[RIB_EID]," against new entry", EID,"\n"
		#foo = re.search(EID, elem[RIB_EID])
		#if foo==None:
		#	continue
		if (elem[RIB_EID]==EID):
			elem[RIB_DIST] = distance
			elem[RIB_TIME] = time()
			found = 1

	# If we didn't update an existing RIB entry for this destination EID
	# make a new entry.
	if (found == 0):
		linkName = alreadyHaveLink(host+":"+port)
		print "Making new RIB entry"
		RIB += [[host, port, EID, distance, time(), linkName, NHEID]]

	return RIB

#
# printRIB
#
def printRIB(RIB):
	print "HOST      PORT      DESTEID    DIST    TIME    LINKNAME     NHEID"
	for elem in RIB:
		print elem[RIB_HOST]," ",elem[RIB_PORT]," ",elem[RIB_EID]," ",elem[RIB_DIST]," ",elem[RIB_TIME]," ",elem[RIB_LINKNAME]," ",elem[RIB_NHEID]

#
# Return a list of strings that are the current
# registrations
#
# Return None if we can't talk to the daemon
#
def getRegistrationList():
	response = talktcl("registration list\n")
	if response==None:
		return(None)
	#response = string.strip(response, "dtn% registration list")
	response = string.strip(response, "registration list")

	# Split the response into lines
	lines = string.split(response, '\n');

	# Throw away the first line
	lines = lines[1:]

	# Throw away things that are not registrations
	lines = onlyLinesContaining(lines, ["id "])
	answer = []
	for i in range(len(lines)):
			temp = string.split(lines[i], " ")
			answer += [temp[3]]
	return answer

#
# return my local EID
#
def myLocalEID():
	foo = talktcl("registration dump\n");
	if foo==None:
		return None

	foo = string.split(foo, "\n");
	foo = onlyLinesContaining(foo, "id 0:")
	bar = foo[0].find('%')
	foo = foo[0][bar+1:]
	foo = string.split(foo)
	return foo[3]

# Figure out if the given newItem (and EID) is covered by an existing one
# from the 'list'.  The format of list is very specific to the sending client
# (that is, list items are assumed to be (destEID, dist nheid)
def alreadyCovered(list, newItem):
	for listItem in list:
		bar = string.split(listItem, " ")
		foo = string.replace(bar[0], "?*", "\?*")
		if re.search(foo, newItem):
			if verbose>4:
				print newItem, " already covered by ", bar[0]
			return(True)
	return False

#
# destinations is a list of ['addr', port] pairs to send messages to
#
# multicastInterfaces is a list of interfaces on which multicast packets will be sent
#
def doClient(destinations, multicastInterfaces):
	print "doClient started with destinations:        ", destinations
	print "doClient started with multicastInterfaces: ", multicastInterfaces

	group = ('', _DND_PORT)

	# Create socket
	try:
		UDPSock = socket(AF_INET,SOCK_DGRAM)
	except:
		print "Can't create UDP socket."
		sys.exit(0)

	try:
		UDPSock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
		UDPSock.setsockopt(SOL_SOCKET, SO_REUSEPORT, 1)
	except:
		print "Can't set UDP socket for REUSEADDR / REUSEPORT in client."
		pass

	#
	# Need to set the socket to SO_BROADCAST in case one of the destinations
	# is a broadcast address.
	#
	try:
		UDPSock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
	except:
		print "Can't set UDP socket for BROADCAST."
		sys.exit(0)

	for interface in multicastInterfaces:
		#
		# Do these in case one of the destination addresses is multicast.
		# IP_MULTICAST_LOOP seems to mean that I get copies of things I
		# send.
		#
		try:
			UDPSock.setsockopt(SOL_IP, IP_MULTICAST_TTL, THE_MULTICAST_TTL)
			UDPSock.setsockopt(SOL_IP, IP_MULTICAST_LOOP, 1)
		except:
			print "Can't set UDP socket for IP_MULITCAST_TTL or IP_MULTICAST_LOOP."

		#
		# If the destination is multicast, I need to do this
		#
		try:
			UDPSock.setsockopt(SOL_IP, IP_MULTICAST_IF, inet_aton(interface) + inet_aton('0.0.0.0'))
		except:
			print "Can't setsockopt SOL_MULTICAST_IF on interface: ", interface
			pass

		print "Interface: ", interface, " set for MULTICAST."

	#
	# Send messages
	#
	while (1):
		# It's slightly simpler if we sleep at the top of the loop;
		# continue's work out easier
		sleep(broadcastInterval)

		# Returns True if I got the mutex
		while messageProcessingMutex.testandset()==False:
			print "Client is sleeping waiting on mutex..."
			sleep(1)

		thingsSent = []
		theList = getRegistrationList();
		if verbose > 2:
			print "getRegistrationList() returned:", theList
		if theList is None:
			# Probably couldn't talk to DTN daemon
			messageProcessingMutex.unlock()
			continue	

		if addLocalEIDWildcard==1:
			# Build a message that contains my IP address and port,
			# plus the list of registrations
			thingsSent += [myEID+"/* 0 "+myEID]

		#
		# Remove any duplication in building thingsSent list
		#
		isAlreadyThere = 0
		# For each registration
		for listEntry in theList:
			# Check against each entry already in the 'to be sent' list
			tempEntry = string.replace(listEntry, "?*", "\?*")

			if alreadyCovered(thingsSent, tempEntry):
				continue
			# OK, need to send this
			# Local registrations are at distance 0
			thingsSent += [listEntry+" 0 "+myEID]

		#
		#
		#
		if rebroadcastRoutes:
			for entry in myRIB:
				thingsSent += [entry[RIB_EID]+" "+str(entry[RIB_DIST])+" "+entry[RIB_NHEID]]

		#
		# Now build the text string to send
		#
		msg = myEID+'\n'
		msg += myListeningDTNTCPPort
		msg += '\n'
		for entry in thingsSent:
				msg += entry
				msg += "\n"
		if ( verbose>0 ):
			print "msg to send is:"
			print msg

		# Send to desired addresses
		for addr,port in destinations:
			print "Sending msg to: ", addr,":", port
			try:
				if(UDPSock.sendto(msg,(addr, port))):
					msg = msg
			except:
				print "Error sending message to:", addr
				print os.strerror("Error sending message to")

		timeOutOldRoutes(myRIB)

		#
		# Unlock the mutex
		#
		messageProcessingMutex.unlock()

	# Close socket
	UDPSock.close()

def installDefaultRoutes():
	for (eid,linkName) in defaultRoutes:
		if ( haveRouteFor(eid)==False ):
			talktcl("route add "+eid+" "+linkName+"\n")
	
def removeExistingRoutes():
	theLinks, theRoutes = getLinksRoutes()
	for route in theRoutes:
		tokens = string.split(route)
		talktcl("route del "+tokens[0]+"\n")
	return


def usage():
	print "dnd.py [-h] [-s] [-c] [-b PORT] [-t PORT] [-L seeBelow] [-d] [-r] [addr,[port]] [addr...]"
	print "  -h:       Print usage information (this)"
	print "  -s:       Only perform server (receiving) actions"
	print "  -c:       Only perform client (transmitting) actions"
	print "  -t #:     Set the DTN Tcl Console Port ("+str(dtnTclConsolePort)+")"
	print "  -L intf,addr:port  Add a listening socket on the given address and"
	print "            port.  If the address is multicast, then the interface "
	print "            needs to be given as well.  Possible syntaxes for the"
	print "            argument of '-L' are:"
	print "                  '-L port'     -- Use INADDR_ANY as the listen address"
	print "                  '-L intf,addr:port' -- Bind to the given interface,"
	print "				listening on a particular address/port --"
	print "		                This is useful for multicast."
	print "                  If you insist on binding to a particular interface"
	print "                  without using multicast, use the interface address"
	print "                  for both the intf and addr parts."
	print "  -d MDIST  Set the MULTICAST_TTL to MDIST (default ",THE_MULTICAST_TTL,")"
	print "  -m intf:  Add interface to the list of multicast SENDING"
	print "            interfaces."
	print "  -r:       Include route information in addition to (local) registration"
	print "            information.  This makes neighbor discovery into a"
	print "            really stupid routing algorithm, but possibly suitable"
	print "            for small lab setups (like several dtn routes in a"
	print "            linear topology)."
	print "  addrs are addresses to which UDP packets should be sent"
	print "            default:", myBroadcast()
	print "  ports are the destination ports for the addresses."
	print "            default:", _DND_PORT
	print "  -R    Remove existing routes and exit."
	print " "
	print " "
	print "Examples:"
	print "These examples assume that 10.9.1.1/24 is a local interface."
	print ""
	print "=================="
	print "Start dnd.py listening on port 5005 and sending to a broadcast address"
	print ""
	print "dnd.py -L 5005 10.9.1.255"
	print ""
	print "=================="
	print "Start dnd.py listening on port 5005 and sending to a remote subnet broadcast address"
	print "(10.10.4.255) and a remote unicast address (10.10.5.17)"
	print ""
	print "dnd.py -L 5005 10.10.4.255 10.10.5.17"
	print ""
	print "=================="
	print "Start dnd.py listening for multicast packets and transmitting to a multicast address"
	print ""
	print "./dnd.py -L 10.9.1.1:239.0.1.99:5005 -m 10.9.1.1 239.0.1.99:5005"
	print ""
	print "=================="
	print "Start dnd.py listening for multicast packets to group 239.0.1.99 on interface 10.9.1.1 and for"
	print "regular packets on port 5001.  Interface 10.9.3.2 is a multicast sendint interface, and we're "
	print "going to send to the multicast group 239.0.1.99 as well as to 10.9.2.255 port 5001"
	print ""
	print "dnd.py -L 10.9.1.1,239.0.1.99:5005 -L 5001 -m 10.9.3.2 239.0.1.99 10.9.2.255:5001"
	print ""


if __name__ == '__main__':
	print "argv is:", sys.argv, "[", len(sys.argv), "]"
	serverOn = True
	clientOn = True
	listenAddress = _DND_ADDR
	bindInterface = INADDR_ANY
	destinations = []  # list of [addr, port] pairs
	multicastSendInterfaces = []  # interfaces on which I may want to SEND MC packets
	listenThings = []   # where I listen for messages
	
	print "This is dnd.py version 1.0"

	#
	# Read DTND configuration information
	#
	myEID = myLocalEID()
	print "myEID is: ",myEID
	if myEID==None:
		print "Can't get local EID.  exiting"
		sys.exit(-1)

	myListeningDTNTCPPort = findListeningPort()
	if myListeningDTNTCPPort == None:
		print "Can't find listening port for TCP CL, client exiting."
		sys.exit(-1)

	sendToPort = _DND_PORT

	try:
		opts, args = getopt.getopt(sys.argv[1:], "L:m:l:b:rd:t:hI:scvR", ["help", "server", "client"])
	except getopt.GetoptError:
		usage()
		sys.exit(2)

	for o, a in opts:
		if o == "-h":
			usage();
			sys.exit(2)
		if o == "-v":
			verbose += 1
		if o == "-s":
			clientOn = False
		if o == "-c":
			serverOn = False
		if o == '-d':
			THE_MULTICAST_TTL = string.atoi(a)
		if o == "-L":
			# Possible syntaxes:
			# 	bindInterface,address:port   -- Multicast
			#	bindInterface,address        -- Multicast
			#	port
			print "-L is working on :", a
			foo = string.split(a, ':')
			if len(foo)==2:
				listenPort = foo[1]

			# foo[0] is empty,, intf, addr,,  or just a port

			bar = string.split(foo[0], ',')
			if len(bar)==1:
				# just a port
				listenPort = bar[0]
				bindInterface = '0.0.0.0'
				listenAddress = '0.0.0.0'
			else:
				# Assuming bindInterface,address syntax at this point
				bindInterface = bar[0]
				listenAddress = bar[1]
			
			listenThings += [[bindInterface, listenAddress, string.atoi(listenPort)]]
		if o == "-b":
			sendToPort = a
		if o == "-m":
			multicastSendInterfaces += [a]
		if o == "-r":
			rebroadcastRoutes = 1
		if o == "-t":
			dtnTclConsolePort = a
		if o == "-R":
			removeExistingRoutes()
			sys.exit(2)

	print "rest of args is now:", args

	#
	# Process destination addresses (where I send to)
	#
	for item in args:
		foo = string.split(item, ':')
		if len(foo)==1:
			# No port information given, use default
			theAddr = foo[0]
			thePort = str(sendToPort)
		else:
			theAddr = foo[0]
			thePort = foo[1]

		destinations += [[theAddr, string.atoi(thePort)]]
		print "Destinations now: ", destinations

	if len(destinations)==0:
		sendToAddress = myBroadcast()
		destinations = [[sendToAddress[0], sendToPort]]

	if len(listenThings)==0:
		listenThings = [['0.0.0.0', '0.0.0.0', sendToPort]]

	print " "
	print "================================"
	print " "
	print "Destinations now: ", destinations
	print " "
	print "ListenThings now: ", listenThings

	removeExistingRoutes()

	installDefaultRoutes()

	if clientOn:
		thread.start_new(doClient, (destinations, multicastSendInterfaces))
	if serverOn:
		for bindInterface, listenAddress, listenPort in listenThings:
			thread.start_new(doServer, (bindInterface, listenAddress, listenPort))

	# Now I just sort of hang out...
	while 1:
		sleep(10)

