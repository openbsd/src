# BeOS hints file
# $Id: beos.sh,v 1.1 1998/02/16 03:51:45 dogcow Exp $

if [ ! -f beos/nm ]; then mwcc -w all -o beos/nm beos/nm.c 2>/dev/null; fi
# If this fails, that's all right - it's only for PPC.

prefix="/boot/home/config"

#cpp="mwcc -e"

libpth='/boot/beos/system/lib /boot/home/config/lib'
usrinc='/boot/develop/headers/posix'
locinc='/boot/develop/headers/ /boot/home/config/include'

libc='/boot/beos/system/lib/libroot.so'
libs=' '

d_bcmp='define'
d_bcopy='define'
d_bzero='define'
d_index='define'
#d_htonl='define' # It exists, but much hackery would be required to support.
# a bunch of extra includes would have to be added, and it's only used at
# one place in the non-socket perl code.

#these are all in libdll.a, which my version of nm doesn't know how to parse.
#if I can get it to both do that, and scan multiple library files, perhaps
#these can be gotten rid of.

usemymalloc='n'
# Hopefully, Be's malloc knows better than perl's.

d_link='undef'
dont_use_nlink='define'
# no posix (aka hard) links for us!

d_syserrlst='undef'
# the array syserrlst[] is useless for the most part.
# large negative numbers really kind of suck in arrays.

# Sockets didn't use to be real sockets but BONE changes this.
# How does one test for BONEness?
if [ ! -f /some/bone/file.h ]; then
    d_socket='undef'
    d_gethbyaddr='undef'
    d_gethbyname='undef'
    d_getsbyname='undef'
fi

ld='gcc'

export PATH="$PATH:$PWD/beos"

case "$ldlibpthname" in
'') ldlibpthname=LIBRARY_PATH ;;
esac

# the waitpid() wrapper
archobjs="beos.o"
test -f beos.c || cp beos/beos.c .


