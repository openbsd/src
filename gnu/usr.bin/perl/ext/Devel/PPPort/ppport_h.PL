package Devel::PPPort; 
sub bootstrap {}; 
require "PPPort.pm";
WriteFile("ppport.tmp");
{
    local $/;
    my $old = '';
    my $new = '';
    $old = <FH> if open(FH, "ppport.h");	close FH;
    $new = <FH> if open(FH, "ppport.tmp");	close FH;
    if ($old ne $new) {
	unlink("ppport.h");
	rename("ppport.tmp", "ppport.h");
    }
    unlink("ppport.tmp");
}

