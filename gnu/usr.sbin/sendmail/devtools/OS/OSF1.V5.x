#	$Sendmail: OSF1.V5.x,v 8.3 2001/08/15 08:55:54 guenther Exp $
define(`confCC', `cc -std1 -Olimit 1000')
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `')
define(`confLIBS', `-ldbm')
define(`confSM_OS_HEADER', `sm_os_osf1')
define(`confSTDIR', `/var/adm/sendmail')
define(`confINSTALL', `installbsd')
define(`confEBINDIR', `/usr/lbin')
define(`confUBINDIR', `${BINDIR}')
define(`confDEPEND_TYPE', `CC-M')

define(`confMTCCOPTS', `-D_REENTRANT')
define(`confMTLDOPTS', `-lpthread')
