# Makefile for OS/2 (Watcom-C) for use with the watcom make.
# Written 11/96 by Ullrich von Bassewitz (uz@musoftware.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# The directory, where the IBM TCP/IP developers toolkit is installed. As far
# as I remember, c:\mptn is the default location. If it is not, it is still
# a good choice :-)
tcpip_dir       = c:\mptn
tcpip_libdir    = $(tcpip_dir)\lib
tcpip_incdir    = $(tcpip_dir)\inc

# Directory for source files and objects
srcdir          = .
top_srcdir      = ..
lib_dir         = $(top_srcdir)\lib
cvs_srcdir      = $(top_srcdir)\src
zlib_dir        = $(top_srcdir)\zlib

# Define the stuff used for building the executable
CC = WCC386
LD = WLINK
CFLAGS = -bm -bt=OS2 -I$(srcdir) -I$(lib_dir) -I$(cvs_srcdir) -I$(zlib_dir) &
        -DIBM_CPP -DHAVE_CONFIG_H -DTCPIP_IBM -d1 -onatx -zp4 -5s -fpi87 -zq &
        -w2 -ze -I$(tcpip_incdir)

# Tell the make where the C files are located
.c:     $(srcdir);$(lib_dir);$(cvs_srcdir);$(zlib_dir)

# Somewhat modified generic rule for .obj files. Don't put the .obj file into
# the current directory, use the source directory instead.
.c.obj: .AUTODEPEND
  $(CC) $(CFLAGS) -fo=$*.obj $^*

# object files from OS/2 sources
OS2_OBJECTS = &
        $(srcdir)\mkdir.obj &
        $(srcdir)\pwd.obj &
        $(srcdir)\filesubr.obj &
        $(srcdir)\run.obj &
        $(srcdir)\stripslash.obj &
        $(srcdir)\rcmd.obj &
        $(srcdir)\waitpid.obj &
        $(srcdir)\popen.obj &
        $(srcdir)\porttcp.obj &
        $(srcdir)\getpass.obj

# object files from ..\src
COMMON_OBJECTS = &
        $(cvs_srcdir)\add.obj &
        $(cvs_srcdir)\admin.obj &
        $(cvs_srcdir)\buffer.obj &
        $(cvs_srcdir)\checkin.obj &
        $(cvs_srcdir)\checkout.obj &
        $(cvs_srcdir)\classify.obj &
        $(cvs_srcdir)\client.obj &
        $(cvs_srcdir)\commit.obj &
        $(cvs_srcdir)\create_adm.obj &
        $(cvs_srcdir)\cvsrc.obj &
        $(cvs_srcdir)\diff.obj &
        $(cvs_srcdir)\edit.obj &
        $(cvs_srcdir)\entries.obj &
        $(cvs_srcdir)\error.obj &
        $(cvs_srcdir)\expand_path.obj &
        $(cvs_srcdir)\fileattr.obj &
        $(cvs_srcdir)\find_names.obj &
        $(cvs_srcdir)\hash.obj &
        $(cvs_srcdir)\history.obj &
        $(cvs_srcdir)\ignore.obj &
        $(cvs_srcdir)\import.obj &
        $(cvs_srcdir)\lock.obj &
        $(cvs_srcdir)\log.obj &
        $(cvs_srcdir)\login.obj &
        $(cvs_srcdir)\logmsg.obj &
        $(cvs_srcdir)\main.obj &
        $(cvs_srcdir)\mkmodules.obj &
        $(cvs_srcdir)\modules.obj &
        $(cvs_srcdir)\myndbm.obj &
        $(cvs_srcdir)\no_diff.obj &
        $(cvs_srcdir)\parseinfo.obj &
        $(cvs_srcdir)\patch.obj &
        $(cvs_srcdir)\rcs.obj &
        $(cvs_srcdir)\rcscmds.obj &
        $(cvs_srcdir)\recurse.obj &
        $(cvs_srcdir)\release.obj &
        $(cvs_srcdir)\remove.obj &
        $(cvs_srcdir)\repos.obj &
        $(cvs_srcdir)\root.obj &
        $(cvs_srcdir)\rtag.obj &
        $(cvs_srcdir)\scramble.obj &
        $(cvs_srcdir)\server.obj &
        $(cvs_srcdir)\status.obj &
        $(cvs_srcdir)\subr.obj &
        $(cvs_srcdir)\tag.obj &
        $(cvs_srcdir)\update.obj &
        $(cvs_srcdir)\watch.obj &
        $(cvs_srcdir)\wrapper.obj &
        $(cvs_srcdir)\vers_ts.obj &
        $(cvs_srcdir)\version.obj &
        $(cvs_srcdir)\zlib.obj
# end of $COMMON_OBJECTS

# objects from ..\lib
LIB_OBJECTS = &
        $(lib_dir)\getopt.obj &
        $(lib_dir)\getopt1.obj &
        $(lib_dir)\getline.obj &
        $(lib_dir)\getwd.obj &
        $(lib_dir)\savecwd.obj &
        $(lib_dir)\sighandle.obj &
        $(lib_dir)\yesno.obj &
        $(lib_dir)\vasprintf.obj &
        $(lib_dir)\xgetwd.obj &
        $(lib_dir)\md5.obj &
        $(lib_dir)\fnmatch.obj &
        $(lib_dir)\regex.obj &
        $(lib_dir)\getdate.obj &
        $(lib_dir)\valloc.obj

ZLIB_OBJECTS = &
        $(zlib_dir)\adler32.obj &
        $(zlib_dir)\compress.obj &
        $(zlib_dir)\crc32.obj &
        $(zlib_dir)\uncompr.obj &
        $(zlib_dir)\deflate.obj &
        $(zlib_dir)\trees.obj &
        $(zlib_dir)\zutil.obj &
        $(zlib_dir)\inflate.obj &
        $(zlib_dir)\infblock.obj &
        $(zlib_dir)\inftrees.obj &
        $(zlib_dir)\infcodes.obj &
        $(zlib_dir)\infutil.obj &
        $(zlib_dir)\inffast.obj

OBJECTS = $(COMMON_OBJECTS) $(LIB_OBJECTS) $(OS2_OBJECTS) $(ZLIB_OBJECTS)

cvs.exe:        $(OBJECTS)
        $(LD) SYSTEM os2v2 DEBUG all NAME cvs.exe OPTION dosseg &
        OPTION stack=32K FILE $(cvs_srcdir)\*.obj,$(lib_dir)\*.obj, &
        $(srcdir)\*.obj,$(zlib_dir)\*.obj &
        LIBRARY $(tcpip_libdir)\tcp32dll.lib, &
        $(tcpip_libdir)\so32dll.lib

strip:  cvs.exe         .SYMBOLIC
        -wstrip cvs.exe
