# irix_6_2.sh
# from Krishna Sethuraman, krishna@sgi.com
# Date: Tue Aug 22 00:38:26 PDT 1995
# removed -ansiposix and -D_POSIX_SOURCE cuz it was choking

# Perl built with this hints file under IRIX 6.2 passes
# all tests (`make test').

ld=ld
i_time='define'
cc="cc -32"
ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -Olimit 3000"
#ccflags="$ccflags -Olimit 3000" # this line builds perl but not tk (beta 8)
lddlflags="-32 -shared"
# Configure would suggest the default -Kpic, which won't work for SGI.
# Configure will respect this blank hint value instead.
cccdlflags=' '

# We don't want these libraries.  Anyone know why?
set `echo X "$libswanted "|sed -e 's/ socket / /' -e 's/ nsl / /' -e 's/ dl / /'`
shift
libswanted="$*"
# Don't need sun crypt bsd PW under 6.2.  You *may* need to link
# with these if you want to run perl built under 6.2 on a 5.3 machine
# (I haven't checked)
#set `echo X "$libswanted "|sed -e 's/ sun / /' -e 's/ crypt / /' -e 's/ bsd / /' -e 's/ PW / /'`
#shift
#libswanted="$*"
