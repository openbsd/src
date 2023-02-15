# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 30;

my $class;

BEGIN { $class = 'Math::BigInt'; }
BEGIN { use_ok($class); }

while (<DATA>) {
    s/#.*$//;           # remove comments
    s/\s+$//;           # remove trailing whitespace
    next unless length; # skip empty lines

    my @in = split /:/;
    my $out = pop @in;

    my ($x, $xo, $y);
    my $test = qq|\$x = $class -> new("$in[0]");|;
    $test .= qq| \$xo = \$x -> copy();|;
    $test .= qq| \$y = \$x -> to_base($in[1]|;
    $test .= qq|, "$in[2]"| if @in == 3;
    $test .= qq|);|;

    eval $test;
    #die $@ if $@;       # this should never happen
    die "\nThe following test died when eval()'ed. This indicates a ",
      "broken test\n\n    $test\n\nThe error message was\n\n    $@\n"
      if $@;

    subtest $test, sub {
        plan tests => 2,

        is($x, $xo, "invocand object was not changed");
        is($y, $out, 'output arg has the right value');
    };
}

__END__

# Base 2

0:2:0
1:2:1
2:2:10
0:2:ab:a
1:2:ab:b
2:2:ab:ba

250:2:11111010
250:2:01:11111010

# Base 8

250:8:372
250:8:01234567:372

# Base 10 (in the last case, use a truncted collation sequence that does not
# include unused characters)

250:10:250
250:10:0123456789:250
250:10:012345:250

# Base 16

250:16:FA
250:16:0123456789abcdef:fa
250:16:0123456789abcdef:fa

# Base 3

250:3:100021
250:3:012:100021

15:3:-/|:/|-

# Base 4

250:4:3322
250:4:0123:3322

# Base 5

250:5:2000
250:5:01234:2000
250:5:abcde:caaa

# Other bases

250:36:6Y

250:37:6S

16:3:121
44027:36:XYZ
125734:62:Why
