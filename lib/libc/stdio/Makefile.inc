#	$OpenBSD: Makefile.inc,v 1.27 2014/11/25 17:01:49 millert Exp $

# stdio sources
.PATH: ${LIBCSRCDIR}/stdio

CFLAGS+=-DFLOATING_POINT -DPRINTF_WIDE_CHAR -DSCANF_WIDE_CHAR

SRCS+=	asprintf.c clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c fgetc.c \
	fgetln.c fgetpos.c fgets.c fileno.c findfp.c flags.c fmemopen.c \
	fopen.c fprintf.c fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
	fseek.c fsetpos.c ftell.c funopen.c fvwrite.c fwalk.c fwrite.c \
	getc.c getchar.c getw.c makebuf.c mktemp.c open_memstream.c \
	open_wmemstream.c perror.c printf.c putc.c putchar.c puts.c putw.c \
	refill.c remove.c rewind.c rget.c scanf.c setbuf.c setbuffer.c \
	setvbuf.c snprintf.c sprintf.c sscanf.c stdio.c tempnam.c tmpfile.c \
	tmpnam.c ungetc.c vasprintf.c vfprintf.c vfscanf.c vprintf.c vscanf.c \
	vsnprintf.c vsprintf.c vsscanf.c wbuf.c wsetup.c flockfile.c \
	fgetwc.c fgetws.c fputwc.c fputws.c fwide.c getwc.c getwchar.c \
	putwc.c putwchar.c ungetwc.c \
	fwprintf.c swprintf.c vfwprintf.c vswprintf.c vwprintf.c wprintf.c \
	fwscanf.c swscanf.c vfwscanf.c vswscanf.c vwscanf.c wscanf.c \
	getdelim.c getline.c dprintf.c vdprintf.c

MAN+=	fclose.3 ferror.3 fflush.3 fgetln.3 fgets.3 fmemopen.3 fopen.3 fputs.3 \
	fread.3 fseek.3 funopen.3 getc.3 mktemp.3 open_memstream.3 perror.3 \
	printf.3 putc.3 remove.3 scanf.3 setbuf.3 setvbuf.3 stdio.3 tmpnam.3 \
	ungetc.3 fgetws.3 fputws.3 fwide.3 getwc.3 putwc.3 ungetwc.3 wprintf.3 \
	wscanf.3 getdelim.3

MLINKS+=ferror.3 clearerr.3 ferror.3 feof.3 ferror.3 fileno.3
MLINKS+=fflush.3 fpurge.3
MLINKS+=fopen.3 fdopen.3 fopen.3 freopen.3
MLINKS+=fputs.3 puts.3
MLINKS+=fread.3 fwrite.3
MLINKS+=fseek.3 fgetpos.3 fseek.3 fsetpos.3 fseek.3 ftell.3 fseek.3 rewind.3
MLINKS+=fseek.3 fseeko.3 fseek.3 ftello.3
MLINKS+=funopen.3 fropen.3 funopen.3 fwopen.3
MLINKS+=getc.3 fgetc.3 getc.3 getchar.3 getc.3 getw.3
MLINKS+=getdelim.3 getline.3
MLINKS+=mktemp.3 mkstemp.3
MLINKS+=mktemp.3 mkdtemp.3
MLINKS+=mktemp.3 mkstemps.3
MLINKS+=mktemp.3 mkostemp.3
MLINKS+=mktemp.3 mkostemps.3
MLINKS+=open_memstream.3 open_wmemstream.3
MLINKS+=printf.3 fprintf.3 printf.3 snprintf.3 printf.3 sprintf.3 \
	printf.3 vfprintf.3 printf.3 vprintf.3 printf.3 vsnprintf.3 \
	printf.3 vsprintf.3 printf.3 asprintf.3 printf.3 vasprintf.3 \
	printf.3 dprintf.3 printf.3 vdprintf.3
MLINKS+=putc.3 fputc.3 putc.3 putchar.3 putc.3 putw.3
MLINKS+=scanf.3 fscanf.3 scanf.3 sscanf.3 scanf.3 vfscanf.3 scanf.3 vscanf.3 \
	scanf.3 vsscanf.3
MLINKS+=setbuf.3 setbuffer.3 setbuf.3 setlinebuf.3
MLINKS+=tmpnam.3 tempnam.3 tmpnam.3 tmpfile.3
MLINKS+=wprintf.3 fwprintf.3 wprintf.3 swprintf.3 wprintf.3 vwprintf.3 \
	wprintf.3 vfwprintf.3 wprintf.3 vswprintf.3
MLINKS+=wscanf.3 fwscanf.3 wscanf.3 swscanf.3 wscanf.3 vfwscanf.3 wscanf.3 \
	vswscanf.3 wscanf.3 vwscanf.3

MLINKS+=getwc.3 fgetwc.3 getwc.3 getwchar.3
MLINKS+=putwc.3 fputwc.3 putwc.3 putwchar.3
