#	$Sendmail: NeXT.2.x,v 8.11 1999/12/08 06:08:07 gshapiro Exp $
define(`confBEFORE', `unistd.h dirent.h')
define(`confMAPDEF', `-DNDBM -DNIS -DNETINFO')
define(`confENVDEF', `-DNeXT')
define(`confLIBS', `-ldbm')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/ucb')
define(`confEBINDIR', `/usr/lib')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confRANLIBOPTS', `-c')
PUSHDIVERT(3)
unistd.h:
	cp /dev/null unistd.h

dirent.h:
	echo "#include <sys/dir.h>" > dirent.h
	echo "#define dirent	direct" >> dirent.h
POPDIVERT
