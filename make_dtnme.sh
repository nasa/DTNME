#/bin/bash

cd oasys_source

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory to the oasys_source directory\n"
exit 1
fi

make clean

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run a 'make clean' on oasys\n"
exit 1
fi

sh build-configure.sh

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run build-configure.sh for oasys\n"
exit 1
fi

./configure --disable-debug-locking --with-odbc=no --with-python=no --with-extra-cflags="-O0 -ggdb3 -w" --with-extra-cxxflags="-std=c++11 -O0 -ggdb3 -w"

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure oasys\n"
exit 1
fi

make

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to make oasys\n"
exit 1
fi

cd ../

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory back to the dtnme base directory\n"
exit 1
fi

make clean

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run a 'make clean' on dtnme\n"
exit 1
fi

sh build-configure.sh ./oasys_source

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run 'build-configure.sh' for dtnme\n"
exit 1
fi

./configure --with-acs --with-ecos --with-oasys=./oasys_source --disable-ecl --disable-edp --enable-bid64bit --with-ltpudp --with-odbc=no --with-extra-cflags="-O0 -ggdb3 -w" --with-extra-cxxflags="-std=c++11 -O0 -ggdb3 -w -fpermissive"

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure dtnme\n"
exit 1
fi

if [$1 -gt 0]
then
  make -j$1
else
  make
fi
printf $?
if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to make dtnme\n"
exit 1
fi
