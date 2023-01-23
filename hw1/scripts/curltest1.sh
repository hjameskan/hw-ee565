#!/bin/bash

for ((request=1;request<=3;request++))
do
    #dir="request_${request}"
    #mkdir $dir
	sleep 2
    time for ((x=1;x<=1000;x++))
    do
        curl -s localhost:8345/${1} > /dev/null &#/text/test.txt --output "./${dir}/${x}.txt" &
    done

done