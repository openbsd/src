# This file has been put together by Anno Siegel <siegel@zrz.TU-Berlin.DE>
# and Andreas Koenig <k@franz.ww.TU-Berlin.DE>. Comments, questions, and
# improvements welcome!

# This file was modified to work on NS 3.0 by Kevin White
# <klwhite@magnus.acs.ohio-state.edu>, based on suggestions by Andreas
# Koenig and Andy Dougherty.

echo With NS 3.0 you won\'t be able to use the POSIX module.
echo Be aware that some of the tests that are run during "make test"
echo will fail due to the lack of POSIX support on this system.
echo
echo Also, if you have the GDBM installed, make sure the header file
echo is located at a place on the system where the C compiler will
echo find it.  By default, it is placed in /usr/local/include/gdbm.h.
echo It will not be found there.  Try moving it to
echo /NextDeveloper/Headers/bsd/gdbm.h.

ccflags='-DUSE_NEXT_CTYPE -DNEXT30_NO_ATTRIBUTE'
POSIX_cflags='ccflags="-posix $ccflags"'
useposix='undef'
ldflags='-u libsys_s'
libswanted='dbm gdbm db'
#
lddlflags='-r'
# Give cccdlflags an empty value since Configure will detect we are
# using GNU cc and try to specify -fpic for cccdlflags.
cccdlflags=' '
#
i_utime='undef'
groupstype='int'
direntrytype='struct direct'
d_strcoll='undef'
# the simple program `for ($i=1;$i<38771;$i++){$t{$i}=123}' fails
# with Larry's malloc on NS 3.2 due to broken sbrk()
usemymalloc='n'
d_uname='define'
d_setpgid='define'
d_setsid='define'
d_tcgetpgrp='define'
d_tcsetpgrp='define'
#
# On some NeXT machines, the timestamp put by ranlib is not correct, and
# this may cause useless recompiles.  Fix that by adding a sleep before
# running ranlib.  The '5' is an empirical number that's "long enough."
# (Thanks to Andreas Koenig <k@franz.ww.tu-berlin.de>)
ranlib='sleep 5; /bin/ranlib' 

