#! /bin/sh
## Shit-simple script to supply the "privmsg <recipient>" of IRC typein, and
## keep the connection alive.  Pipe this thru "nc -v -w 5 irc-server port".
## Note that this mechanism makes the script easy to debug without being live,
## since it just echoes everything bound for the server.
## if you want autologin-type stuff, construct some appropriate files and
## shovel them in using the "<" mechanism.

# magic arg: if "tick", do keepalive process instead of main loop
if test "$1" = "tick" ; then
# ignore most signals; the parent will nuke the kid
# doesn't stop ^Z, of course.
  trap '' 1 2 3 13 14 15 16
  while true ; do
    sleep 60
    echo "PONG !"
  done
fi

# top level: fire ourselves off as the keepalive process, and keep track of it
sh $0 tick &
ircpp=$!
echo "[Keepalive: $ircpp]" >&2
# catch our own batch of signals: hup int quit pipe alrm term urg
trap 'kill -9 $ircpp ; exit 0' 1 2 3 13 14 15 16
sleep 2

sender=''
savecmd=''

# the big honkin' loop...
while read xx yy ; do
  case "${xx}" in
# blank line: do nothing
    "")
	continue
    ;;
# new channel or recipient; if bare ">", we're back to raw literal mode.
    ">")
	if test "${yy}" ; then
	  sender="privmsg ${yy} :"
	else
	  sender=''
	fi
	continue
    ;;
# send crud from a file, one line per second.  Can you say "skr1pt kidz"??
# *Note: uses current "recipient" if set.
    "<")
	if test -f "${yy}" ; then
	  ( while read zz ; do
	    sleep 1
	    echo "${sender}${zz}"
	  done ) < "$yy"
	  echo "[done]" >&2
	else
	  echo "[File $yy not found]" >&2
	fi
	continue
    ;;
# do and save a single command, for quick repeat
    "/")
	if test "${yy}" ; then
	  savecmd="${yy}"
	fi
	echo "${savecmd}"
    ;;
# default case goes to recipient, just like always
    *)
	echo "${sender}${xx} ${yy}"
	continue
    ;;
  esac
done

# parting shot, if you want it
echo "quit :Bye all!"
kill -9 $ircpp
exit 0
