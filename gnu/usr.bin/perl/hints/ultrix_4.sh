# hints/ultrix_4.sh
# Last updated by Andy Dougherty  <doughera@lafcol.lafayette.edu>
# Fri Feb 10 10:04:51 EST 1995
#
# Use   Configure -Dcc=gcc   to use gcc.
#
# I don't know if -g is really needed.  (AD)
case "$optimize" in
'') optimize=-g ;;
esac

# Some users have reported Configure runs *much* faster if you 
# replace all occurences of /bin/sh by /bin/sh5
# Something like:
#   sed 's!/bin/sh!/bin/sh5!g' Configure > Configure.sh5
# Then run "sh5 Configure.sh5 [your options]"

case "$myuname" in
*risc*) cat <<EOF
Note that there is a bug in some versions of NFS on the DECStation that
may cause utime() to work incorrectly.  If so, regression test io/fs
may fail if run under NFS.  Ignore the failure.
EOF
esac

# Compiler flags that depend on osversion:
case "$cc" in
*gcc*) ;;
*)
    case "$osvers" in
    *4.1*)	ccflags="$ccflags -DLANGUAGE_C -Olimit 2900" ;;
    *4.2*)	ccflags="$ccflags -DLANGUAGE_C -Olimit 2900"
		# Prototypes sometimes cause compilation errors in 4.2.
		prototype=undef   
		case "$myuname" in
		*risc*)  d_volatile=undef ;;
		esac
		;;
    *4.3*)	ccflags="$ccflags -std1 -DLANGUAGE_C -Olimit 2900" ;;
    *)	ccflags="$ccflags -std -Olimit 2900" ;;
    esac
    ;;
esac

# Other settings that depend on $osvers:
case "$osvers" in
*4.1*)	;;
*4.2*)	libswanted=`echo $libswanted | sed 's/ malloc / /'` ;;
*4.3*)	;;
*)	ranlib='ranlib' ;;
esac

groupstype='int'
