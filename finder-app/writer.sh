#!/bin/bash

if [ $# != 2 ]
then
echo "parameters are invalid only 2 are expected"
exit 1
fi

writefile=$1
writestr=$2


Directory=$(dirname "${writefile}")
if [ ! -d Directory ]
then
mkdir -p "${Directory}" && echo "${writestr}" > $writefile
exit 0
fi

if [ ! -f "$writefile" ]
then
    echo "File creation is failed"
    exit 1
fi

