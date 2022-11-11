#!/bin/bash

DTNDIR=/var/dtn
DTNDBDIR=$DTNDIR/db
INITDTNDB=false
# Parse command line arguments
if [ -z "$1" ] ; then
  CONFFILE=/conf/dtnme.conf
else
  CONFFILE=$1
fi
if [ ! -f "$CONFFILE" ] ; then
  echo "Configuration file $CONFFILE can't be found"
  exit 1
fi
# Check if we need to create DTN-related directories
if [ ! -d "$DTNDIR" ] || [ ! -d "$DTNDBDIR" ]; then
  echo "Creating DTN directories"
  mkdir "$DTNDIR"
  INITDTNDB=true
fi

# Start DTNME
# Initialize database, if required.
if $INITDTNDB; then
  echo "Initializing database"
  dtnd -c $CONFFILE --init-db &
  DTNMEPID=$!
  echo "Waiting 5 seconds for database to settle"
  sleep 5
  kill $DTNMEPID
fi
echo "Starting DTNME"
#Launch DTNME
dtnd -c $CONFFILE


