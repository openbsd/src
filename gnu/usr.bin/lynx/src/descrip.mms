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
	UCAuto.obj, UCAux.obj, UCdomap.obj

.ifdef WIN_TCP
TCP = WIN_TCP
TCPOPT = WIN_TCP
.ifdef DEC_C
CDEF = __VMS_CURSES
.endif
.endif

.ifdef CMU_TCP
TCP = CMU_TCP
TCPOPT = CMU_TCP
.ifdef DEC_C
CDEF = __VMS_CURSES
.endif
.endif

.ifdef SOCKETSHR_TCP
TCP = SOCKETSHR_TCP
TCPOPT = SOCKETSHR_TCP
.ifdef DEC_C
CDEF = _DECC_V4_SOURCE,__VMS_CURSES
.endif
.endif

.ifdef UCX
TCP = UCX
.ifdef DEC_C
TCPOPT = UCXSHR
CDEF = __VMS_CURSES
.else
TCPOPT = UCXOLB
.endif
.endif

.ifdef TCPWARE
TCP = TCPWARE
.ifdef DEC_C
TCTOPT= TCPWARESHR
CDEF = __VMS_CURSES
.else
TCTOPT= TCPWAREOLB
.endif
.endif

.ifdef MULTINET
TCP = MULTINET
TCPOPT = MULTINET
.ifdef DEC_C
CDEF = _DECC_V4_SOURCE,__SOCKET_TYPEDEFS,__VMS_CURSES
.endif
.endif

.ifdef DECnet
TCP = DECNET
TCPOPT = DECNET
.endif

.ifdef TCP
.else
TCP = MULTINET
TCPOPT = MULTINET
.ifdef DEC_C
CDEF = _DECC_V4_SOURCE,__SOCKET_TYPEDEFS,__VMS_CURSES
.endif
.endif

.ifdef DEC_C
COMPILER = DECC
.ifdef TCPWARE
TCPFLAGS = /decc/Prefix=All/NoMember/Define=(ACCESS_AUTH,$(TCP),UCX,$(CDEF))
.else
TCPFLAGS = /decc/Prefix=All/NoMember/Define=(ACCESS_AUTH,$(TCP),$(CDEF))
.endif
.else
.ifdef GNU_C
COMPILER = GNUC
CC = gcc
.else
COMPILER = VAXC
.endif
.ifdef TCPWARE
TCPFLAGS = /Define = (ACCESS_AUTH, $(TCP), UCX)
.else
TCPFLAGS = /Define = (ACCESS_AUTH, $(TCP))
.endif
.endif

TOPT = sys$disk:[]$(TCPOPT).opt
COPT = sys$disk:[]$(COMPILER).opt
WWWLIB = [-.WWW.Library.Implementation]WWWLib_$(TCP).olb
CFLAGS = $(TCPFLAGS) $(CFLAGS)/Include=([], [-], [.chrtrans], [-.WWW.Library.Implementation])


lynx :	lynx.exe
	@ Continue

HDRS = [.chrtrans]iso01_uni.h

lynx.exe :   $(HDRS) $(OBJS) $(WWWLIB)
	$(LINK) /Executable = Lynx.exe $(OBJS), $(WWWLIB)/lib, $(TOPT)/opt, $(COPT)/opt

$(HDRS) :
	set default [.chrtrans]
	@build-chrtrans
	set default [-]

clean :
	- Set Protection = (Owner:RWED) *.*;-1
	- Purge /NoLog /NoConfirm
	- Delete /NoConfirm /NoLog *.obj;*
	- Delete /NoConfirm /NoLog *.exe;*
