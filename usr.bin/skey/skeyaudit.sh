#!/bin/sh
# $Id: skeyaudit.sh,v 1.1.1.1 1995/10/18 08:46:08 deraadt Exp $
# This script will look thru the skeykeys file for
# people with sequence numbers less then LOWLIMIT=12
# and send them an e-mail reminder to use skeyinit soon
# 

AWK=/usr/bin/awk
GREP=/usr/bin/grep
ECHO=/bin/echo
KEYDB=/etc/skeykeys
LOWLIMIT=12
ADMIN=root
SUBJECT="Reminder: Run skeyinit"
HOST=`/bin/hostname`


if [ "$1" != "" ]
then
 LOWLIMIT=$1
fi


# an skeykeys entry looks like
#   jsw 0076 la13079          ba20a75528de9d3a
# the sequence number is the second entry
#

for i in `$AWK '{print $1}' $KEYDB`
do
SEQ=`$GREP "^$i[ 	]" $KEYDB | $AWK '{print $2}'`
if [ $SEQ -lt $LOWLIMIT ]
then
  KEY=`$GREP "^$i[ 	]" $KEYDB | $AWK '{print $3}'`
  if [ $SEQ -lt  3 ]
  then
  SUBJECT="IMPORTANT action required"
  fi
  (
  $ECHO "You are nearing the end of your current S/Key sequence for account $i"
  $ECHO "on system $HOST."
  $ECHO ""
  $ECHO "Your S/key sequence number is now $SEQ.  When it reaches zero you"
  $ECHO "will no longer be able to use S/Key to login into the system.  "
  $ECHO " "
  $ECHO "Type \"skeyinit -s\" to reinitialize your sequence number."
  $ECHO ""
  ) | /usr/bin/Mail -s "$SUBJECT"  $i  $ADMIN
fi
done
