#	$Sendmail: HP-UX.11.x,v 8.11 1999/07/02 23:54:55 rand Exp $

# +z is to generate position independant code
define(`confCC', `cc -Ae +z')
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `-DV4FS -DHPUX11')
define(`confOPTIMIZE', `+O3')
define(`confLIBS', `-ldbm -lnsl')
define(`confSHELL', `/usr/bin/sh')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confSBINGRP', `mail')

define(`confMTCCOPTS', `-D_POSIX_C_SOURCE=199506L')
define(`confMTLDOPTS', `-lpthread')
define(`confLD', `ld')
define(`confLDOPTS_SO', `-b')
define(`confCCOPTS_SO', `')
