#! /bin/sh
## Like "rcp" but uses netcat on a high port.
## do "ncp targetfile" on the RECEIVING machine
## then do "ncp sourcefile receivinghost" on the SENDING machine
## if invoked as "nzp" instead, compresses transit data.

## pick your own personal favorite port, which will be used on both ends.
## You should probably change this for your own uses.
MYPORT=23456

## if "nc" isn't systemwide or in your PATH, add the right place
# PATH=${HOME}:${PATH} ; export PATH

test "$3" && echo "too many args" && exit 1
test ! "$1" && echo "no args?" && exit 1
me=`echo $0 | sed 's+.*/++'`
test "$me" = "nzp" && echo '[compressed mode]'

# if second arg, it's a host to send an [extant] file to.
if test "$2" ; then
  test ! -f "$1" && echo "can't find $1" && exit 1
  if test "$me" = "nzp" ; then
    compress -c < "$1" | nc -v -w 2 $2 $MYPORT && exit 0
  else
    nc -v -w 2 $2 $MYPORT < "$1" && exit 0
  fi
  echo "transfer FAILED!"
  exit 1
fi

# fall here for receiver.  Ask before trashing existing files
if test -f "$1" ; then
  echo -n "Overwrite $1? "
  read aa
  test ! "$aa" = "y" && echo "[punted!]" && exit 1
fi
# 30 seconds oughta be pleeeeenty of time, but change if you want.
if test "$me" = "nzp" ; then
  nc -v -w 30 -p $MYPORT -l < /dev/null | uncompress -c > "$1" && exit 0
else
  nc -v -w 30 -p $MYPORT -l < /dev/null > "$1" && exit 0
fi
echo "transfer FAILED!"
# clean up, since even if the transfer failed, $1 is already trashed
rm -f "$1"
exit 1
