$ v = 'f$verify(0)'
$!			BUILD-HEADER.COM
$!
$!   Command file to use MAKEUCTB.EXE on VMS systems for creating
$!   a chrtrans header (foo.h) file from a table (foo.tbl) file.
$!   Use the file root as P1, e.g.:
$!
$!	$ @build-header iso05_uni
$!
$!   will create iso05_uni.h from iso05_uni.tbl.
$!
$!   28-Jun-1997	F.Macrides		macrides@sci.wfeb.edu
$!	Initial version, for Lynx v2.7.1+fotemods
$!
$ ON CONTROL_Y THEN GOTO CLEANUP
$ ON ERROR THEN GOTO CLEANUP
$ CHRproc = f$environment("PROCEDURE")
$ CHRwhere = f$parse(CHRproc,,,"DEVICE") + f$parse(CHRproc,,,"DIRECTORY")
$!
$ Create_header:
$!=============
$ v1 = f$verify(1)
$!
$!	Create a Lynx [.SRC.CHRTRANS] header file.
$!
$ makeuctb := $'CHRwhere'makeuctb
$ makeuctb 'P1'.tbl
$ v1 = 'f$verify(0)'
$ exit
$!
$ CLEANUP:
$    v1 = 'f$verify(0)'
$    write sys$output "Default directory:"
$    show default
$    v1 = f$verify(v)
$ exit
