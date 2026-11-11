#!/bin/sh

cnt=0

command="ntpdate -b "$1
echo $command
until eval $command
do
   if test ${cnt} -lt 6
   then
      sleep 60
      cnt=`expr $cnt + 1`
   else
      sleep 1800
   fi
done
#echo "ntpdate synchronization succeeded!!"
