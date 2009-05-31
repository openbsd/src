$! Command file to remove files created by build.com -T.Dickey
$ set noverify
$ delete [...]*.obj;*
$ delete [...]*.olb;*
$ delete [...]*.lis;*
$ delete [...]*.map;*
$ delete [...]*.exe;*
$
$	name = ""
$ loop_tbl :
$	last = name
$	name = F$SEARCH("[.src.chrtrans]*.tbl;",1)
$	if name .nes. "" .and. name .nes. last
$	then
$		head = "[.src.chrtrans]" + F$PARSE(name,,,"NAME","SYNTAX_ONLY") + ".h"
$		if f$search("''head'",2) .nes. ""
$		then
$			write sys$output "head " + head
$			delete 'head;*
$		endif
$		goto loop_tbl
$	endif
