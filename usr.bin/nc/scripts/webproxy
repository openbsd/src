#! /bin/sh
## Web proxy, following the grand tradition of Web things being handled by
## gross scripts.  Uses netcat to listen on a high port [default 8000],
## picks apart requests and sends them on to the right place.  Point this
## at the browser client machine you'll be coming from [to limit access to
## only it], and point the browser's concept of an HTTP proxy to the
## machine running this.  Takes a single argument of the client that will
## be using it, and rejects connections from elsewhere.  LOGS the queries
## to a configurable logfile, which can be an interesting read later on!
## If the argument is "reset", the listener and logfile are cleaned up.
##
## This works surprisingly fast and well, for a shell script, although may
## randomly fail when hammered by a browser that tries to open several
## connections at once.  Drop the "maximum connections" in your browser if
## this is a problem.
##
## A more degenerate case of this, or preferably a small C program that
## does the same thing under inetd, could handle a small site's worth of
## proxy queries.  Given the way browsers are evolving, proxies like this
## can play an important role in protecting your own privacy.
##
## If you grabbed this in ASCII mode, search down for "eew" and make sure
## the embedded-CR check is intact, or requests might hang.
##
## Doesn't handle POST forms.  Who cares, if you're just watching HTTV?
## Dumbness here has a highly desirable side effect: it only sends the first
## GET line, since that's all you really ever need to send, and suppresses
## the other somewhat revealing trash that most browsers insist on sending.

# set these as you wish: proxy port...
PORT=8000
# logfile spec: a real file or /dev/null if you don't care
LFILE=${0}.log
# optional: where to dump connect info, so you can see if anything went wrong
# CFILE=${0}.conn
# optional extra args to the listener "nc", for instance "-s inside-net-addr"
# XNC=''

# functionality switch has to be done fast, so the next listener can start
# prelaunch check: if no current client and no args, bail.
case "${1}${CLIENT}" in
  "")
    echo needs client hostname
    exit 1
  ;;
esac

case "${1}" in
  "")
# Make like inetd, and run the next relayer process NOW.  All the redirection
# is necessary so this shell has NO remaining channel open to the net.
# This will hang around for 10 minutes, and exit if no new connections arrive.
# Using -n for speed, avoiding any DNS/port lookups.
    nc -w 600 -n -l -p $PORT -e "$0" $XNC "$CLIENT" < /dev/null > /dev/null \
	2> $CFILE &
  ;;
esac

# no client yet and had an arg, this checking can be much slower now
umask 077

if test "$1" ; then
# if magic arg, just clean up and then hit our own port to cause server exit
  if test "$1" = "reset" ; then
    rm -f $LFILE
    test -f "$CFILE" && rm -f $CFILE
    nc -w 1 -n 127.0.0.1 $PORT < /dev/null > /dev/null 2>&1
    exit 0
  fi
# find our ass with both hands
  test ! -f "$0" && echo "Oops, cannot find my own corporeal being" && exit 1
# correct launch: set up client access control, passed along thru environment.
  CLIENT="$1"
  export CLIENT
  test "$CFILE" || CFILE=/dev/null
  export CFILE
  touch "$CFILE"
# tell us what happened during the last run, if possible
  if test -f "$CFILE"  ; then
    echo "Last connection results:"
    cat $CFILE
  fi

# ping client machine and get its bare IP address
  CLIENT=`nc -z -v -w 8 "$1" 22000 2>&1 | sed 's/.*\[\(..*\)\].*/\1/'`
  test ! "$CLIENT" && echo "Can't find address of $1" && exit 1

# if this was an initial launch, be informative about it
  echo "=== Launch: $CLIENT" >> $LFILE
  echo "Proxy running -- will accept connections on $PORT from $CLIENT"
  echo "  Logging queries to $LFILE"
  test -f "$CFILE" && echo "  and connection fuckups to $CFILE"

# and run the first listener, showing us output just for the first hit
  nc -v -w 600 -n -l -p $PORT -e "$0" $XNC "$CLIENT" &
  exit 0
fi

# Fall here to handle a page.
# GET type://host.name:80/file/path HTTP/1.0
# Additional: trash
# More: trash
# <newline>

read x1 x2 x3 x4
echo "=== query: $x1 $x2 $x3 $x4" >> $LFILE
test "$x4" && echo "extra junk after request: $x4" && exit 0
# nuke questionable characters and split up the request
hurl=`echo "$x2" | sed -e "s+.*//++" -e 's+[\`'\''|$;<>{}\\!*()"]++g'`
# echo massaged hurl: $hurl >> $LFILE
hh=`echo "$hurl" | sed -e "s+/.*++" -e "s+:.*++"`
hp=`echo "$hurl" | sed -e "s+.*:++" -e "s+/.*++"`
test "$hp" = "$hh" && hp=80
hf=`echo "$hurl" | sed -e "s+[^/]*++"`
# echo total split: $hh : $hp : $hf >> $LFILE
# suck in and log the entire request, because we're curious
# Fails on multipart stuff like forms; oh well...
if test "$x3" ; then
  while read xx ; do
    echo "${xx}" >> $LFILE
    test "${xx}" || break
# eew, buried returns, gross but necessary for DOS stupidity:
    test "${xx}" = "" && break
  done
fi
# check for non-GET *after* we log the query...
test "$x1" != "GET" && echo "sorry, this proxy only does GETs" && exit 0
# no, you can *not* phone home, you miserable piece of shit
test "`echo $hh | fgrep -i netscap`" && \
  echo "access to Netscam's servers <b>DENIED.</b>" && exit 0
# Do it.  30 sec net-wait time oughta be *plenty*...
# Some braindead servers have forgotten how to handle the simple-query syntax.
# If necessary, replace below with (echo "$x1 $hf" ; echo '') | nc...
echo "$x1 $hf" | nc -w 30 "$hh" "$hp" 2> /dev/null || \
  echo "oops, can't get to $hh : $hp".
echo "sent \"$x1 $hf\" to $hh : $hp" >> $LFILE
exit 0

