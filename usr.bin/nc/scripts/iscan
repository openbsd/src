#! /bin/sh
## duplicate DaveG's ident-scan thingie using netcat.  Oooh, he'll be pissed.
## args: target port [port port port ...]
## hose stdout *and* stderr together.
##
## advantages: runs slower than ident-scan, giving remote inetd less cause
## for alarm, and only hits the few known daemon ports you specify.
## disadvantages: requires numeric-only port args, the output sleazitude,
## and won't work for r-services when coming from high source ports.

case "${2}" in
  "" ) echo needs HOST and at least one PORT ; exit 1 ;;
esac

# ping 'em once and see if they *are* running identd
nc -z -w 9 "$1" 113 || { echo "oops, $1 isn't running identd" ; exit 0 ; }

# generate a randomish base port
RP=`expr $$ % 999 + 31337`

TRG="$1"
shift

while test "$1" ; do
  nc -v -w 8 -p ${RP} "$TRG" ${1} < /dev/null > /dev/null &
  PROC=$!
  sleep 3
  echo "${1},${RP}" | nc -w 4 -r "$TRG" 113 2>&1
  sleep 2
# does this look like a lamer script or what...
  kill -HUP $PROC
  RP=`expr ${RP} + 1`
  shift
done

