!	Make LYNX hypertext browser under VMS
!       =====================================
!
!	NOTE:  Use [.SRC.CHRTRANS]BUILD-CHRTRANS.COM to create the
!	       chrtrans header files before using this descrip.mms.
!
! History:
!  01/01/93 creation at KU (Lou montulli@ukanaix.cc.ukans.edu). 
!  04/12/93 (seb@lns61.tn.cornell.edu)
!            modified to support either UCX or MULTINET
!  12/13/93 (macrides@sci.wfeb.edu)
!	     Added conditional compilations for VAXC vs. DECC
!	     (dependencies not yet specified; this is just a
!	      "starter", should anyone want to do it well).
!  10/26/94 (dyson@IowaSP.Physics.UIowa.EDU) RLD
!            Updated for AXP/VMS v6.1 and VAX/VMS v5.5-1
!  12/07/94 (macrides@sci.wfeb.edu)
!	     Updated for DECC/VAX, VAXC/VAX and DECC/AXP
!  02/17/95 (macrides@sci.wfeb.edu)
!	     Updated for v2.3-FM
!  03/23/95 (macrides@sci.wfeb.edu)
!	     Replaced references to v2.3.8 or v2.3.9 with v2.3-FM to
!	     avoid any confusion with official releases at UKans.
!  07/29/95 (macrides@sci.wfeb.edu)
!	     Added support for GNUC.
!
! Instructions:
!	Use the correct command line for your TCP/IP implementation:
!
!	$ MMS /Macro = (MULTINET=1)		for VAXC - MultiNet
!	$ MMS /Macro = (WIN_TCP=1)		for VAXC - Wollongong TCP/IP
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
!	$ MMS /Macro = (TCPWARE=1, DEC_C=1)	for DECC - TCPWare TCP/IP
!	$ MMS /Macro = (DECNET=1, DEC_C=1)	for DECC - socket emulation over DECnet
!
!	$ MMS /Macro = (MULTINET=1, GNU_C=1)	for GNUC - MultiNet
!	$ MMS /Macro = (WIN_TCP=1, GNU_C=1)	for GNUC - Wollongong TCP/IP
!	$ MMS /Macro = (UCX=1, GNU_C=1)		for GNUC - UCX
!	$ MMS /Macro = (CMU_TCP=1, GNU_C=1)	for GNUC - OpenCMU TCP/IP
!	$ MMS /Macro = (SOCKETSHR_TCP=1,GNU_C=1) for GNUC - SOCKETSHR/NETLIB
!	$ MMS /Macro = (TCPWARE=1, GNU_C=1)	for GNUC - TCPWare TCP/IP
!	$ MMS /Macro = (DECNET=1, GNU_C=1)	for GNUC - socket emulation over DECnet

.ifdef MULTINET
TCPM = MULTINET
.endif

.ifdef WIN_TCP
TCPM = WIN_TCP
.endif

.ifdef UCX
TCPM = UCX
.endif

.ifdef CMU_TCP
TCPM = CMU_TCP
.endif

.ifdef SOCKETSHR_TCP
TCPM = SOCKETSHR_TCP
.endif

.ifdef TCPWARE
TCPM = TCPWARE
.endif

.ifdef DECNET
TCPM = DECNET
.endif

.ifdef TCPM
.else
TCPM = MULTINET	!Default to MultiNet
.endif

.ifdef GNU_C
CC = gcc
.endif

lynx :	lynx.exe
	! Finished Building LYNX for VMS!!!

lynx.exe : library exe
	@ Continue

library :
	Set Default [.www.library.implementation]
.ifdef DEC_C
	$(MMS) /Description = [-.VMS]DESCRIP.MMS /Macro = ($(TCPM)=1, DEC_C=1) Library
.else
.ifdef GNU_C
	$(MMS) /Description = [-.VMS]DESCRIP.MMS /Macro = ($(TCPM)=1, GNU_C=1) Library
.else
	$(MMS) /Description = [-.VMS]DESCRIP.MMS /Macro = ($(TCPM)=1) Library
.endif
.endif
	Set Default [---]

exe :
	Set Default [.src]
.ifdef DEC_C
	$(MMS) /Macro = ($(TCPM)=1, DEC_C=1) Lynx
.else
.ifdef GNU_C
	$(MMS) /Macro = ($(TCPM)=1, GNU_C=1) Lynx
.else
	$(MMS) /Macro = ($(TCPM)=1) Lynx
.endif
.endif
	Copy /NoLog /NoConfirm lynx.exe [-]
	Set Protection = (Owner:RWE, World:RE) [-]lynx.exe
	Set Default [-]

clean :
	Set Default [.www.library.implementation]
	$(MMS) /Description = [-.VMS]DESCRIP.MMS clean
	Set Default [---]
	Set Default [.src]
	$(MMS) clean
	Set Default [-]
	- Purge /NoLog /NoConfirm
