$ CC :== CC/DEBUG/NOOPTIMIZE/STANDARD=VAXC/DEFINE=HAVE_CONFIG_H-
/INCLUDE_DIRECTORY=([-],[-.LIB],[-.SRC],[-.VMS])/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES
$ CC fnmatch.c
$ CC getdate.c
$ CC getline.c
$ CC getopt.c
$ CC getopt1.c
$ CC md5.c
$ CC regex.c
$ CC savecwd.c
$ CC sighandle.c
$ CC stripslash.c
$ CC valloc.c
$ CC xgetwd.c
$ CC yesno.c
$ library/create gnulib.olb fnmatch.obj,getdate.obj,getline.obj,-
getopt.obj,getopt1.obj,md5.obj,regex.obj,savecwd.obj,sighandle.obj,-
stripslash.obj,valloc.obj,xgetwd.obj,yesno.obj
