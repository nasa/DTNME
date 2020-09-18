#!/bin/bash

if [ ! -z "$1" ] && [ -d "$1" ]
then
  make DESTDIR=$1 install
  if [ $? -ne 0 ]
  then
    printf "Uh Oh, something went wrong!\nAn error occured while trying to install dtnme\n"
    exit 1
  fi
else
  printf "Uh Oh, something went wrong!\nPlease specify a valid directory to install to\n"
  exit 1
fi
