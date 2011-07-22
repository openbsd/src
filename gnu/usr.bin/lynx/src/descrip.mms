! $LynxId: descrip.mms,v 1.12 2008/06/30 23:50:22 tom Exp $
!
!       Make LYNX hypertext browser under VMS
!       =====================================
!
!	NOTE:  Use [.SRC.CHRTRANS]BUILD-CHRTRANS.COM to create the
!	       chrtrans header files before using this descrip.mms.
!
! History:
!  1/1/93  creation at KU (Lou montulli@ukanaix.cc.ukans.edu).
!  4/12/93 (seb@lns61.tn.cornell.edu)
!           modified to support either UCX or MULTINET
!  12/2/93 modified to support Lynx rewrite
!  12/13/93 (macrides@sci.wfeb.edu)
!	     Added conditional compilations for VAXC vs. DECC
!	     (dependencies not yet specified; this is just a
!	      "starter", should anyone want to do it well).
!  10/31/94 RLD Updated for Lynx v2.3.4-VMS, supporting OpenCMU
!               and TCPWare
!  11/11/94 RLD Updated for Lynx v2.3.5-VMS
!  11/18/94 FM Updated for SOCKETSHR/NETLIB
!  12/07/94 FM Updated for DECC/VAX, VAXC/VAX and DECC/AXP
!  05/03/95 FM Include /NoMember for DECC (not the default on AXP, and
!		the code assumes byte alignment).
!  06/14/95 FM Added LYList.
!  07/26/95 FM Separated transport (TOPT) and compiler (COPT) option files.
!  07/29/95 FM Added support for GNUC.
!  02/29/96 FM Added LYMap.
!  06/28/97 FM Added UCAuto, UCAux, and UCdomap.
!  15 Sep 06 (TD)	Cleanup...
!
! Instructions:
!       Use the correct command line for your TCP/IP implementation:
!
!	$ MMS                                   for VAXC - MultiNet
!	$ MMS /Macro = (MULTINET=1)		for VAXC - MultiNet
!	$ MMS /Macro = (WIN_TCP=1)              for VAXC - Wollongong TCP/IP
!	$ MMS /Macro = (UCX=1)			for VAXC - UCX
!	$ MMS /Macro = (CMU_TCP=1)		for VAXC - OpenCMU TCP/IP
!	$ MMS /Macro = (SOCKETSHR_TCP=1)	for VAXC - SOCKETSHR/NETLIB
!	$ MMS /Macro = (TCPWARE=1)		for VAXC - TCPWare TCP/IP
!	$ MMS /Macro = (DECNET=1)		for VAXC - socket emulation over DECnet
!
!	$ MMS /Macro = (MULTINET=1, DEC_C=1)	for DECC - MultiNet
!	$ MMS /Macro = (WIN_TCP=1, DEC_C=1)	for DECC - Wollongong TCP/IP
!	$ MMS /Macro = (UCX=1, DEC_C=1)		for DECC - UCX
!	$ MMS /Macro = (CMU_TCP=1, DEC_C=1)	for DECC - OpenCMU TCP/IP
!	$ MMS /Macro = (SOCKETSHR_TCP=1,DEC_C=1) for DECC - SOCKETSHR/NETLIB
!	$ MMS /Macro = (TCPWARE=1, DEC_C=1)	for DECC - OpenCMU TCP/IP
!	$ MMS /Macro = (DECNET=1, DEC_C=1)	for DECC - socket emulation over DECnet
!
!	$ MMS /Macro = (MULTINET=1, GNU_C=1)	for GNUC - MultiNet
!	$ MMS /Macro = (WIN_TCP=1, GNU_C=1)	for GNUC - Wollongong TCP/IP
!	$ MMS /Macro = (UCX=1, GNU_C=1)		for GNUC - UCX
!	$ MMS /Macro = (CMU_TCP=1, GNU_C=1)	for GNUC - OpenCMU TCP/IP
!	$ MMS /Macro = (SOCKETSHR_TCP=1,GNU_C=1) for GNUC - SOCKETSHR/NETLIB
!	$ MMS /Macro = (TCPWARE=1, GNU_C=1)	for GNUC - OpenCMU TCP/IP
!	$ MMS /Macro = (DECNET=1, GNU_C=1)	for GNUC - socket emulation over DECnet

