#	$Sendmail: HP-UX.10.x,v 8.14 1999/08/10 00:06:41 gshapiro Exp $
define(`confCC', `cc -Aa')
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `-D_HPUX_SOURCE -DV4FS')
define(`confOPTIMIZE', `+O3')
define(`confLIBS', `-lndbm')
define(`confSHELL', `/usr/bin/sh')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confSBINGRP', `mail')
