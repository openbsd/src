use strict;
use Cwd;
my $CWD = getcwd();
$CWD =~ s!^[a-z]:!!i;
$CWD =~ s!/!\\!g;
$CWD;
