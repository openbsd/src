#!perl

use strict;
use warnings;

use Test::More tests => 176;

my $class;

BEGIN { $class = 'Math::BigInt'; }
BEGIN { use_ok($class); }

my @data;

while (<DATA>) {
    s/#.*$//;           # remove comments
    s/\s+$//;           # remove trailing whitespace
    next unless length; # skip empty lines

    my @in = split /:/;
    my $out = pop @in;

    # As class method.

    {
        my $x;
        my $test = qq|\$x = $class -> from_base("$in[0]", $in[1]|;
        $test .= qq|, "$in[2]"| if @in == 3;
        $test .= qq|);|;

        eval $test;
        #die $@ if $@;           # this should never happen
        die "\nThe following test died when eval()'ed. This indicates a ",
          "broken test\n\n    $test\n\nThe error message was\n\n    $@\n"
          if $@;

        subtest $test, sub {
            plan tests => 2,

            is(ref($x), $class, "output arg is a $class");
            is($x, $out, 'output arg has the right value');
        };
    }

    # As instance method.

    {
        for my $str ("-1", "0", "1", "-inf", "+inf", "NaN") {
            my $x;
            my $test = qq|\$x = $class -> new("$str");|;
            $test .= qq| \$x -> from_base("$in[0]", $in[1]|;
            $test .= qq|, "$in[2]"| if @in == 3;
            $test .= qq|);|;

            eval $test;
            #die $@ if $@;       # this should never happen
            die "\nThe following test died when eval()'ed. This indicates a ",
              "broken test\n\n    $test\n\nThe error message was\n\n    $@\n"
              if $@;

            subtest $test, sub {
                plan tests => 2,

                is(ref($x), $class, "output arg is a $class");
                is($x, $out, 'output arg has the right value');
            };
        }
    }
}

__END__

# Base 2

11111010:2:250
11111010:2:01:250

# Base 8

372:8:250
372:8:01234567:250

# Base 10 (in the last case, use a truncted collation sequence that does not
# include unused characters)

250:10:250
250:10:0123456789:250
250:10:012345:250

# Base 16

fa:16:250
FA:16:250
fa:16:0123456789abcdef:250

# Base 3

100021:3:250
100021:3:012:250

/|-:3:-/|:15

# Base 4

3322:4:250
3322:4:0123:250

# Base 5

2000:5:250
2000:5:01234:250
caaa:5:abcde:250

# when base is less than or equal to 36, case is ignored

6Y:36:250
6y:36:250

6S:37:250
7H:37:276

121:3:16

XYZ:36:44027

Why:62:125734
