#
# This fragment illustrates how to invoke MacCVS via AppleEvents and pass environment
# variables to MacCVS.  Not all CVS environment variables make sense for MacOS.  This
# fragment also illustrates the default result handling mode, which is to put the info on
# a SIOUX console.
#

&MacPerl'DoAppleScript(<<ENDIT);
tell application "Data:Mike:Projects:cvs-1.8.6:macintosh:MacCVS"
	do script { "status" } environment { "CVSROOT", "ladwig\@manic:/projects/sdt/repository/eM2" }
end tell
ENDIT

exit(0);



#
# This fragment illustrates how to invoke MacCVS change its directory prior to executing
# the CVS command.  This is akin to being in a directory when you invoke CVS.
#

&MacPerl'DoAppleScript(<<ENDIT);
tell application "Data:Mike:Projects:cvs-1.8.6:macintosh:MacCVS"
	do script { "status" } environment { "CVSROOT", "ladwig\@manic:/projects/sdt/repository/eM2" } pathway "OS:Workspace:eM:src:daemon"
end tell
ENDIT

exit(0);



#
# This fragment illustrates how to get MacCVS to return results via AppleEvents.
# Note:  If you add "NoLineBuffer True" after the "Mode AE", each individual line
# of results will be returned in a separate AppleEvent.
#

use Mac::AppleEvents;

AEInstallEventHandler("MCVS", "DATA", "MacCVSData", 0);

&MacPerl'DoAppleScript(<<ENDIT);
tell application "Data:Mike:Projects:cvs-1.8.6:macintosh:MacCVS.PPC"
	do script { "-help add" } environment { "CVSROOT", "ladwig\@manic:/projects/sdt/repository/sdt" } Mode AE
end tell
ENDIT

$done = 0;
$in = 0;
while( $done = 0 ) { sleep(1);}
print "QUITTING!\n";

AERemoveEventHandler ("MCVS", "DATA");
exit(0);

sub MacCVSData {
	my($event) = @_;
	
	print "**** MCVS/Data Handler called\n";

 $rDesc = AEGetParamDesc($event, "----");
	if( $rDesc )
	{
		$data =  AEPrint($rDesc);
		chop $data; $data = substr($data, 1);
		print "---- data = <$data> \n";
	}
	AEDisposeDesc($rDesc);

 $rDesc = AEGetParamDesc($event, "DONE");
	if( $rDesc )
		{ print "!!!! DONE\n"; $done = 1; AEDisposeDesc($rDesc); }

	print "Exiting MCVS/Data Handler ****\n";
	return 0;
}





#
# This fragment illustrates how to have MacCVS save the results to a file in your MacOS
# filesystem.
#

&MacPerl'DoAppleScript(<<ENDIT);
tell application "Data:Projects:cvs-1.8.6:macintosh:MacCVS"
	do script { "-d ladwig\@manic:/projects/sdt/repository/eM2", "status" } mode file filename "os:out.file"
end tell
ENDIT

exit(0);

