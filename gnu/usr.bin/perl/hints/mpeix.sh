# The MPE/iX linker doesn't complain about unresolved symbols, and so the only
# way to test for unresolved symbols in a program is by attempting to run it.
# But this is slow, and fraught with problems, so the better solution is to use
# nm.
#
# MPE/iX lacks a fully functional native nm, so we need to use our fake nm
# script which will extract the symbol info from the native link editor and
# reformat into something nm-like.
#
# Created for 5.003 by Mark Klein, mklein@dis.com.
# Substantially revised for 5.004_01 by Mark Bixby, markb@cccd.edu.
# Revised again for 5.004_69 by Mark Bixby, markb@cccd.edu.
# Revised for 5.6.0 by Mark Bixby, mbixby@power.net.
# Revised for 5.7.3 by Mark Bixby, mark@bixby.org.
# Revised for 5.8.0 by Mark Bixby, mark@bixby.org.
#
osname='mpeix'
osvers=`uname -r | sed -e 's/.[A-Z]\.\([0-9]\)\([0-9]\)\.[0-9][0-9]/\1.\2/'`
#
# Force Configure to use our wrapper mpeix/nm script
#
PATH="$PWD/mpeix:$PATH"
nm="$PWD/mpeix/nm"
_nm=$nm
nm_opt='-configperl'
usenm='true'
#
# Work around the broken inline cat bug that corrupts here docs
#
alias -x cat=/bin/cat
#
# Various directory locations.
#
# Which ones of these does Configure get wrong?
test -z "$prefix" && prefix="/$HPACCOUNT/$HPGROUP"
archname='PA-RISC1.1'
bin="$prefix"
installman1dir="$prefix/man/man1"
installman3dir="$prefix/man/man3"
man1dir="$prefix/man/man1"
man3dir="$prefix/man/man3"
perlpath="$prefix/PERL"
scriptdir="$prefix"
startperl="#!$prefix/perl"
startsh='#!/bin/sh'
#
# Compiling.
#
test -z "$cc" && cc='gcc'
cccdlflags='none'
ccflags="$ccflags -DMPE -D_POSIX_SOURCE -D_SOCKET_SOURCE -D_POSIX_JOB_CONTROL -DIS_SOCKET_CLIB_ITSELF"
locincpth="$locincpth /usr/local/include /usr/contrib/include /BINDFW/CURRENT/include /SYSLOG/PUB"
test -z "$optimize" && optimize="-O2"
ranlib='/bin/true'
# Special compiling options for certain source files.
# But what if you want -g?
regcomp_cflags='optimize=-O'
toke_cflags='ccflags="$ccflags -DARG_ZERO_IS_SCRIPT"'
#
# Linking.
#
lddlflags='-b'
# Delete bsd and BSD from the library list.  Remove other randomly ordered
# libraries and then re-add them in their proper order (the MPE linker is
# order-sensitive).  Add additional MPE-specific libraries.
for mpe_remove in bind bsd BSD c curses m socket str svipc syslog; do
  set `echo " $libswanted " | sed -e 's/ /  /g' -e "s/ $mpe_remove //"`
  libswanted="$*"
done
libswanted="$libswanted bind syslog curses svipc socket str m c"
loclibpth="$loclibpth /usr/local/lib /usr/contrib/lib /BINDFW/CURRENT/lib /SYSLOG/PUB"
#
# External functions and data items.
#
# Q: Does Configure *really* get *all* of these wrong?
#
# A: Yes.  There are two MPE problems here.  The 'undef' functions exist on MPE,
# but are merely dummy routines that return ENOTIMPL or ESYSERR.  Since they're
# useless, let's just tell Perl to avoid them.  Also, a few data items are
# 'undef' because while they may exist in structures, they are uninitialized.
#
# The 'define' cases are a bit weirder.  MPE has a libc.a, libc.sl, and two
# special kernel shared libraries, /SYS/PUB/XL and /SYS/PUB/NL.  Much of what
# is in libc.a is duplicated within XL and NL, so when we created libc.sl, we
# omitted the duplicated functions.  Since Configure end ups scanning libc.sl,
# we need to 'define' the functions that had been removed.
#
# We don't want to scan XL or NL because we would find way too many POSIX or
# Unix named functions that are really vanilla MPE functions that do something
# completely different than on POSIX or Unix.
d_crypt='define'
d_dbmclose='undef'
d_difftime='define'
d_dlerror='undef'
d_dlopen='undef'
d_Gconvert='gcvt((x),(n),(b))'
d_getnbyaddr='define'
d_getnbyname='define'
d_getpbyname='define'
d_getpbynumber='define'
d_getsbyname='define'
d_getsbyport='define'
d_gettimeod='undef'
d_inetaton='undef'
d_link='undef'
d_mblen='define'
d_mbstowcs='define'
d_mbtowc='define'
d_memchr='define'
d_memcmp='define'
d_memcpy='define'
d_memmove='define'
d_memset='define'
d_pwage='undef'
d_pwcomment='undef'
d_pwgecos='undef'
d_pwpasswd='undef'
d_setegid='undef'
d_seteuid='undef'
d_setitimer='undef'
d_setpgid='undef'
d_setsid='undef'
d_setvbuf='define'
d_statblks='undef'
d_strchr='define'
d_strcoll='define'
d_strerrm='strerror(e)'
d_strerror='define'
d_strtod='define'
d_strtol='define'
d_strtoul='define'
d_strxfrm='define'
d_syserrlst='define'
d_time='define'
d_wcstombs='define'
d_wctomb='define'
#
# Include files.
#
i_gdbm='undef' # the port is currently incomplete
i_termios='undef' # we have termios, but not the full set (just tcget/setattr)
i_time='define'
i_systime='undef'
i_systimek='undef'
timeincl='/usr/include/time.h'
#
# Data types.
#
timetype='time_t'
#
# Functionality.
#
uselargefiles="$undef"
#
# Expected functionality provided in mpeix.c.
#
archobjs='mpeix.o'

# Help gmake find mpeix.c
test -h mpeix.c || ln -s mpeix/mpeix.c mpeix.c

d_gettimeod='define'
d_truncate='define'
