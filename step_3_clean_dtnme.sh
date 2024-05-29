#/bin/bash

make clean
make cfgclean

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run make clean on the primary DTNME source\n"
exit 1
fi

cd third_party/oasys

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to change directory to the oasys_source directory\n"
exit 1
fi

make clean
make cfgclean

if [ $? -ne 0 ]
then
printf "Uh Oh, something went wrong\nAn error occured while trying to run make clean on the third_party OASYS source\n"
exit 1
fi
