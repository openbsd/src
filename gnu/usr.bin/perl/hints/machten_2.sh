# machten.sh
# This file has been put together by Mark Pease <peasem@primenet.com>
# Comments, questions, and improvements welcome!
#
# MachTen does not support dynamic loading. If you wish to, you
# can get <ftp://tsx-11.mit.edu/pub/linux/sources/libs/dld-src-3.2.4.tar.gz>
# compile and install. This is the version of DLD that works with the
# ext/DynaLoader/dl_dld.xs in the perl5 package. Have fun!
#
#  Original version was for MachTen 2.1.1.
#  Last modified by Andy Dougherty   <doughera@lafcol.lafayette.edu>
#  Fri Feb  9 13:04:45 EST 1996

# I don't know why this is needed.  It might be similar to NeXT's
# problem.  See hints/next_3.sh.
usemymalloc='n'

so='none'
# These are useful only if you have DLD, but harmless otherwise.
# Make sure gcc doesn't use -fpic.
cccdlflags=' '  # That's an empty space.
lddlflags='-r'
dlext='o'

# MachTen does not support POSIX enough to compile the POSIX module.
useposix=false

#MachTen might have an incomplete Berkeley DB implementation.
i_db=$undef

#MachTen versions 2.X have no hard links.  This variable is used
# by File::Find.
# This will generate a harmless message:
# Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
#	Propagating recommended variable dont_use_nlink
dont_use_nlink=define

cat <<'EOM' >&4

Tests
	io/fs test 4  and
	op/stat test 3
may fail since MachTen versions 2.X have no hard links.

At the end of Configure, you will see a harmless message

Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
	Propagating recommended variable dont_use_nlink

Read the File::Find documentation for more information.

EOM
