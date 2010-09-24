
use Test::More tests => 2;
pass();
require Pod::Perldoc::ToChecker;
$Pod::Perldoc::VERSION
 and print "# Pod::Perldoc version $Pod::Perldoc::VERSION\n";
pass();

