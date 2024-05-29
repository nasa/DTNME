#/bin/bash

MAKE=make

re='^[0-9]+$'

if ! [[ $1 =~ $re ]]; then
MAKE=$1
fi

cd third_party/oasys

#chmod +x tools/extract-version
#chmod +x tools/subst-version

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory to the oasys_source directory\n"
exit 1
fi

#autoheader must have this generated to work
touch oasys-version.h

sh build-configure.sh

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run build-configure.sh for oasys\n"
exit 1
fi

# The OASYS default configuration is usually sufficient for DTNME. 
./configure

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure oasys\n"
exit 1
fi

gmake

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to make oasys\n"
exit 1
fi

cd ../../

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory back to the dtnme base directory\n"
exit 1
fi

#autoheader must have this generated to work
touch dtn-version.h

sh build-configure.sh

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run 'build-configure.sh' for dtnme\n"
exit 1
fi

# The DTNME default configuration is usually sufficient for DTNME
#  add --without-dtpc if you do not want to enable DTPC
#  add --with-wolfssl if you want to use TLS secourity in TCP CLv4 (and install wolfssl in third_party directory)
./configure


if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure dtnme\n"
exit 1
fi

gmake -j 8

printf $?
if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to make dtnme\n"
fi
