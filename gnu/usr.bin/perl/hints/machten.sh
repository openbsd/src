# machten.sh
# This is for MachTen 4.0.2.  It might work on other versions too.
#
# MachTen users might need a fixed tr from ftp.tenon.com.  This should
# be described in the MachTen release notes.
#
# MachTen 2.x has its own hint file.
#
# This file has been put together by Andy Dougherty
# <doughera@lafcol.lafayette.edu> based on comments from lots of
# folks, especially 
# 	Mark Pease <peasem@primenet.com>
#	Martijn Koster <m.koster@webcrawler.com>
#	Richard Yeh <rcyeh@cco.caltech.edu>
#
# File::Find's use of link count disabled by Dominic Dunlop 950528
# Perl's use of sigsetjmp etc. disabled by Dominic Dunlop 950521
#
# Comments, questions, and improvements welcome!
#
# MachTen 4.X does support dynamic loading, but perl doesn't
# know how to use it yet.
#
#  Updated by Dominic Dunlop <domo@tcp.ip.lu>
#  Tue May 28 11:20:08 WET DST 1996

# Configure doesn't know how to parse the nm output.
usenm=undef

# At least on PowerMac, doubles must be aligned on 8 byte boundaries.
# I don't know if this is true for all MachTen systems, or how to
# determine this automatically.
alignbytes=8

# There appears to be a problem with perl's use of sigsetjmp and
# friends.  Use setjmp and friends instead.
d_sigsetjmp='undef' 

# MachTen always reports ony two links to directories, even if they
# contain subdirectories.  Consequently, we use this variable to stop
# File::Find using the link count to determine whether there are
# subdirectories to be searched.  This will generate a harmless message:
# Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
#	Propagating recommended variable dont_use_nlink
dont_use_nlink=define

cat <<'EOM' >&4

Tests
	io/fs test 4  and
	op/stat test 3
may fail since MachTen does not return a useful nlinks field to stat
on directories.

At the end of Configure, you will see a harmless message

Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
	Propagating recommended variable dont_use_nlink

Read the File::Find documentation for more information.

EOM
