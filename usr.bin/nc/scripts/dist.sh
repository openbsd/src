#! /bin/sh
## This is a quick example listen-exec server, which was used for a while to
## distribute netcat prereleases.  It illustrates use of netcat both as a
## "fake inetd" and a syslogger, and how easy it then is to crock up a fairly
## functional server that restarts its own listener and does full connection
## logging.  In a half-screen of shell script!!

PORT=31337

sleep 1
SRC=`tail -1 dist.log`
echo "<36>elite: ${SRC}" | ./nc -u -w 1 localhost 514 > /dev/null 2>&1
echo ";;; Hi, ${SRC}..."
echo ";;; This is a PRERELEASE version of 'netcat', tar/gzip/uuencoded."
echo ";;; Unless you are capturing this somehow, it won't do you much good."
echo ";;; Ready??  Here it comes!  Have phun ..."
sleep 8
cat dist.file
sleep 1
./nc -v -l -p ${PORT} -e dist.sh < /dev/null >> dist.log 2>&1 &
sleep 1
echo "<36>elite: done" | ./nc -u -w 1 localhost 514 > /dev/null 2>&1
exit 0
