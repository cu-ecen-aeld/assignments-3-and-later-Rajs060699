#!/bin/bash

filesdir=$1
searchstr=$2

if [ $# != 2 ]
then
echo "Number of paramters are incorrect"
exit 1
fi

if [ ! -d "$filesdir" ]
then
echo "file directory is not present"
exit 1
fi


file_directory_count=$(find $filesdir -type f | wc -l)
matching_lines_count=$(grep -r $searchstr  $filesdir | wc -l)
echo "The number of files are ${file_directory_count} and the number of matching lines are ${matching_lines_count}"
exit 0

