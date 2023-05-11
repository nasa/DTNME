#/bin/bash

cd third_party/oasys

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory to the oasys_source directory\n"
exit 1
fi

sh build-configure.sh

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run build-configure.sh for oasys\n"
exit 1
fi

CLANG_PATH=$(whereis clang)

# The OASYS default configuration is usually sufficient for DTNME. 
./configure --disable-atomic-asm \
--with-cc=/usr/bin/clang --with-cxx=/usr/bin/clang++ \
--with-extra-ldflags="-L/usr/lib" \
--with-extra-cflags="-fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope -fno-optimize-sibling-calls -std=c11" \
--with-extra-cxxflags="-fsanitize=address -fno-omit-frame-pointer -fno-common -std=c++14 -g -fstandalone-debug -fsanitize-address-use-after-scope -fno-optimize-sibling-calls -Wno-undefined-var-template"

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

cd ../../

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory back to the dtnme base directory\n"
exit 1
fi

sh build-configure.sh

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run 'build-configure.sh' for dtnme\n"
exit 1
fi

# The DTNME default configuration is usually sufficient for DTNME
#  add --without-dtpc if you do not want to enable DTPC
#  add --with-wolfssl if you want to use TLS secourity in TCP CLv4 (and install wolfssl in third_party directory)
./configure \
--with-cc=/usr/bin/clang --with-cxx=/usr/bin/clang++ \
--with-extra-ldflags="-L/usr/lib" \
--with-extra-cflags="-fsanitize=address -fno-omit-frame-pointer -fno-common -fno-optimize-sibling-calls -std=c11" \
--with-extra-cxxflags="-fsanitize=address -fno-omit-frame-pointer -fno-common -std=c++14 -g -fsanitize-address-use-after-scope -fno-optimize-sibling-calls -fstandalone-debug -Wno-undefined-var-template"


if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to configure dtnme\n"
exit 1
fi

if [ $1 -gt 0 ]
then
  make -j $1
else
  make
fi

printf $?
if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to make dtnme\n"
fi
