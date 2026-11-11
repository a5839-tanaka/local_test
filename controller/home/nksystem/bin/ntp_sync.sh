#!/bin/sh

cnt=0

command="/home/nksystem/tool/ntp_sync "$1" "$2
echo $command
until eval $command
do
   if test ${cnt} -lt 6
   then
echo "ntpdate synchronization Error!"
      sleep 60
      cnt=`expr $cnt + 1`
   else
      sleep 1800
   fi
done
echo "ntp_sync synchronization succeeded!!"
