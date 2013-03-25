#!./perl

use strict;
use Test::More tests => 6;

my $v_plus = $] + 1;
my $v_minus = $] - 1;

unless (eval 'use open ":std"; 1') {
  # pretend that open.pm is present
  $INC{'open.pm'} = 'open.pm';
  eval 'sub open::foo{}';		# Just in case...
}

no strict;

is( eval "use if ($v_minus > \$]), strict => 'subs'; \${'f'} = 12", 12,
    '"use if" with a false condition, fake pragma');
is( eval "use if ($v_minus > \$]), strict => 'refs'; \${'f'} = 12", 12,
    '"use if" with a false condition and a pragma');

is( eval "use if ($v_plus > \$]), strict => 'subs'; \${'f'} = 12", 12,
    '"use if" with a true condition, fake pragma');

is( eval "use if ($v_plus > \$]), strict => 'refs'; \${'f'} = 12", undef,
    '"use if" with a true condition and a pragma');
like( $@, qr/while "strict refs" in use/, 'expected error message'),

# Old version had problems with the module name 'open', which is a keyword too
# Use 'open' =>, since pre-5.6.0 could interpret differently
is( (eval "use if ($v_plus > \$]), 'open' => IN => ':crlf'; 12" || 0), 12,
    '"use if" with open');
