$ v = 'f$verify(0)'
$!			VMSBUILD.COM for lpansi.c
$!
$!   Command file to build LPANSI.EXE on VMS systems.
$!
$!   19-Dec-1996	F.Macrides		macrides@sci.wfeb.edu
$!	Initial version, for Lynx.
$!
$ ON CONTROL_Y THEN GOTO CLEANUP
$ ON ERROR THEN GOTO CLEANUP
$ proc = f$environment("PROCEDURE")
$ where = f$parse(proc,,,"DEVICE") + f$parse(proc,,,"DIRECTORY")
$ set default 'where'
$ write sys$output "Default directory:"
$ show default
$ write sys$output ""
$!
$!
$ if p1 .nes. ""
$   then
$      cc_opts = "/DEBUG/NOOPT"
$      link_opts = "/DEBUG"
$   else
$      cc_opts = ""
$      link_opts = ""
$ endif
$!
$ v1 = f$verify(1)
$!
$!	Build LPANSI.EXE
$!
$ v1 = 'f$verify(0)'
$ IF f$trnlnm("VAXCMSG") .eqs. "DECC$MSG" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/DECC" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/VAXC"
$ THEN
$  compiler := "DECC"
$  v1 = f$verify(1)
$! Compile with DECC:
$  cc := cc/decc/prefix=all 'cc_opts'
$  v1 = 'f$verify(0)'
$ ELSE
$  IF f$search("gnu_cc:[000000]gcclib.olb") .nes. ""
$  THEN
$   compiler := "GNUC"
$   v1 = f$verify(1)
$! Compile with GNUC:
$   cc := gcc 'cc_opts'
$   v1 = 'f$verify(0)'
$  ELSE
$   compiler := "VAXC"
$   v1 = f$verify(1)
$! Compile with VAXC:
$   cc := cc 'cc_opts'
$   v1 = 'f$verify(0)'
$  ENDIF
$ ENDIF
$ v1 = f$verify(1)
$!
$ cc lpansi
$!
$!	Link the objects and libaries.
$!
$ link/exe=lpansi.exe 'link_opts' -
lpansi.obj, -
sys$disk:[--.src]'compiler'.opt/opt
$ v1 = 'f$verify(0)'
$ CLEANUP:
$    v1 = 'f$verify(0)'
$    write sys$output "Default directory:"
$    show default
$    v1 = f$verify(v)
$ exit
