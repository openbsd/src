#	$Sendmail: SCO.5.x,v 8.13 1999/04/26 16:11:50 gshapiro Exp $
define(`confCC', `cc -b elf')
define(`confLIBS', `-lsocket -lndbm -lprot -lcurses -lm -lx -lgen')
define(`confMAPDEF', `-DMAP_REGEX -DNDBM')
define(`confSBINGRP', `bin')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/bin')
define(`confINSTALL', `${BUILDBIN}/install.sh')
