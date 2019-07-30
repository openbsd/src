# hints/gnu.sh
# Last modified: Thu Dec 10 20:47:28 CET 1998
# Mark Kettenis <kettenis@phys.uva.nl>

# libnsl is unusable on the Hurd.
# XXX remove this once SUNRPC is implemented.
set `echo X "$libswanted "| sed -e 's/ bsd / /' -e 's/ nsl / /' -e 's/ c / pthread /'`
shift
libswanted="$*"

# Debian 4.0 puts ndbm in the -lgdbm_compat library.
libswanted="$libswanted gdbm_compat"

case "$optimize" in
'') optimize='-O2' ;;
esac

case "$plibpth" in
'') plibpth=`gcc -print-search-dirs | grep libraries |
        cut -f2- -d= | tr ':' $trnl | grep -v 'gcc' | sed -e 's:/$::'`
    set X $plibpth # Collapse all entries on one line
    shift
    plibpth="$*"
    ;;
esac

# Flags needed to produce shared libraries.
lddlflags='-shared'

# Flags needed by programs that use dynamic linking.
ccdlflags='-Wl,-E'

# Debian bug #258618
ccflags="-D_GNU_SOURCE $ccflags"

cat > UU/uselargefiles.cbu <<'EOCBU'
# This script UU/uselargefiles.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use large files.
case "$uselargefiles" in
''|$define|true|[yY]*)
# Keep this in the left margin.
ccflags_uselargefiles="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"

	ccflags="$ccflags $ccflags_uselargefiles"
	;;
esac
EOCBU

# The following routines are only available as stubs in GNU libc.
# XXX remove this once metaconf detects the GNU libc stubs.
d_msgctl='undef'
d_msgget='undef'
d_msgrcv='undef'
d_msgsnd='undef'
d_semctl='undef'
d_semget='undef'
d_semop='undef'
d_shmat='undef'
d_shmctl='undef'
d_shmdt='undef'
d_shmget='undef'
