# Sequent Dynix/Ptx v. 4 hints
# Created 1996/03/15 by Brad Howerter, bhower@wgc.woodward.com
# Use Configure -Dcc=gcc to use gcc.

# cc wants -G for dynamic loading
lddlflags='-G'

# Remove inet to avoid this error in Configure, which causes Configure
# to be unable to figure out return types:
# dynamic linker: ./ssize: can't find libinet.so,
# link with -lsocket instead of -l inet

libswanted=`echo $libswanted | sed -e 's/ inet / /'`

# Configure defaults to usenm='y', which doesn't work very well
usenm='n'

# The Perl library has to be built as a shared library so that dynamic
# loading will work (otherwise code loaded with dlopen() won't be able
# to reference symbols in the main part of perl).  Note that since
# Configure doesn't normally prompt about $d_shrplib this will cause a
# `Whoa there!'.  This is normal, just keep the recommended value.  A
# consequence of all this is that you've got to include the source
# directory in your LD_LIBRARY_PATH when you're building and testing
# perl.
d_shrplib=define

cat <<'EOM' >&4

If you get a 'Whoa there!' with regard to d_shrplib, you can ignore
it, and just keep the recommended value.

If you wish to use dynamic linking, you must use
   LD_LIBRARY_PATH=`pwd`; export LD_LIBRARY_PATH
or
   setenv LD_LIBRARY_PATH `pwd`
before running make.

EOM
