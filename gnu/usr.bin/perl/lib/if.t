#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More tests => 4;

my $v_plus = $] + 1;
my $v_minus = $] - 1;


ok( eval "use if ($v_minus > \$]), strict => 'subs'; \${'f'} = 12" eq 12,
    '"use if" with a false condition, fake pragma');

ok( eval "use if ($v_minus > \$]), strict => 'refs'; \${'f'} = 12" eq 12,
    '"use if" with a false condition and a pragma');

ok( eval "use if ($v_plus > \$]), strict => 'subs'; \${'f'} = 12" eq 12,
    '"use if" with a true condition, fake pragma');

ok( (not defined eval "use if ($v_plus > \$]), strict => 'refs'; \${'f'} = 12"
     and $@ =~ /while "strict refs" in use/),
    '"use if" with a true condition and a pragma');

