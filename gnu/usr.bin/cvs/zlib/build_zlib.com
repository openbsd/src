$ CC :== CC/DEBUG/NOOPTIMIZE/STANDARD=VAXC/DEFINE=HAVE_CONFIG_H-
/INCLUDE_DIRECTORY=([-],[-.LIB],[-.SRC],[-.VMS])/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES
$ CC adler32.c
$ CC compress.c
$ CC crc32.c
$ CC uncompr.c
$ CC deflate.c
$ CC trees.c
$ CC zutil.c
$ CC inflate.c
$ CC infblock.c
$ CC inftrees.c
$ CC infcodes.c
$ CC infutil.c
$ CC inffast.c
$ library/create zlib.olb adler32.obj,-
compress.obj,crc32.obj,uncompr.obj,deflate.obj,trees.obj,zutil.obj,-
inflate.obj,infblock.obj,inftrees.obj,infcodes.obj,infutil.obj,inffast.obj
