# set -x

# Install the perl and its libraries anywhere:
case "$userelocatableinc" in
'') userelocatableinc='define' ;;
esac

# The Android linker has some unusual behavior: No matter what
# path is passed in to dlopen(), it'll only use the path's
# basename when trying to find a cached library.
# Unfortunately, this is quite problematic for us, since for example,
# Hash::Util and List::Util both end up creating a Util.so --
# So if you load List::Util and then Hash::Util, the dlopen() for
# the latter will return the handle for the former.
# See the implementation for details:
# https://code.google.com/p/android-source-browsing/source/browse/linker/linker.c?repo=platform--bionic&r=9ec0f03a0d0b17bbb94ac0b9fef6add28a133c3a#1231
# What d_libname_unique does is inform MakeMaker that, rather than
# creating Hash/Util/Util.so, it needs to make Hash/Util/Perl_Hash_Util.so
d_libname_unique='define'

# On Android the shell is /system/bin/sh:
targetsh='/system/bin/sh'
case "$usecrosscompile" in
define) ;;
   # If we aren't cross-compiling, then sh should also point
   # to /system/bin/sh.
*) sh=$targetsh ;;
esac

# Make sure that we look for libm
libswanted="$libswanted m"

# Down with locales!
# https://github.com/android/platform_bionic/blob/master/libc/CAVEATS
d_locconv='undef'
d_setlocale='undef'
d_setlocale_r='undef'
i_locale='undef'

# https://code.google.com/p/android-source-browsing/source/browse/libc/netbsd/net/getservent_r.c?repo=platform--bionic&r=ca6fe7bebe3cc6ed7e2db5a3ede2de0fcddf411d#95
d_getservent_r='undef'

# Bionic defines several stubs that just warn and return NULL
# https://gitorious.org/0xdroid/bionic/blobs/70b2ef0ec89a9c9d4c2d4bcab728a0e72bafb18e/libc/bionic/stubs.c
# https://android.googlesource.com/platform/bionic/+/master/libc/bionic/stubs.cpp

# If they warn with 'FIX' or 'Android', assume they are the stubs
# we want to avoid.

# These are all stubs as well, but the core doesn't use them:
# getusershell setusershell endusershell

# This script UU/archname.cbu will get 'called-back' by Configure.
$cat > UU/archname.cbu <<'EOCBU'
# egrep pattern to detect a stub warning on Android.
# Right now we're checking for:
# Android 2.x: FIX ME! implement FUNC
# Android 4.x: FUNC is not implemented on Android
android_stub='FIX|Android'

