$ v0 = 0
$ v = f$verify(v0)
$! $LynxId: libmake.com,v 1.15 2011/05/23 23:58:48 tom Exp $
$!			LIBMAKE.COM
$!
$!   Command file to build the WWWLibrary on VMS systems.
$!
$!   11-Jul-2010	Ch. Gartmann
$!	add support for "MULTINETUCX"
$!   01-Jul-2007	T.Dickey
$!	add support for "TCPIP" (TCPIP Services)
$!   23-Oct-2004	T.Dickey
$!	cleanup, remove duplication, etc.
$!   08-Oct-1997	F.Macrides		macrides@sci.wfeb.edu
$!	Added comments and minor tweaks for convenient addition of
$!	compiler definitions and compiler and linker options.
$!   26-Jul-1995	F.Macrides		macrides@sci.wfeb.edu
$!	Adding support for GNUC.
$!   03-May-1995	F.Macrides		macrides@sci.wfeb.edu
$!	Include /nomember for compilations with DECC.  It's not the
$!	default on AXP and the code assumes byte alignment.
$!   07-Dec-1994	F.Macrides		macrides@sci.wfeb.edu
$!	Updated for DECC/VAX, VAXC/VAX and DECC/AXP
$!   03-NOV-1994	A.Harper		A.Harper@kcl.ac.uk
$!	Mods to support SOCKETSHR/NETLIB and add a /DEBUG/NOOPT option
$!   02-Jun-1994	F.Macrides		macrides@sci.wfeb.edu
$!	Mods to support TCPWare (To use non-blocking connects, you need
$!	the DRIVERS_V405B.INC patch from FTP.PROCESS.COM for TCPware for
$!	OpenVMS Version 4.0-5, or a higher version of TCPWare, which will
$!	have that bug in the TCPDRIVER fixed.  Otherwise, add NO_IOCTL to
$!	the /define=(...) list.)
$!   20-May-1994	Andy Harper		A.Harper@bay.cc.kcl.ac.uk
$!	Added support for the CMU TCP/IP transport
$!   13-Dec-1993	F.Macrides		macrides@sci.wfeb.edu
$!	Mods for conditional compilations with VAXC versus DECC
$!   10-Dec-1993	F.Macrides		macrides@sci.wfeb.edu
$!	Initial version, for WWWLibrary v2.14 with Lynx v2.1
$!
$ ON CONTROL_Y THEN GOTO CLEANUP
$ ON ERROR THEN GOTO CLEANUP
$!
$!	Compiler definitions can be added here as a comma separated
$!	list with a lead comma, e.g., ",HAVE_FOO_H,DO_BLAH".  They
$!	will apply only to the libwww-FM modules. - FM
$!
$ extra_defs = ",ACCESS_AUTH"
$!
$!	Include-paths can be added here as a comma separated
$!	list with a lead comma, e.g., ",foo".
$!
$ extra_incs = ""
$!
$ extra_libs = ""
$!
$!	If no TCP/IP agent is specified (as the first argument),
$!	prompt for a number from the list.   Note that the agent
$!	must be the first argument if the debugger mode is to be
$!	set via a second argument (see below). - FM
$!
$ agent = 0
$ IF P1 .EQS. ""
$ THEN
$ 	write sys$output "Acceptable TCP/IP agents are"
$ 	write sys$output " [1] MultiNet (default)"
$ 	write sys$output " [2] UCX"
$ 	write sys$output " [3] WIN_TCP"
$	write sys$output " [4] CMU_TCP"
$	write sys$output " [5] SOCKETSHR_TCP"
$	write sys$output " [6] TCPWARE"
$ 	write sys$output " [7] DECNET"
$ 	write sys$output " [8] TCPIP"
$ 	write sys$output " [9] Multinet UCX emulation"
$ 	read sys$command/prompt="Agent [1,2,3,4,5,6,7,8,9] (RETURN = [1]) " agent
$ ENDIF
$ if agent .eq. 1 .or. agent .eqs. "" .or. p1 .eqs. "MULTINET" then -
    transport = "MULTINET"
