# $LynxId: makefile.dsl,v 1.19 2008/06/30 23:53:42 tom Exp $

OBJS= UCdomap.o UCAux.o UCAuto.o \
LYClean.o LYShowInfo.o LYEdit.o LYStrings.o \
LYMail.o HTAlert.o GridText.o LYGetFile.o \
LYMain.o LYMainLoop.o LYCurses.o LYBookmark.o LYmktime.o LYUtils.o \
LYOptions.o LYReadCFG.o LYSearch.o LYHistory.o LYSession.o \
LYForms.o LYPrint.o LYrcFile.o LYDownload.o LYNews.o LYKeymap.o \
HTML.o HTFWriter.o HTInit.o DefaultStyle.o LYLocal.o LYUpload.o \
LYLeaks.o LYexit.o LYJump.o LYList.o LYCgi.o LYTraversal.o \
LYEditmap.o LYCharSets.o LYCharUtils.o LYMap.o LYCookie.o LYExtern.o \
LYStyle.o LYHash.o LYPrettySrc.o TRSTable.o parsdate.o

CFLAGS= -O2 $(MCFLAGS) $(INTLFLAGS) -I. -I.. $(SLANGINC)

# comment this line to suppress DIRED support
DIRED_DEFS = \
 -DDIRED_SUPPORT \
 -DOK_UUDECODE \
 -DOK_TAR \
 -DOK_GZIP \
 -DOK_ZIP \
 -DOK_OVERRIDE

CC = gcc

MCFLAGS = \
 $(DIRED_DEFS) \
 -DACCESS_AUTH \
 -DDISP_PARTIAL \
 -DDJGPP_KEYHANDLER \
 -DDOSPATH \
 -DHAVE_POPEN \
 -DNOUSERS \
 -DNO_CUSERID \
 -DNO_TTYTYPE \
 -DNO_UTMP \
 -DUSE_EXTERNALS \
 -DUSE_PRETTYSRC \
 -DUSE_SLANG \
 -DUSE_SOURCE_CACHE \
 -DUSE_ZLIB \
 $(SSLFLAGS) \
 $(SSLINC) \
 -I./chrtrans \
 -I../WWW/Library/Implementation \
 -I/dev/env/DJDIR/watt32/inc

WWWLIB = \
 ../WWW/Library/djgpp/libwww.a \
 /dev/env/DJDIR/watt32/lib/libwatt.a

LIBS= $(SLANGLIB) -lslang $(SSLLIB) -lz $(INTLLIBS)

# Uncomment the following to enable Internationalization.
#INTLFLAGS = -DHAVE_GETTEXT -DHAVE_LIBINTL_H
#INTLLIBS= -lintl -liconv

# Uncomment the following to enable SSL.
#SSLFLAGS = -DUSE_SSL
#SSLLIB = -lssl -lcrypto
#SSLINC = -I/dev/env/DJDIR/include/openssl

all: lynx.exe

lynx.exe:   message $(OBJS) $(WWWLIB)
	@echo "Linking and creating Lynx executable"
	$(CC) $(CFLAGS) -o lynx.exe  $(OBJS) $(WWWLIB) $(LIBS)
	@echo "Welcome to Lynx!"

message:
	@echo "Compiling Lynx sources"

dbg:	$(OBJS) $(WWWLIB)
	@echo "Making Lynx code"
	$(CC) -g $(OBJS) $(CFLAGS) $(WWWLIB) $(LIBS)

lint:
	lint *.c  > ../lint.out

clean:
	rm -f lynx.exe core *.[ob]

DefaultStyle.o:	../userdefs.h
HTFWriter.o:	../userdefs.h
LYBookmark.o:	../userdefs.h
LYCharSets.o:	../userdefs.h
LYCharUtils.o:	../userdefs.h
LYCookie.o:	../userdefs.h
LYDownload.o:	../userdefs.h
LYEditmap.o:	../userdefs.h
LYExtern.o:	../userdefs.h
LYGetFile.o:	../userdefs.h
LYHistory.o:	../userdefs.h
LYKeymap.o:	../userdefs.h
LYMain.o:	../userdefs.h
LYMainLoop.o:	../userdefs.h
LYOptions.o:	../userdefs.h
LYReadCFG.o:	../userdefs.h
LYShowInfo.o:	../userdefs.h
LYStrings.o:	../userdefs.h
LYTraversal.o:	../userdefs.h
LYUtils.o:	../userdefs.h
LYmktime.o:	../userdefs.h
parsdate.o:	../userdefs.h