$cat > try.c << 'EOM'
#include <netdb.h>
int main() { (void) getnetbyname("foo"); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_getnbyname="$undef"
fi

$cat > try.c << 'EOM'
#include <netdb.h>
int main() { (void) getnetbyaddr((uint32_t)1, AF_INET); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_getnbyaddr="$undef"
fi

$cat > try.c << 'EOM'
#include <stdio.h>
#include <mntent.h>
#include <unistd.h>
int main() { (void) getmntent(stdout); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_getmntent="$undef"
fi

$cat > try.c << 'EOM'
#include <netdb.h>
int main() { (void) getprotobyname("foo"); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_getpbyname="$undef"
fi

$cat > try.c << 'EOM'
#include <netdb.h>
int main() { (void) getprotobynumber(1); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_getpbynumber="$undef"
fi

$cat > try.c << 'EOM'
#include <sys/types.h>
#include <pwd.h>
int main() { endpwent(); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_endpwent="$undef"
fi

$cat > try.c << 'EOM'
#include <unistd.h>
int main() { (void) ttyname(STDIN_FILENO); return(0); }
EOM
$cc $ccflags try.c -o try
android_warn=`$run ./try 2>&1 | $egrep "$android_stub"`
if test "X$android_warn" != X; then
   d_ttyname="$undef"
fi

EOCBU

if $test "X$targetrun" = "Xadb"; then

$rm $run $to $from $targetmkdir

case "$src" in
    /*) run=$src/Cross/run
            targetmkdir=$src/Cross/mkdir
            to=$src/Cross/to
            from=$src/Cross/from
            ;;
    *)  pwd=`test -f ../Configure && cd ..; pwd`
            run=$pwd/Cross/run
            targetmkdir=$pwd/Cross/mkdir
            to=$pwd/Cross/to
            from=$pwd/Cross/from
               ;;
esac

targetrun=adb-shell
targetto=adb-push
targetfrom=adb-pull
run=$run-$targetrun
to=$to-$targetto
from=$from-$targetfrom

$cat >$run <<EOF
#!/bin/sh
doexit="echo \\\$? >$targetdir/output.status"
env=''
case "\$1" in
-cwd)
  shift
  cwd=\$1
  shift
  ;;
esac
case "\$1" in
-env)
  shift
  env=\$1
  shift
  ;;
esac
case "\$cwd" in
'') cwd=$targetdir ;;
esac
case "\$env" in
'') env="echo "
esac
exe=\$1
shift
args=\$@
$to \$exe > /dev/null 2>&1

# send copy results to /dev/null as otherwise it outputs speed stats which gets in our way.
# sometimes there is no $?, I dunno why? we then get Cross/run-adb-shell: line 39: exit: XX: numeric argument required
adb -s $targethost shell "sh -c '(cd \$cwd && \$env ; \$exe \$args > $targetdir/output.stdout 2>$targetdir/output.stderr) ; \$doexit '" > /dev/null

rm output.stdout output.stderr output.status 2>/dev/null

$from output.stdout
$from output.stderr
$from output.status

# We get back Ok\r\n on android for some reason, grrr:
$cat output.stdout | $tr -d '\r'
if test -s output.stderr; then
    $cat output.stderr | $tr -d '\r' >&2
fi

result_status=\`$cat output.status | $tr -d '\r'\`

rm output.stdout output.stderr output.status

# Also, adb doesn't exit with the commands exit code, like ssh does, double-grr
exit \$result_status

EOF
$chmod a+rx $run

$cat >$targetmkdir <<EOF
#!/bin/sh
adb -s $targethost shell "mkdir -p \$@"
EOF
$chmod a+rx $targetmkdir

$cat >$to <<EOF
#!/bin/sh
for f in \$@
do
  case "\$f" in
  /*)
    adb -s $targethost push \$f \$f            || exit 1
    ;;
  *)
    (adb -s $targethost push \$f $targetdir/\$f < /dev/null 2>&1) || exit 1
    ;;
  esac
done
exit 0
EOF
$chmod a+rx $to

$cat >$from <<EOF
#!/bin/sh
for f in \$@
do
  $rm -f \$f
  (adb -s $targethost pull $targetdir/\$f . > /dev/null 2>&1) || exit 1
done
exit 0
EOF
$chmod a+rx $from

fi # Cross-compiling with adb

case "$usecrosscompile" in
define)
if $test "X$hostosname" = "Xdarwin"; then
  firstmakefile=GNUmakefile;
fi

# When cross-compiling, full_csh and d_csh will get the
# host's values, which is all sorts of wrong.  So unless
# full_csh has been set on the command line, set d_csh to
# undef.
case "$full_csh" in
'') d_csh="$undef"
;;
esac

;;
*)
ldflags="$ldflags -L/system/lib"
;;
esac

osvers="`$run getprop ro.build.version.release`"

# We want osname to be linux-android during Configure,
# but plain 'android' afterwards.
case "$src" in
    /*) pwd="$src";;
    *)  pwd=`test -f ../Configure && cd ..; pwd`
        ;;
esac

$cat <<EOO >> $pwd/config.arch

osname='android'
eval "libpth='$libpth /system/lib /vendor/lib'"
EOO

# Android is a linux variant, so run those hints.
. ./hints/linux.sh
