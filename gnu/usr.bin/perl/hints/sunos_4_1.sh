# hints/sunos_4_1.sh
# Last modified:  Thu Feb  8 11:46:05 EST 1996
# Andy Dougherty  <doughera@lafcol.lafayette.edu>

case "$cc" in
*gcc*)	usevfork=false ;;
*)	usevfork=true ;;
esac

# Configure will issue a WHOA warning.  The problem is that
# Configure finds getzname, not tzname.  If you're in the System V
# environment, you can set d_tzname='define' since tzname[] is
# available in the System V environment.
d_tzname='undef'

# SunOS 4.1.3 has two extra fields in struct tm.  This works around
# the problem.  Other BSD platforms may have similar problems.
POSIX_cflags='ccflags="$ccflags -DSTRUCT_TM_HASZONE"'

# check if user is in a bsd or system 5 type environment
if cat -b /dev/null 2>/dev/null
then # bsd
      groupstype='int'
else # sys5
      groupstype='gid_t'
fi
 
