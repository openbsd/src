
use Test;
BEGIN {plan tests => 2};
ok 1;
require Pod::Perldoc::ToText;
$Pod::Perldoc::VERSION
 and print "# Pod::Perldoc version $Pod::Perldoc::VERSION\n";
ok 1;

