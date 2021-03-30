#!/bin/bash
#
#   Run this script from within the 3rdparty/wolfssl-#.#.#   subdirectory
#

pushd wolfssl
./configure --enable-tls13  --enable-opensslextra --enable-savecert --enable-sessioncerts --enable-static --enable-fastmath --enable-fasthugemath --enable-harden
make -j 9
popd
