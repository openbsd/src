usevfork='true'
case "$usemymalloc" in
    "") usemymalloc='n'
        ;;
esac
libswanted=`echo $libswanted | sed 's/ malloc / /'`

objformat=`/usr/bin/objformat`
if [ x$objformat = xelf ]; then
    libpth="/usr/lib /usr/local/lib"
    glibpth="/usr/lib /usr/local/lib"
    ldflags="-Wl,-E "
    lddlflags="-shared "
else
    if [ -e /usr/lib/aout ]; then
        libpth="/usr/lib/aout /usr/local/lib /usr/lib"
        glibpth="/usr/lib/aout /usr/local/lib /usr/lib"
    fi
    lddlflags='-Bshareable'
fi
cccdlflags='-DPIC -fPIC'

ccflags="${ccflags} -DHAS_FPSETMASK -DHAS_FLOATINGPOINT_H"
if /usr/bin/file -L /usr/lib/libc.so | /usr/bin/grep -vq "not stripped" ; then
    usenm=false
fi

signal_t='void'
d_voidsig='define'

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
        lc_r=`/sbin/ldconfig -r|grep ':-lc_r'|awk '{print $NF}'|sed -n '$p'`
	    	ldflags="-pthread $ldflags"
	      d_gethostbyaddr_r="undef"
	      d_gethostbyaddr_r_proto="0"

	set `echo X "$libswanted "| sed -e 's/ c / c_r /'`
	shift
	libswanted="$*"
	# Configure will probably pick the wrong libc to use for nm scan.
	# The safest quick-fix is just to not use nm at all...
	usenm=false

        unset lc_r

	# Even with the malloc mutexes the Perl malloc does not
	# seem to be threadsafe in MidnightBSD?
	case "$usemymalloc" in
	'') usemymalloc=n ;;
	esac
esac
EOCBU

# malloc wrap works
case "$usemallocwrap" in
'') usemallocwrap='define' ;;
esac
