$!
$! Lynx_Dir:VMSPrint.com  -  Alan J. Hirsh (hirsh@atuk.aspentec.com)
$! ---------------------
$!	Lynx deletes temporary files on exit.  If your printer queue
$!	is very busy such that Lynx is deleting the files before they
$!	have been queued for printing, use PRINTER commands in lynx.cfg
$!	which invoke this script.
$!
$! PRINTER:description for menu:@Lynx_Dir\:VMSPrint queue_name %s:FALSE:58
$!
$! P1 = queue_name (e.g., sys$print)  P2 = temporary Lynx file (%s)
$! ---------------------------------  -----------------------------
$ copy 'P2' 'P2'_temp_print
$ print/queue='P1'/delete 'P2'_temp_print
$ exit
