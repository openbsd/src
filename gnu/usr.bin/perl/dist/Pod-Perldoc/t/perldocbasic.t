
use Test::More tests => 3;
pass();
require Pod::Perldoc;
ok($Pod::Perldoc::VERSION)
 and print "# Pod::Perldoc version $Pod::Perldoc::VERSION\n";
pass();

