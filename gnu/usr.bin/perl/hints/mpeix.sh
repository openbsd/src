# Created for 5.003 by Mark Klein, mklein@dis.com.
# Substantially revised for 5.004_01 by Mark Bixby, markb@cccd.edu.
# Revised again for 5.004_69 by Mark Bixby, markb@cccd.edu.
# Revised for 5.6.0 by Mark Bixby, mbixby@power.net.
# Revised for 5.7.3 by Mark Bixby, mark@bixby.org.
# Revised for 5.8.0 by Mark Bixby, mark@bixby.org.
# Revised for 5.8.8/5.9.3 by Ken Hirsch, kenhirsch@ftml.net
#
osname='mpeix'
osvers=`uname -r | sed -e 's/.[A-Z]\.\([0-9]\)\([0-9]\)\.[0-9][0-9]/\1.\2/'`

#
# Don't use nm.  Instead, we'll use the MPEAUTOCONF environment variable
# to force error for unresolved externals.
# This is slower than nm (about 70 minutes instead of 35 minutes),
# but much more reliable.

usenm='false'
export AUTOCONF=1 MPEAUTOCONF=1

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
ccdlflags='-Xlinker -WL,xl=/usr/lib/libcurses.sl,/lib/libsvipc.sl,/usr/lib/libsocket.sl,/usr/lib/libstr.sl,/lib/libm.sl,/lib/libc.sl'
ccflags="$ccflags -DMPE -D_POSIX_SOURCE -D_SOCKET_SOURCE -D_POSIX_JOB_CONTROL"
locincpth="$locincpth /usr/local/include /usr/contrib/include /BIND/CURRENT/include /SYSLOG/PUB"
test -z "$optimize" && optimize="-O2"
ranlib='/bin/true'
# Special compiling options for certain source files.
# But what if you want -g?
regcomp_cflags='optimize=-O'
toke_cflags='ccflags="$ccflags -DARG_ZERO_IS_SCRIPT"'

#
# Linking.
#
# Build a fixed sigsetjmp that can be used in dynamic libraries
# This needs to be compiled with -O2, so I do it here, rather
# than with make
gcc -c -O2 mpeix/mpeix_setjmp.c
lddlflags="-b $PWD/mpeix_setjmp.o"

# Delete bsd and BSD from the library list.  Remove other randomly ordered
# libraries and then re-add them in their proper order (the MPE linker is
# order-sensitive).  Add additional MPE-specific libraries.
for mpe_remove in bind bsd BSD c curses m socket str svipc syslog; do
  set `echo " $libswanted " | sed -e 's/ /  /g' -e "s/ $mpe_remove //"`
  libswanted="$*"
done
libswanted="$libswanted bind syslog curses svipc socket str m c"
loclibpth="$loclibpth /usr/local/lib /usr/contrib/lib /BIND/CURRENT/lib /SYSLOG/PUB"
#
# External functions and data items.
#
# Q: Does Configure *really* get *all* of these wrong?
#
# A: Yes.  There are two MPE problems here.  The 'undef' functions exist on MPE,
# but are merely dummy routines that return ENOTIMPL or ESYSERR.  Since they're
# useless, let's just tell Perl to avoid them.  Also, a few data items are
# 'undef' because while they may exist in structures, they are uninitialized.

d_Gconvert='gcvt((x),(n),(b))'

d_inetaton='undef'

# these fields exist, but are uninitialized
d_pwage='undef'
d_pwcomment='undef'
d_pwgecos='undef'
d_pwpasswd='undef'
d_statblks='undef'

# These functions exist, 
#  but either return ENOSYS/ESYSERR/ENOSYS or work so differently
# that it is not helpful to include them

d_lchown='undef'
d_link='undef'
d_setegid='undef'
d_seteuid='undef'
d_setitimer='undef'
d_setpgid='undef'
d_setsid='undef'


# These are defined in mpeix/mpeix.c
d_gettimeod='define'
d_truncate='define'

# Include files.
#
#??i_gdbm='undef' # the port is currently incomplete

i_termios='undef' # we have termios, but not the full set (just tcget/setattr)

i_time='define'
i_systime='undef'
i_systimek='undef'
timeincl='/usr/include/time.h'
#
# Data types.
#
timetype='time_t'

# Functionality.
#
uselargefiles="$undef"

# Expected functionality provided in mpeix.c.
#

# Help gmake find mpeix.c
test -h mpeix.c || ln -s mpeix/mpeix.c mpeix.c

archobjs='mpeix.o mpeix_setjmp.o'