OBJS =	DefaultStyle.obj, GridText.obj, HTAlert.obj, HTFWriter.obj, -
	HTInit.obj, HTML.obj, LYBookmark.obj, LYCgi.obj, LYCharSets.obj, -
	LYCharUtils.obj, LYClean.obj, LYCookie.obj, LYCurses.obj, -
	LYDownload.obj, LYEdit.obj, LYEditmap.obj, LYForms.obj, -
	LYGetFile.obj, LYHistory.obj, LYJump.obj, LYKeymap.obj, -
	LYLeaks.obj, LYList.obj, LYMail.obj, LYMain.obj, LYMainLoop.obj, -
	LYMap.obj, LYNews.obj, LYOptions.obj, LYPrint.obj, LYReadCFG.obj, -
	LYSearch.obj, LYShowInfo.obj, LYStrings.obj, LYTraversal.obj, -
	LYUpload.obj, LYUtils.obj, LYexit.obj, LYrcFile.obj, TRSTable.obj, -
	LYmktime.obj, UCAuto.obj, UCAux.obj, UCdomap.obj, parsdate.obj

.ifdef SLANG
SCREEN_DEF = USE_SLANG
SCREEN_INC = , SLANG_INC 
SCREEN_LIB = , SLANG_LIB:slang.olb/lib
.else
.ifdef DEC_C
SCREEN_DEF = __VMS_CURSES
.endif
.endif

.ifdef DEC_C
COMPILER = DECC
MODEL_DEF = _DECC_V4_SOURCE
.else
MODEL_DEF =
.ifdef GNU_C
COMPILER = GNUC
CC = gcc
.else
COMPILER = VAXC
.endif
.endif

.ifdef WIN_TCP
NETWORK_DEF = WIN_TCP
NETWORK_OPT = WIN_TCP
.else
.ifdef CMU_TCP
NETWORK_DEF = CMU_TCP
NETWORK_OPT = CMU_TCP
.else
.ifdef SOCKETSHR_TCP
NETWORK_DEF = SOCKETSHR_TCP
NETWORK_OPT = SOCKETSHR_TCP
.else
.ifdef UCX
NETWORK_DEF = UCX
.ifdef DEC_C
NETWORK_OPT = UCXSHR
.else
NETWORK_OPT = UCXOLB
.endif
.else
.ifdef TCPWARE
NETWORK_DEF = TCPWARE,UCX
.ifdef DEC_C
NETWORK_OPT = TCPWARESHR
.else
NETWORK_OPT = TCPWAREOLB
.endif
.else
.ifdef DECnet
NETWORK_DEF = DECNET
NETWORK_OPT = DECNET
.else !  Default to MultiNet
NETWORK_DEF = MULTINET,__SOCKET_TYPEDEFS
NETWORK_OPT = MULTINET
.endif !  DECnet
.endif !  TCPWARE
.endif !  UCX
.endif !  SOCKETSHR_TCP
.endif !  CMU_TCP
.endif !  WIN_TCP

COMPILER_DEF = $(MODEL_DEF),$(NETWORK_DEF),$(SCREEN_DEF)

.ifdef DEC_C
MY_CFLAGS = /decc/Prefix=All/NoMember/Define=(ACCESS_AUTH,$(COMPILER_DEF))
.else
MY_CFLAGS = /Define = (ACCESS_AUTH, $(COMPILER_DEF))
.endif

.if "$(MMS_ARCHNAME)" .eq "IA64"
TOPT = 
COPT = 
.else
TOPT = ,sys$disk:[]$(NETWORK_OPT).opt/opt
COPT = ,sys$disk:[]$(COMPILER).opt/opt
.endif

WWWLIB = [-.WWW.Library.Implementation]WWWLib.olb
CFLAGS = $(MY_CFLAGS) $(CFLAGS)/Include=([], [-], [.chrtrans], [-.WWW.Library.Implementation]$(SCREEN_INC))


lynx :	lynx.exe
	@ Continue

HDRS = [.chrtrans]iso01_uni.h

lynx.exe :   $(HDRS) $(OBJS) $(WWWLIB)
	$(LINK) /Executable = Lynx.exe $(OBJS), $(WWWLIB)/lib $(SCREEN_LIB) $(TOPT) $(COPT)

$(HDRS) :
	set default [.chrtrans]
	@build-chrtrans
	set default [-]

clean :
	- Set Protection = (Owner:RWED) *.*;-1
	- Purge /NoLog /NoConfirm
	- Delete /NoConfirm /NoLog *.obj;*
	- Delete /NoConfirm /NoLog *.exe;*
