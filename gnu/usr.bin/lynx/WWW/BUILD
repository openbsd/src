#! /bin/csh
#			Build all WWW Code for this platform
#
#	Figure out what sort of unix this is
#	(NeXT machines don't have uname!)

set UNAME=NeXT
if (-e /usr/bin/uname) set UNAME=`/usr/bin/uname`
if (-e /bin/uname) set UNAME=`/bin/uname`
if (-e /usr/apollo/bin) set UNAME=`ver sys5.3 /bin/uname`
if ( $UNAME == "" ) then
    if (-r /NextApps ) set UNAME=next
endif
#
setenv UNAME $UNAME
# For apollo, must use bsd mode. Also, WWW_MACH not inherited through make!
if ($UNAME == "DomainOS")    setenv WWW_MACH	apollo_m68k
if ($UNAME == next)	setenv WWW_MACH 	next
if ($UNAME == "HP-UX")	setenv WWW_MACH 	snake
if ($UNAME == "IRIX")	setenv WWW_MACH 	sgi
if ($UNAME == "SunOS")	setenv WWW_MACH		sun4
if ($UNAME == "ULTRIX")	setenv WWW_MACH		decstation
if ($UNAME == "AIX")    setenv WWW_MACH		rs6000
if ($UNAME == "OSF1")   setenv WWW_MACH		osf1

if ($WWW_MACH == "") then
    echo "Please edit BUILD file to include your machine OS"
    echo "and mail differences back to www-bug@info.cern.ch
    exit -99
endif
echo "________________________________________________________________"
echo "WWW build for machine type:                            " $WWW_MACH

#	Now go do build

#	We don't want SHELL set to something funny to screw up make

(cd All/Implementation; unsetenv SHELL; make)
set stat = $status
echo
echo "WWW build for " $WWW_MACH  " done. status = " $stat
exit $stat
