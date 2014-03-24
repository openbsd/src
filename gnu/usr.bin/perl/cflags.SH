#!/bin/sh

case $PERL_CONFIG_SH in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

if test -f config_h.SH -a ! -f config.h; then
    . ./config_h.SH
    CONFIG_H=already-done
fi

warn=''

# Add -Wall for the core modules iff gcc and not already -Wall
case "$gccversion" in
'') ;;
Intel*) ;; # The Intel C++ plays gcc on TV but is not really it.
*)  case "$ccflags" in
    *-Wall*) ;;
    *) warn="$warn -Wall" ;;
    esac
    ;;
esac

# Create a test source file for testing what options can be fed to
# gcc in this system; include a selection of most common and commonly
# hairy include files.

cat >_cflags.c <<__EOT__
#include "EXTERN.h"
#include "perl.h"
/* The stdio.h, errno.h, and setjmp.h should be there in any ANSI C89. */
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
/* Just in case the inclusion of perl.h did not
 * pull in enough system headers, let's try again. */
#ifdef I_STDLIB
#include <stdlib.h>
#endif
#ifdef I_STDDEF
#include <stddef.h>
#endif
#ifdef I_STDARG
#include <stdarg.h>
#endif
#ifdef I_LIMITS
#include <limits.h>
#endif
#ifdef I_DIRENT
#include <dirent.h>
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#ifdef I_SYS_RESOURCE
#include <sys/resource.h>
#endif
#ifdef I_SYS_SELECT
#include <sys/select.h>
#endif
#if defined(HAS_SOCKET) && !defined(VMS) && !defined(WIN32) /* See perl.h. */
#include <sys/socket.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#ifdef I_SYS_TIMES
#include <sys/times.h>
#endif
#ifdef I_SYS_WAIT
#include <sys/wait.h>
#endif
/* The gcc -ansi can cause a lot of noise in Solaris because of:
 /usr/include/sys/resource.h:148: warning: 'struct rlimit64' declared inside parameter list
 */
int main(int argc, char *argv[]) {

/* Add here test code found to be problematic in some gcc platform. */

/* Off_t/off_t is a struct in Solaris with largefiles, and with gcc -ansi
 * that struct cannot be compared in some gcc releases with a flat
 * integer, such as a STRLEN. */

  IV iv;
  Off_t t0a = 2;
  STRLEN t0b = 3;
  int t0c = t0a == t0b;

/* In FreeBSD 6.2 (and probably other releases too), with -Duse64bitint,
   perl will use atoll(3).  However, that declaration is hidden in <stdlib.h>
   if we force the compiler to use -std=c89 mode.
*/
  iv = Atol("42");

  return (!t0c && (iv == 42)) ? 0 : -1; /* Try to avoid 'unused' warnings. */
}
__EOT__

stdflags=''

# Further gcc warning options.  Build up a list of options that work.
# Note that some problems may only show up with combinations of options,
# e.g. a warning might show up only with -Wall -ansi, not with either
# one individually.
# TODO:  Ponder whether to migrate this back to Configure so hints files can 
# tweak it.  Also, be paranoid about whether results we've deduced in Configure
# (especially about things like long long, which are not in C89) will still be 
# valid if we now add flags like -std=c89.

case "$gccversion" in
'') ;;
[12]*) ;; # gcc versions 1 (gasp!) and 2 are not good for this.
Intel*) ;; # # Is that you, Intel C++?
*)  for opt in -ansi -std=c89 -W -Wextra -Wdeclaration-after-statement \
		-Wendif-labels -Wc++-compat -Wwrite-strings
    do
       case " $ccflags " in
       *" $opt "*) ;; # Skip if already there.
       *) rm -f _cflags$_exe
          case "`$cc -DPERL_NO_INLINE_FUNCTIONS $cflags $warn $stdflags $opt _cflags.c -o _cflags$_exe 2>&1`" in
          *"unrecognized"*) ;;
          *"implicit declaration"*) ;; # Was something useful hidden?
          *"Invalid"*) ;;
          *"is valid for C"*) ;;
          *) if test -x _cflags$_exe
             then
               case "$opt" in
               -std*) stdflags="$stdflags $opt" ;;
               *) warn="$warn $opt" ;;
               esac
             fi
             ;;
          esac
          ;;
       esac
    done
    ;;
esac
rm -f _cflags.c _cflags$_exe

