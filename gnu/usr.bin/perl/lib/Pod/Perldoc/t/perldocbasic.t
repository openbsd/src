
use Test;
BEGIN {plan tests => 3};
ok 1;
require Pod::Perldoc;
ok($Pod::Perldoc::VERSION)
 and print "# Pod::Perldoc version $Pod::Perldoc::VERSION\n";
ok 1;

