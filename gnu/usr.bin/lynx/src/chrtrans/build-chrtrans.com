$ v = 'f$verify(0)'
$!			BUILD-CHRTRANS.COM
$!
$!   Command file to build MAKEUCTB.EXE on VMS systems
$!   and then use it to create the chrtrans header files.
$!
$!   28-Jun-1997	F.Macrides		macrides@sci.wfeb.edu
$!	Initial version, for Lynx v2.7.1+fotemods
$!
$ ON CONTROL_Y THEN GOTO CLEANUP
$ ON ERROR THEN GOTO CLEANUP
$ CHRproc = f$environment("PROCEDURE")
$ CHRwhere = f$parse(CHRproc,,,"DEVICE") + f$parse(CHRproc,,,"DIRECTORY")
$!
$ if p1 .nes. ""
$   then
$      CHRcc_opts = "/DEBUG/NOOPT"
$      CHRlink_opts = "/DEBUG"
$   else
$      CHRcc_opts = ""
$      CHRlink_opts = ""
$ endif
$!
$ Compile_makeuctb:
$!================
$ v1 = f$verify(1)
$!
$!	Compile the Lynx [.SRC.CHRTRANS]makeuctb module.
$!
$  v1 = 'f$verify(0)'
$ IF f$trnlnm("VAXCMSG") .eqs. "DECC$MSG" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/DECC" .or. -
     f$trnlnm("DECC$CC_DEFAULT") .eqs. "/VAXC"
$ THEN
$  CHRcompiler := "DECC"
$  v1 = f$verify(1)
$! DECC:
$  cc := cc/decc/prefix=all /nomember 'CHRcc_opts'-
	   /INCLUDE=([],[-],[--],[--.WWW.Library.Implementation]) 
$  v1 = 'f$verify(0)'
$ ELSE
$  IF f$search("gnu_cc:[000000]gcclib.olb") .nes. ""
$  THEN
$   CHRcompiler := "GNUC"
$   v1 = f$verify(1)
$! GNUC:
$   cc := gcc 'CHRcc_opts'/INCLUDE=([],[-],[--],[--.WWW.Library.Implementation]) 
$   v1 = 'f$verify(0)'
$  ELSE
$   CHRcompiler := "VAXC"
$   v1 = f$verify(1)
$! VAXC:
$   cc := cc 'CHRcc_opts'/INCLUDE=([],[-],[--],[--.WWW.Library.Implementation]) 
$   v1 = 'f$verify(0)'
$  ENDIF
$ ENDIF
$!
$ v1 = f$verify(1)
$ cc makeuctb
$ v1 = 'f$verify(0)'
$!
$ Link_makeuctb:
$!=============
$ v1 = f$verify(1)
$!
$!	Link the Lynx [.SRC.CHRTRANS]makeuctb module.
$!
$ link/exe=makeuctb.exe'CHRlink_opts' makeuctb, -
sys$disk:[-]'CHRcompiler'.opt/opt
$ v1 = 'f$verify(0)'
$!
$ Create_headers:
$!==============
$ v1 = f$verify(1)
$!
$!	Create the Lynx [.SRC.CHRTRANS] header files.
$!
$ makeuctb := $'CHRwhere'makeuctb
$ makeuctb cp1250_uni.tbl
$ makeuctb cp1251_uni.tbl
$ makeuctb cp1252_uni.tbl
$ makeuctb cp1253_uni.tbl
$ makeuctb cp1255_uni.tbl
$ makeuctb cp1256_uni.tbl
$ makeuctb cp1257_uni.tbl
$ makeuctb cp437_uni.tbl
$ makeuctb cp737_uni.tbl
$ makeuctb cp775_uni.tbl
$ makeuctb cp850_uni.tbl
$ makeuctb cp852_uni.tbl
$ makeuctb cp862_uni.tbl
$ makeuctb cp864_uni.tbl
$ makeuctb cp866_uni.tbl
$ makeuctb cp866u_uni.tbl
$ makeuctb cp869_uni.tbl
$ makeuctb def7_uni.tbl
$ makeuctb dmcs_uni.tbl
$ makeuctb hp_uni.tbl
$ makeuctb iso01_uni.tbl
$ makeuctb iso02_uni.tbl
$ makeuctb iso03_uni.tbl
$ makeuctb iso04_uni.tbl
$ makeuctb iso05_uni.tbl
$ makeuctb iso06_uni.tbl
$ makeuctb iso07_uni.tbl
$ makeuctb iso08_uni.tbl
$ makeuctb iso09_uni.tbl
$ makeuctb iso10_uni.tbl
$ makeuctb iso15_uni.tbl
$ makeuctb koi8r_uni.tbl
$ makeuctb koi8u_uni.tbl
$ makeuctb mac_uni.tbl
$ makeuctb mnem_suni.tbl
$ makeuctb mnem2_suni.tbl
$ makeuctb mnem_suni.tbl
$ makeuctb next_uni.tbl
$ makeuctb pt154_uni.tbl
$ makeuctb rfc_suni.tbl
$ makeuctb utf8_uni.tbl
$ makeuctb viscii_uni.tbl
$ v1 = 'f$verify(0)'
$ exit
$!
$ CLEANUP:
$    v1 = 'f$verify(0)'
$    write sys$output "Default directory:"
$    show default
$    v1 = f$verify(v)
$ exit
