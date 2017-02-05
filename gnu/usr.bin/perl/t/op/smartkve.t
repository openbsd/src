#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}
use strict;
use warnings;
no warnings 'experimental::refaliasing';
use vars qw($data $array $values $hash $errpat);

plan 'no_plan';

my $empty;

# Keys -- errors
$errpat = qr/Experimental keys on scalar is now forbidden/;

eval "keys undef";
like($@, $errpat,
  'Errors: keys undef throws error'
);

undef $empty;
eval q"keys $empty";
like($@, $errpat,
  'Errors: keys $undef throws error'
);

is($empty, undef, 'keys $undef does not vivify $undef');

eval "keys 3";
like($@, qr/Type of arg 1 to keys must be hash/,
  'Errors: keys CONSTANT throws error'
);

eval "keys qr/foo/";
like($@, $errpat,
  'Errors: keys qr/foo/ throws error'
);

eval q"keys $hash qw/fo bar/";
like($@, $errpat,
  'Errors: keys $hash, @stuff throws error'
) or print "# Got: $@";

# Values -- errors
$errpat = qr/Experimental values on scalar is now forbidden/;

eval "values undef";
like($@, $errpat,
  'Errors: values undef throws error'
);

undef $empty;
eval q"values $empty";
like($@, $errpat,
  'Errors: values $undef throws error'
);

is($empty, undef, 'values $undef does not vivify $undef');

eval "values 3";
like($@, qr/Type of arg 1 to values must be hash/,
  'Errors: values CONSTANT throws error'
);

eval "values qr/foo/";
like($@, $errpat,
  'Errors: values qr/foo/ throws error'
);

eval q"values $hash qw/fo bar/";
like($@, $errpat,
  'Errors: values $hash, @stuff throws error'
) or print "# Got: $@";

# Each -- errors
$errpat = qr/Experimental each on scalar is now forbidden/;

eval "each undef";
like($@, $errpat,
  'Errors: each undef throws error'
);

undef $empty;
eval q"each $empty";
like($@, $errpat,
  'Errors: each $undef throws error'
);

is($empty, undef, 'each $undef does not vivify $undef');

eval "each 3";
like($@, qr/Type of arg 1 to each must be hash/,
  'Errors: each CONSTANT throws error'
);

eval "each qr/foo/";
like($@, $errpat,
  'Errors: each qr/foo/ throws error'
);

eval q"each $hash qw/foo bar/";
like($@, $errpat,
  'Errors: each $hash, @stuff throws error'
) or print "# Got: $@";

use feature 'refaliasing';
my $a = 7;
our %h;
\$h{f} = \$a;
($a, $b) = each %h;
is "$a $b", "f 7", 'each %hash in list assignment';
$a = 7;
($a, $b) = (3, values %h);
is "$a $b", "3 7", 'values %hash in list assignment';
*a = sub { \@_ }->($a);
$a = 7;
($a, $b) = each our @a;
is "$a $b", "0 7", 'each @array in list assignment';
$a = 7;
($a, $b) = (3, values @a);
is "$a $b", "3 7", 'values @array in list assignment';
