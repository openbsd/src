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
$ define/user sys$output 'CHRwhere'iso01_uni.h	!ISO Latin 1
$ makeuctb iso01_uni.tbl
$ define/user sys$output 'CHRwhere'cp850_uni.h	!DosLatin1 (cp850)
$ makeuctb cp850_uni.tbl
$ define/user sys$output 'CHRwhere'cp1252_uni.h	!WinLatin1 (cp1252)
$ makeuctb cp1252_uni.tbl
$ define/user sys$output 'CHRwhere'cp437_uni.h	!DosLatinUS (cp437)
$ makeuctb cp437_uni.tbl
$ define/user sys$output 'CHRwhere'hp_uni.h	!HP Roman8
$ makeuctb hp_uni.tbl
$ define/user sys$output 'CHRwhere'dmcs_uni.h	!DEC Multinational
$ makeuctb dmcs_uni.tbl
$ define/user sys$output 'CHRwhere'mac_uni.h	!Macintosh (8 bit)
$ makeuctb mac_uni.tbl
$ define/user sys$output 'CHRwhere'next_uni.h	!NeXT character set
$ makeuctb next_uni.tbl
$ define/user sys$output 'CHRwhere'viscii_uni.h	!Vietnamese (VISCII)
$ makeuctb viscii_uni.tbl
$ define/user sys$output 'CHRwhere'def7_uni.h	!7 bit approximations
$ makeuctb def7_uni.tbl
$ define/user sys$output 'CHRwhere'iso02_uni.h	!ISO Latin 2
$ makeuctb iso02_uni.tbl
$ define/user sys$output 'CHRwhere'cp852_uni.h	!DosLatin2 (cp852)
$ makeuctb cp852_uni.tbl
$ define/user sys$output 'CHRwhere'cp1250_uni.h	!WinLatin2 (cp1250)
$ makeuctb cp1250_uni.tbl
$ define/user sys$output 'CHRwhere'iso03_uni.h	!ISO Latin 3
$ makeuctb iso03_uni.tbl
$ define/user sys$output 'CHRwhere'iso04_uni.h	!ISO Latin 4
$ makeuctb iso04_uni.tbl
$ define/user sys$output 'CHRwhere'cp775_uni.h	!DosBlatRim (cp775)
$ makeuctb cp775_uni.tbl
$ define/user sys$output 'CHRwhere'cp1257_uni.h	!WinBlatRim (cp1257)
$ makeuctb cp1257_uni.tbl
$ define/user sys$output 'CHRwhere'iso05_uni.h	!ISO 8859-5 Cyrillic
$ makeuctb iso05_uni.tbl
$ define/user sys$output 'CHRwhere'cp866_uni.h	!DosCyrillic (cp866)
$ makeuctb cp866_uni.tbl
$ define/user sys$output 'CHRwhere'cp866u_uni.h	!Ukrainian Cyrillic
$ makeuctb cp866u_uni.tbl
$ define/user sys$output 'CHRwhere'cp1251_uni.h	!WinCyrillic (cp1251)
$ makeuctb cp1251_uni.tbl
$ define/user sys$output 'CHRwhere'koi8r_uni.h	!KOI8-R Cyrillic
$ makeuctb koi8r_uni.tbl
$ define/user sys$output 'CHRwhere'koi8u_uni.h	!KOI8-U Ukranian Cyrillic
$ makeuctb koi8u_uni.tbl
$ define/user sys$output 'CHRwhere'iso06_uni.h	!ISO 8859-6 Arabic
$ makeuctb iso06_uni.tbl
$ define/user sys$output 'CHRwhere'cp864_uni.h	!DosArabic (cp864)
$ makeuctb cp864_uni.tbl
$ define/user sys$output 'CHRwhere'cp1256_uni.h	!WinArabic (cp1256)
$ makeuctb cp1256_uni.tbl
$ define/user sys$output 'CHRwhere'iso07_uni.h	!ISO 8859-7 Greek
$ makeuctb iso07_uni.tbl
$ define/user sys$output 'CHRwhere'cp737_uni.h	!DosGreek (cp737)
$ makeuctb cp737_uni.tbl
$ define/user sys$output 'CHRwhere'cp869_uni.h	!DosGreek2 (cp869)
$ makeuctb cp869_uni.tbl
$ define/user sys$output 'CHRwhere'cp1253_uni.h	!WinGreek (cp1253)
$ makeuctb cp1253_uni.tbl
$ define/user sys$output 'CHRwhere'iso08_uni.h	!ISO 8859-8 Hebrew
$ makeuctb iso08_uni.tbl
$ define/user sys$output 'CHRwhere'cp862_uni.h	!DosHebrew (cp862)
$ makeuctb cp862_uni.tbl
$ define/user sys$output 'CHRwhere'cp1255_uni.h	!WinHebrew (cp1255)
$ makeuctb cp1255_uni.tbl
$ define/user sys$output 'CHRwhere'iso09_uni.h	!ISO 8859-9 (Latin 5)
$ makeuctb iso09_uni.tbl
$ define/user sys$output 'CHRwhere'iso10_uni.h	!ISO 8859-10
$ makeuctb iso10_uni.tbl
$ define/user sys$output 'CHRwhere'iso15_uni.h	!ISO 8859-15
$ makeuctb iso15_uni.tbl
$ define/user sys$output 'CHRwhere'utf8_uni.h	!UNICODE UTF-8
$ makeuctb utf8_uni.tbl
$ define/user sys$output 'CHRwhere'rfc_suni.h	!RFC 1345 w/o Intro
$ makeuctb rfc_suni.tbl
$ define/user sys$output 'CHRwhere'mnem2_suni.h !RFC 1345 Mnemonic
$ makeuctb mnem2_suni.tbl
$ define/user sys$output 'CHRwhere'mnem_suni.h	!(not used)
$ makeuctb mnem_suni.tbl
$ v1 = 'f$verify(0)'
$ exit
$!
$ CLEANUP:
$    v1 = 'f$verify(0)'
$    write sys$output "Default directory:"
$    show default
$    v1 = f$verify(v)
$ exit
