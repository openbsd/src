1,/START OF targmatch.h/	d
/END OF targmatch.h/,$		d
s/^#if/KEEP #if/
s/^#endif/KEEP #endif/
s/^[ 	]*#.*$//
s/^KEEP #/#/
s/[ 	]*\\$//
s/[| 	][| 	]*\([^|() 	][^|() 	]*\)[ 	]*|/{ "\1", NULL },/g
s/[| 	][| 	]*\([^|() 	][^|() 	]*\)[ 	]*)/{ "\1",/g
s/^[ 	]*targ_defvec=\([^ 	]*\)/#if !defined (SELECT_VECS) || defined (HAVE_\1)\
\&\1\
#else\
UNSUPPORTED_TARGET\
#endif\
},/
s/.*=.*//
s/;;//
