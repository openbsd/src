use strict;
use warnings; no warnings 'once';

use Test::More tests => 1;

use XS::APItest;
use Hash::Util 'lock_value';

my %h;
$h{g} = *foo;
lock_value %h, 'g';

ok(!SvIsCOW($h{g}), 'SvIsCOW is honest when it comes to globs');