case "$gccversion" in
'') ;;
*)
  if [ "$gccansipedantic" = "" ]; then
    # If we have -Duse64bitint (or equivalent) in effect and the quadtype
    # has become 'long long', gcc -pedantic becomes unbearable (moreso
    # when combined with -Wall) because long long and LL and %lld|%Ld
    # become warn-worthy.  So let's drop the -pedantic in that case.
    case "$quadtype:$sPRId64" in
    "long long"*|*lld*|*Ld*)
      ccflags="`echo $ccflags|sed 's/-pedantic/ /'`"
      warn="`echo $warn|sed 's/-pedantic/ /'`"
      ;;
    esac
    # Similarly, since 'long long' isn't part of C89, FreeBSD 6.2 headers
    # don't declare atoll() under -std=c89, but we need it.  In general,
    # insisting on -std=c89 is inconsistent with insisting on using
    # 'long long'. So drop -std=c89 and -ansi as well if we're using 
    # 'long long' as our main integral type.
    case "$ivtype" in
    "long long")
	ccflags=`echo $ccflags|sed -e 's/-pedantic/ /' -e 's/-std=c89/ /' -e 's/-ansi/ /'`
	warn=`echo $warn|sed -e 's/-pedantic/ /' -e 's/-ansi/ /'`
	stdflags=`echo $stdflags|sed -e 's/-std=c89/ /'`
	;;
    esac
  fi
  # Using certain features (like the gcc statement expressions)
  # requires knowing whether -pedantic has been specified.
  case "$warn$ccflags" in
  *-pedantic*) warn="$warn -DPERL_GCC_PEDANTIC" ;;
  esac
  ;;
esac

# Code to set any extra flags here.
extra=''

echo "Extracting cflags (with variable substitutions)"
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
rm -f cflags
$spitshell >cflags <<!GROK!THIS!
$startsh

# Extra warnings, used e.g. for gcc.
warn="$warn"
# Extra standardness.
stdflags="$stdflags"
# Extra extra.
extra="$extra"
# what do executables look like?
_exe="$_exe"

!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>cflags <<'!NO!SUBS!'
case $PERL_CONFIG_SH in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac

: syntax: cflags [optimize=XXX] [file[.suffix]]
: displays the compiler command line for file

case "X$1" in
Xoptimize=*|X"optimize=*")
	eval "$1"
	shift
	;;
esac

also=': '
case $# in
1) also='echo 1>&2 "	  CCCMD = "'
esac

case $# in
0) set *.c; echo "The current C flags are:" ;;
esac

set `echo "$* " | sed -e 's/\.[oc] / /g' -e 's/\.obj / /g' -e "s/\\$obj_ext / /g"`

for file do

    case "$#" in
    1) ;;
    *) echo $n "    $file.c	$c" ;;
    esac

    : allow variables like toke_cflags to be evaluated

    if echo $file | grep -v / >/dev/null
    then
      eval 'eval ${'"${file}_cflags"'-""}'
    fi

    : or customize here

    case "$file" in
    *) ;;

    *) : Customization examples follow: ;;
    av) ccflags=`echo $ccflags | sed -e s/-pipe//` ;;
    deb) ccflags="$ccflags -fno-jump-tables" ;;
    hv) warn=`echo $warn | sed -e s/-Wextra//` ;;
    toke) optimize=-O0 ;;
    esac

    : The examples are intentionally unreachable as the '*)' case always
    : matches. To use them, move before the '*)' and edit as appropriate.
    : It is not a good idea to set ccflags to an absolute value here, as it
    : often contains general -D defines which are needed for correct
    : compilation. It is better to edit ccflags as shown, using interpolation
    : to add flags, or sed to remove flags.


    case "$cc" in
    *g++*)
      # Extra paranoia in case people have bad canned ccflags:
      # bad in the sense that the flags are accepted by g++,
      # but then whined about.
      for f in -Wdeclaration-after-statement -std=c89
      do
        ccflags=`echo $ccflags|sed 's/$f/ /'`
      done
      ;;
    esac
    cppflags=`echo $cppflags|sed 's/-Wdeclaration-after-statement/ /'`

    case "$cc" in
    *clang)
      # clang complains a lot about -Wunused-value which are not fixable
      warn="$warn -Wno-unused-value"
      ;;
    *g++*)
      # Without -Wno-unused-variable g++ 4.x compiles are rather unwatchable
      # because of all the warnings about Perl___notused, and g++ doesn't do
      # __attribute__((unused)) (and even if at some stage it may, people do
      # have older gcc installations), and ((void)x) isn't enough to silence
      # the noises about XS functions not using their cv parameter, so we need
      # the -Wno-unused-parameter too.
      # Yes, we lose some valid warnings, but hopefully other compilers
      # (like gcc) will still pick up those warnings.
      for o in -Wno-unused-variable -Wno-unused-parameter
      do
        case "$warn" in
        *$o*) ;;
        *) warn="$warn $o" ;;
        esac
      done
      ;;
    esac


    : Can we perhaps use $ansi2knr here
    echo "$cc -c -DPERL_CORE $ccflags $stdflags $optimize $warn $extra"
    eval "$also "'"$cc -DPERL_CORE -c $ccflags $stdflags $optimize $warn $extra"'

    . $TOP/config.sh

    # end per file behaviour
done
!NO!SUBS!
chmod 755 cflags
$eunicefix cflags
