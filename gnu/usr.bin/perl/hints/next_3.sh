# This file has been put together by Anno Siegel <siegel@zrz.TU-Berlin.DE>
# and Andreas Koenig <k@franz.ww.TU-Berlin.DE>. Comments, questions, and
# improvements welcome!
#
# These hints work for NeXT 3.2 and 3.3.  3.0 has it's own
# special hint file.

ccflags='-DUSE_NEXT_CTYPE'
POSIX_cflags='ccflags="-posix $ccflags"'
ldflags='-u libsys_s'
libswanted='dbm gdbm db'

lddlflags='-r'
# Give cccdlflags an empty value since Configure will detect we are
# using GNU cc and try to specify -fpic for cccdlflags.
cccdlflags=' '

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
#
# There where reports that the compiler on HPPA machines
# fails with the -O flag on pp.c.
if [ `arch` = "hppa" ]; then
pp_cflags='optimize="-g"'
fi
