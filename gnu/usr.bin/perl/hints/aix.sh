# hints/aix.sh
# AIX 3.x.x hints thanks to Wayne Scott <wscott@ichips.intel.com>
# AIX 4.1 hints thanks to Christopher Chan-Nui <channui@austin.ibm.com>.
# Merged on Mon Feb  6 10:22:35 EST 1995 by
#   Andy Dougherty  <doughera@lafcol.lafayette.edu>


# Configure finds setrgid and setruid, but they're useless.  The man
# pages state:
#    setrgid: The EPERM error code is always returned.
#    setruid: The EPERM error code is always returned. Processes cannot
#             reset only their real user IDs.
d_setrgid='undef'
d_setruid='undef'

alignbytes=8

usemymalloc='n'

# Make setsockopt work correctly.  See man page.
# ccflags='-D_BSD=44'

# uname -m output is too specific and not appropriate here
case "$archname" in
'') archname="$osname" ;;
esac

case "$osvers" in
3*) d_fchmod=undef
    ccflags='-D_ALL_SOURCE'
    ;;
*)  # These hints at least work for 4.x, possibly other systems too.
    d_setregid='undef'
    d_setreuid='undef'
    ccflags='-qmaxmem=8192 -D_ALL_SOURCE -D_ANSI_C_SOURCE -D_POSIX_SOURCE'
    nm_opt='-B'
    ;;
esac

# The optimizer in 4.1.1 apparently generates bad code for scope.c.
# Configure doesn't offer an easy way to propagate extra variables
# only for certain cases, so the following contortion is required:
# This is probably not needed in 5.002 and later.
# scope_cflags='case "$osvers" in 4.1*) optimize=" ";; esac'

# Changes for dynamic linking by Wayne Scott <wscott@ichips.intel.com>
#
# Tell perl which symbols to export for dynamic linking.
case "$cc" in
*gcc*) ccdlflags='-Xlinker -bE:perl.exp' ;;
*) ccdlflags='-bE:perl.exp' ;;
esac

# The first 3 options would not be needed if dynamic libs. could be linked
# with the compiler instead of ld.
# -bI:$(PERL_INC)/perl.exp  Read the exported symbols from the perl binary
# -bE:$(BASEEXT).exp        Export these symbols.  This file contains only one
#                           symbol: boot_$(EXP)  can it be auto-generated?
case "$osvers" in
3*) 
lddlflags='-H512 -T512 -bhalt:4 -bM:SRE -bI:$(PERL_INC)/perl.exp -bE:$(BASEEXT).exp -e _nostart -lc'
    ;;
*) 
lddlflags='-H512 -T512 -bhalt:4 -bM:SRE -bI:$(PERL_INC)/perl.exp -bE:$(BASEEXT).exp -b noentry -lc'

;;
esac
