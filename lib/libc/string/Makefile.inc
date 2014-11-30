#	$OpenBSD: Makefile.inc,v 1.36 2014/11/30 19:43:56 deraadt Exp $

# string sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/string ${LIBCSRCDIR}/string

SRCS+=	explicit_bzero.c memccpy.c memmem.c memrchr.c stpcpy.c stpncpy.c \
	strcasecmp.c strcasestr.c strcoll.c strdup.c \
	strerror.c strerror_r.c strmode.c strndup.c strnlen.c \
	strsignal.c strtok.c strxfrm.c \
	timingsafe_bcmp.c timingsafe_memcmp.c \
	wcscat.c wcschr.c wcscmp.c wcscpy.c wcscspn.c wcslcat.c wcslcpy.c \
	wcslen.c wcsncat.c wcsncmp.c wcsncpy.c wcspbrk.c wcsrchr.c wcsspn.c \
	wcsstr.c wcstok.c wcswcs.c wcswidth.c wmemchr.c wmemcmp.c wmemcpy.c \
	wmemmove.c wmemset.c wcsdup.c wcscasecmp.c

# machine-dependent net sources
# ../arch/ARCH/Makefile.inc must include sources for:
#	bcmp() bcopy() bzero() ffs() index() memchr() memcmp() memset()
#	memcpy() memmove() memset() rindex() strcat() strchr()
#	strcmp() strcpy() strcspn() strlen() strlcat() strlcpy()
#	strncat() strncmp() strncpy() strpbrk() strrchr() strsep()
#	strspn() strstr() swab()

.include "${LIBCSRCDIR}/arch/${MACHINE_CPU}/string/Makefile.inc"

MAN+=	bcmp.3 bcopy.3 bstring.3 bzero.3 ffs.3 memccpy.3 memchr.3 \
	memcmp.3 memcpy.3 memmem.3 memmove.3 memset.3 stpcpy.3 strcasecmp.3 \
	strcat.3 strchr.3 strcmp.3 strcoll.3 strcpy.3 strcspn.3 strdup.3 \
	strerror.3 string.3 strlen.3 strmode.3 strncat.3 strncpy.3 strpbrk.3 \
	strrchr.3 strsep.3 strsignal.3 strspn.3 strstr.3 strtok.3 strxfrm.3 \
	swab.3 strlcpy.3 timingsafe_bcmp.3 \
	wcscasecmp.3 wcscat.3 wcschr.3 wcscmp.3 wcscpy.3 \
	wcscspn.3 wcsdup.3 wcslcpy.3 wcslen.3 wcspbrk.3 wcsrchr.3 wcsspn.3 \
	wcsstr.3 wcstok.3 wcswidth.3 wmemchr.3 wmemcmp.3 wmemcpy.3 wmemmove.3 \
	wmemset.3

MLINKS+=bzero.3 explicit_bzero.3
MLINKS+=memchr.3 memrchr.3
MLINKS+=stpcpy.3 stpncpy.3
MLINKS+=strchr.3 index.3
MLINKS+=strrchr.3 rindex.3
MLINKS+=strcasecmp.3 strncasecmp.3
MLINKS+=strcmp.3 strncmp.3
MLINKS+=strdup.3 strndup.3
MLINKS+=strlcpy.3 strlcat.3
MLINKS+=strlen.3 strnlen.3
MLINKS+=strstr.3 strcasestr.3
MLINKS+=strtok.3 strtok_r.3
MLINKS+=strerror.3 strerror_r.3
MLINKS+=timingsafe_bcmp.3 timingsafe_memcmp.3
MLINKS+=wcscasecmp.3 wcsncasecmp.3
MLINKS+=wcscat.3 wcsncat.3
MLINKS+=wcscmp.3 wcsncmp.3
MLINKS+=wcscpy.3 wcsncpy.3
MLINKS+=wcslcpy.3 wcslcat.3
