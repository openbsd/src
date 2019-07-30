use warnings;
use strict;
use Test::Simple tests => 2;

use overload '""' => sub { 'stringvalue' }, fallback => 1;

BEGIN {
my $x = bless {}, 'main';
ok ($x eq 'stringvalue', 'fallback worked');
}


# NOTE: delete the next line and this test script will pass
use overload '+' => sub { die "unused"; };

my $x = bless {}, 'main';
ok (eval {$x eq 'stringvalue'}, 'fallback worked again');

