$ CC :== CC/DEBUG/NOOPTIMIZE/STANDARD=VAXC/DEFINE=HAVE_CONFIG_H-
/INCLUDE_DIRECTORY=([-],[-.LIB],[-.SRC],[-.VMS])/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES
$ CC diff.c
$ CC analyze.c
$ CC cmpbuf.c
$ CC dir.c
$ CC io.c
$ CC util.c
$ CC context.c
$ CC ed.c
$ CC ifdef.c
$ CC normal.c
$ CC side.c
$ CC version.c
$ CC diff3.c
$ library/create diff.olb diff.obj,analyze.obj,cmpbuf.obj,-
dir.obj,io.obj,util.obj,context.obj,ed.obj,ifdef.obj,normal.obj,-
side.obj,version.obj,diff3.obj
