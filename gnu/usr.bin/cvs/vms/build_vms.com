$ CC :== CC/DEBUG/NOOPTIMIZE/STANDARD=VAXC/DEFINE=HAVE_CONFIG_H-
/INCLUDE_DIRECTORY=([-],[-.LIB],[-.SRC],[-.VMS])/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES
$ CC filesubr.c
$ CC filutils.c
$ CC getpass.c
$ CC getwd.c
$ CC misc.c
$ CC ndir.c
$ CC pipe.c
$ CC piped_child.c
$ CC pwd.c
$ CC rcmd.c
$ CC readlink.c
$ CC rmdir.c
$ CC stat.c
$ CC startserver.c
$ CC unlink.c
$ CC utime.c
$ CC /NOSTANDARD vmsmunch.c
$ CC waitpid.c
$ library/create openvmslib.olb filesubr.obj,-
filutils.obj,getpass.obj,getwd.obj,misc.obj,ndir.obj,pipe.obj,-
pwd.obj,rcmd.obj,readlink.obj,rmdir.obj,stat.obj,startserver.obj,-
unlink.obj,utime.obj,vmsmunch.obj,waitpid.obj
