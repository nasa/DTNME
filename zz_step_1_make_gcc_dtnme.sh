#/bin/bash

MAKE=make
JOBS=1

re='^[0-9]+$'

if [ $# -gt 2 ]; then
    printf "\nUsage: $0 [make_executable_name [num_jobs]]\n"
    printf "\n\t[make_executable_name] allows for compatibility with systems\n\tthat use a different naming convention such as 'make' vs 'gmake' \n\n"
    exit 1
fi

if [ $# -gt 0 ]; then
    if ! [[ $1 =~ $re ]]; then
        MAKE=$1
    else
        if [ $1 -gt 0 ]; then
            JOBS=$1
        fi
    fi
fi
    
if [ $# -eq 2 ]; then
    if ! [[ $2 =~ $re ]]; then
        printf "\nUsage: $0 [make_executable_name [num_jobs]]\n"
        printf "\n\t[make_executable_name] allows for compatibility with systems\n\tthat use a different naming convention such as 'make' vs 'gmake' \n\n"
        exit 1
    else
        if [ $2 -gt 0 ]; then
            JOBS=$2
        fi
    fi
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
###./configure --disable-atomic-asm
./configure

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure oasys\n"
exit 1
fi

$MAKE

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
#### ./configure
./configure --prefix=/dzcore_home_dirs/bins/me13


if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure dtnme\n"
exit 1
fi

if [ $JOBS -gt 0 ]
then
  $MAKE -j $JOBS
else
  $MAKE
fi

printf $?
if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to make dtnme\n"
fi
