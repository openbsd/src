$! v = 'f$verify(0)'
$!
$!	FIXED512.COM:	Uses the FILE utility or SET FILE/ATTR
$!			to convert binary stream_LF file headers
$!			to FIXED 512.
$!
$!	Author:	F.Macrides (macrides@sci.wfeb.edu)
$!
$!	Usage:  @Lynx_Dir:FIXED512 <stream_LF file that's actually binary>
$!
$!	Lynx performs the conversion via internal code if the file is
$!	recognized as binary.  This command procedure can be used externally
$!	if that detection failed, e.g., for binary files downloaded from FTP
$!      servers with extensions that hadn't been mapped to a binary MIME
$!	type in the Lynx configuration files.
$! 
$!===========================================================================
$!
$!  Get and build Joe Meadow's FILE utility for VAX and AXP:
$!	ftp://ftp.wku.edu/vms/fileserv/file.zip
$!  and define it here, or system-wide, as a foreign command.
$!
$! FILE := $device:[directory]FILE.EXE
$!-----------------------------------------------------------
$!
$!  Just exit and leave the file stream_LF if we didn't get
$!  the filename as P1, or if it's not a valid file spec.
$!
$ IF P1 .EQS. "" THEN EXIT
$ IF F$SEARCH(P1) .EQS. "" THEN EXIT
$!-----------------------------------------------------------
$!
$!  If we have FILE installed, use it.
$!
$ If f$type(FILE) .eqs. "STRING"
$ Then
$  FILE 'P1'-
   /TYPE=FIXED -
   /ATTRIBUTES=NOIMPLIEDCC -
   /CHARACTERISTICS=BEST_TRY_CONTIGUOUS -
   /RECORD_SIZE=512 -
   /MAXIMUM_RECORD_SIZE=512
$  EXIT
$ EndIf
$!-----------------------------------------------------------
$!
$!  If we get to here, and we're VMS v6+,
$!  we'll use SET FILE/ATTR to do it.
$!
$ If f$extract(1,1,f$getsyi("VERSION")) .ge. 6
$ Then
$   SET FILE/ATTR=(RFM:FIX,LRL:512,MRS:512,RAT:NONE) 'P1'
$ EndIf
$ EXIT