$ if agent .eq. 2 .or. p1 .eqs. "UCX"           then transport = "UCX"
$ if agent .eq. 3 .or. p1 .eqs. "WIN_TCP"       then transport = "WIN_TCP"
$ if agent .eq. 4 .or. p1 .eqs. "CMU_TCP"       then transport = "CMU_TCP"
$ if agent .eq. 5 .or. p1 .eqs. "SOCKETSHR_TCP" then transport = "SOCKETSHR_TCP"
$ if agent .eq. 6 .or. p1 .eqs. "TCPWARE"       then transport = "TCPWARE"
$ if agent .eq. 7 .or. p1 .eqs. "DECNET"        then transport = "DECNET"
$ if agent .eq. 8 .or. p1 .eqs. "TCPIP"         then transport = "TCPIP"
$ IF agent .EQ. 9 .OR. P1 .EQS. "MULTINETUCX"
$    THEN
$    transport = "UCX"
$    extra_defs = extra_defs + ",MUCX"
$ ENDIF
$!
$ if transport .eqs. "SOCKETSHR_TCP" then extra_defs = extra_defs + ",_DECC_V4_SOURCE"
$ if transport .eqs. "TCPIP"         then extra_defs = extra_defs + ",TCPIP_SERVICES"
$ if transport .eqs. "TCPWARE"       then extra_defs = extra_defs + ",UCX"
$!
$  if option .eqs. "TCPIP"
$  then
$     if f$trnlnm("TCPIP$IPC_SHR") .eqs. "" then define TCPIP$IPC_SHR SYS$LIBRARY:TCPIP$IPC_SHR
$  endif
$!
$ if P2 .nes. ""
$ then
$   count_parm = 0
$ parse_p2:
$   value_parm = f$element('count_parm, ",", "''p2'")
$   if value_parm .nes. ","
$   then
$      if value_parm .eqs. "BZLIB"
$      then
$         write sys$output "** adding BZlib to build."
$         extra_defs = extra_defs + ",USE_BZLIB"
$         extra_incs = extra_incs + "," + BZLIB_INC
$         extra_libs = extra_libs + "," + BZLIB_LIB + "libbz2/LIB"
$      endif
$      if value_parm .eqs. "SLANG"
$      then
$         write sys$output "** adding SLang to build."
$         extra_defs = extra_defs + ",USE_SLANG"
$         extra_incs = extra_incs + "," + SLANG_INC
$         extra_libs = extra_libs + "," + SLANG_LIB + "slang.olb/lib"
$      endif
$      if value_parm .eqs. "SSL"
$      then
$         write sys$output "** adding SSL to build."
$         IF F$TYPE( ssl_lib ) .EQS. "" THEN ssl_lib = F$TRNLNM("SSLLIB")
$         IF F$TYPE( ssl_inc ) .EQS. "" THEN ssl_inc = F$TRNLNM("SSLINCLUDE")
$         extra_defs = extra_defs + ",USE_SSL,USE_OPENSSL_INCL"
$         extra_libs = extra_libs + "," + SSL_LIB + "libssl/LIB," + SSL_LIB + "libcrypto/LIB"
$!
$!	The "#include <openssl/ssl.h>" requires a logical variable "openssl".
$!
$         define/nolog openssl 'SSL_INC
$      endif
$      if value_parm .eqs. "ZLIB"
$      then
$         write sys$output "** adding Zlib to build."
$         extra_defs = extra_defs + ",USE_ZLIB"
$         extra_incs = extra_incs + "," + ZLIB_INC
$         extra_libs = extra_libs + "," + ZLIB_LIB + "libz/LIB"
$      endif
$      count_parm = count_parm + 1
$      goto parse_p2
$   endif
$ endif
$!
$!	Compiler options can be specified here.  If there was
$!	a second argument (with any value), then debugger mode
$!	with no optimization will be specified as well. - FM
$!
$ cc_opts = ""
$ if p3 .nes. "" then cc_opts = cc_opts + "/DEBUG/NOOPT"
$!
$ all_defs = transport + extra_defs
$ all_incs = "[-.Implementation],[---.src],[---.src.chrtrans],[---]" + extra_incs
$!
$ IF f$getsyi("ARCH_NAME") .eqs. "Alpha" .or. -
     f$getsyi("ARCH_NAME") .eqs. "IA64" .or. -
     f$trnlnm("VAXCMSG") .eqs. "DECC$MSG" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/DECC" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/VAXC"
$ THEN
$! DECC:
$  If transport .eqs. "UCX" .or. transport .eqs. "TCPWARE"
$  Then
$    cc := cc/decc/prefix=all /nomember 'cc_opts'-
	  /warning=(disable=implicitfunc)-
	  /DEFINE=('all_defs) -
	  /INCLUDE=('all_incs)
$  Else
$    if transport .eqs. "MULTINET" then -
	extra_defs = extra_defs + ",_DECC_V4_SOURCE,__SOCKET_TYPEDEFS"
$    cc := cc/decc/prefix=all /nomember 'cc_opts'-
	  /warning=(disable=implicitfunc)-
	  /DEFINE=('all_defs) -
	  /INCLUDE=('all_incs)
$  EndIf
$ ELSE
$  IF f$search("gnu_cc:[000000]gcclib.olb") .nes. ""
$  THEN
$! GNUC:
$    cc := gcc/DEFINE=('all_defs) 'cc_opts' /INCLUDE=('all_incs)
$  ELSE
$! VAXC:
$    cc := cc/DEFINE=('all_defs) 'cc_opts' /INCLUDE=('all_incs)
$  ENDIF
$ ENDIF
$ v1 = f$verify(1)
$ show sym cc
$ cc [-.Implementation]HTString.c
$ cc [-.Implementation]HTParse.c
$ cc [-.Implementation]HTAccess.c
$ cc [-.Implementation]HTTP.c
$ cc [-.Implementation]HTFile.c
$ cc [-.Implementation]HTBTree.c
$ cc [-.Implementation]HTFTP.c
$ cc [-.Implementation]HTTCP.c
$ cc [-.Implementation]SGML.c
$ cc [-.Implementation]HTMLDTD.c
$ cc [-.Implementation]HTChunk.c
$ cc [-.Implementation]HTPlain.c
$ cc [-.Implementation]HTMLGen.c
$ cc [-.Implementation]HTAtom.c
$ cc [-.Implementation]HTAnchor.c
$ cc [-.Implementation]HTStyle.c
$ cc [-.Implementation]HTList.c
$ cc [-.Implementation]HTRules.c
$ cc [-.Implementation]HTFormat.c
$ cc [-.Implementation]HTMIME.c
$ cc [-.Implementation]HTNews.c
$ cc [-.Implementation]HTGopher.c
$ cc [-.Implementation]HTTelnet.c
$ cc [-.Implementation]HTFinger.c
$ cc [-.Implementation]HTWSRC.c
$ cc [-.Implementation]HTAAUtil.c
$ cc [-.Implementation]HTAABrow.c
$ cc [-.Implementation]HTGroup.c
$ cc [-.Implementation]HTAAProt.c
$ cc [-.Implementation]HTAssoc.c
$ cc [-.Implementation]HTLex.c
$ cc [-.Implementation]HTUU.c
$ cc [-.Implementation]HTVMSUtils.c
$ cc [-.Implementation]HTWAIS.c
$ cc [-.Implementation]HTVMS_WaisUI.c
$ cc [-.Implementation]HTVMS_WaisProt.c
$!
$ result = "[-.Implementation]WWWLib_''transport'.olb"
$ If f$search("''result'") .eqs. "" Then -
    LIBRARY/Create 'result
$ LIBRARY/Replace 'result *.obj
$ Delete/nolog/noconf *.obj;*
$!
$ v1 = f$verify(v0)
$ CLEANUP:
$    v1 = f$verify(v)
$exit
