$ v = 'f$verify(0)'
$!			LIBMAKE.COM
$!
$!   Command file to build the WWWLibrary on VMS systems.
$!
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
$ extra = ""
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
$ 	read sys$command/prompt="Agent [1,2,3,4,5,6,7] (RETURN = [1]) " agent
$ ENDIF
$ if agent .eq. 1 .or. agent .eqs. "" .or. p1 .eqs. "MULTINET" then -
    transport = "MULTINET"
$ if agent .eq. 2 .or. p1 .eqs. "UCX" then transport = "UCX"
$ if agent .eq. 3 .or. p1 .eqs. "WIN_TCP" then transport = "WIN_TCP"
$ if agent .eq. 4 .or. p1 .eqs. "CMU_TCP" then transport = "CMU_TCP"
$ if agent .eq. 5 .or. p1 .eqs. "SOCKETSHR_TCP" then transport = "SOCKETSHR_TCP"
$ if agent .eq. 6 .or. p1 .eqs. "TCPWARE" then transport = "TCPWARE"
$ if agent .eq. 7 .or. p1 .eqs. "DECNET" then transport = "DECNET"
$!
$ if transport .eqs. "SOCKETSHR_TCP" then extra = extra + ",_DECC_V4_SOURCE"
$ if transport .eqs. "TCPWARE" then extra = extra + ",UCX"
$!
$ if p2 .nes. ""
$ then
$   ssl_arg = "openssl"
$   extra = extra + ",USE_SSL,USE_OPENSSL_INCL"
$ else
$   ssl_arg = ""
$ endif
$!
$!	Compiler options can be specified here.  If there was
$!	a second argument (with any value), then debugger mode
$!	with no optimization will be specified as well. - FM
$!
$ cc_opts = ""
$ if p3 .nes. "" then cc_opts = cc_opts + "/DEBUG/NOOPT"
$!
$ IF f$trnlnm("VAXCMSG") .eqs. "DECC$MSG" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/DECC" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/VAXC"
$ THEN
$  v1 = f$verify(1)
$! DECC:
$  v1 = 'f$verify(0)'
$  If transport .eqs. "UCX" .or. transport .eqs. "TCPWARE"
$  Then
$  v1 = f$verify(1)
$!
$ cc/decc/prefix=all /nomember 'cc_opts'-
    /warning=(disable=implicitfunc)-
    /DEFINE=(ACCESS_AUTH,'transport''extra',VC="""2.14""")-
    /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---]) -
    [-.Implementation]HTString.c
$!
$ cc := cc/decc/prefix=all /nomember 'cc_opts'-
	  /warning=(disable=implicitfunc)-
	  /DEFINE=(ACCESS_AUTH,'transport''extra')-
	  /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---])
$!
$  v1 = 'f$verify(0)'
$  Else
$  if transport .eqs. "MULTINET" then -
	extra = extra + ",_DECC_V4_SOURCE,__SOCKET_TYPEDEFS"
$  v1 = f$verify(1)
$!
$ cc/decc/prefix=all /nomember 'cc_opts'-
    /warning=(disable=implicitfunc)-
    /DEFINE=(ACCESS_AUTH,'transport''extra',VC="""2.14""")-
    /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---]) -
    [-.Implementation]HTString.c
$!
$ cc := cc/decc/prefix=all /nomember 'cc_opts'-
	  /warning=(disable=implicitfunc)-
	  /DEFINE=(ACCESS_AUTH,'transport''extra')-
	  /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---])
$!
$  v1 = 'f$verify(0)'
$  EndIf
$ ELSE
$  IF f$search("gnu_cc:[000000]gcclib.olb") .nes. ""
$  THEN
$   v1 = f$verify(1)
$! GNUC:
$!
$   gcc/DEFINE=(ACCESS_AUTH,'transport''extra',VC="""2.14""") 'cc_opts'-
       /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---]) -
       [-.Implementation]HTString.c
$!
$   cc := gcc/DEFINE=(ACCESS_AUTH,'transport''extra') 'cc_opts'-
	     /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---])
$!
$   v1 = 'f$verify(0)'
$  ELSE
$   v1 = f$verify(1)
$! VAXC:
$!
$   cc/DEFINE=(ACCESS_AUTH,'transport''extra',VC="""2.14""") 'cc_opts'-
      /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---]) -
      [-.Implementation]HTString.c
$!
$   cc := cc/DEFINE=(ACCESS_AUTH,'transport''extra') 'cc_opts'-
	    /INCLUDE=([-.Implementation],[---.src],[---.src.chrtrans],[---])
$!
$   v1 = 'f$verify(0)'
$  ENDIF
$ ENDIF
$ v1 = f$verify(1)
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
$ If f$search("[-.Implementation]WWWLib_''transport'.olb") .eqs. "" Then -
    LIBRARY/Create [-.Implementation]WWWLib_'transport'.olb
$ LIBRARY/Replace [-.Implementation]WWWLib_'transport'.olb *.obj
$ Delete/nolog/noconf *.obj;*
$!
$ v1 = 'f$verify(0)'
$ CLEANUP:
$    v1 = f$verify(v)
$exit
