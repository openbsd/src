#! /bin/sh
## web relay -- a degenerate version of webproxy, usable with browsers that
## don't understand proxies.  This just forwards connections to a given server.
## No query logging, no access control [although you can add it to XNC for
## your own run], and full-URL links will undoubtedly confuse the browser
## if it can't reach the server directly.  This was actually written before
## the full proxy was, and it shows.
## The arguments in this case are the destination server and optional port.
## Please flame pinheads who use self-referential absolute links.

# set these as you wish: proxy port...
PORT=8000
# any extra args to the listening "nc", for instance "-s inside-net-addr"
XNC=''

# functionality switch, which has to be done fast to start the next listener
case "${1}${RDEST}" in
  "")
    echo needs hostname
    exit 1
  ;;
esac

case "${1}" in
  "")
# no args: fire off new relayer process NOW.  Will hang around for 10 minutes
    nc -w 600 -l -n -p $PORT -e "$0" $XNC < /dev/null > /dev/null 2>&1 &
# and handle this request, which will simply fail if vars not set yet.
    exec nc -w 15 $RDEST $RPORT
  ;;
esac

# Fall here for setup; this can now be slower.
RDEST="$1"
RPORT="$2"
test "$RPORT" || RPORT=80
export RDEST RPORT

# Launch the first relayer same as above, but let its error msgs show up
# will hang around for a minute, and exit if no new connections arrive.
nc -v -w 600 -l -p $PORT -e "$0" $XNC < /dev/null > /dev/null &
echo \
  "Relay to ${RDEST}:${RPORT} running -- point your browser here on port $PORT"
exit 0
