#	$Sendmail: AIX.4.x,v 8.12 1999/06/02 22:53:35 gshapiro Exp $
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `-D_AIX4')
define(`confOPTIMIZE', `-O3 -qstrict')
define(`confLIBS', `-ldbm')
define(`confINSTALL', `/usr/ucb/install')
define(`confEBINDIR', `/usr/lib')
define(`confSBINGRP', `system')
define(`confDEPEND_TYPE', `AIX')
