#!perl -T

use strict;
use warnings;

use Test::More tests => 2;

use Scalar::Util qw(tainted);
use Locale::Maketext;

my @ENV_values = values %ENV;
my $tainted_value;
do { $tainted_value = shift @ENV_values  } while(!$tainted_value || ref $tainted_value);
$tainted_value =~ s/([\[\]])/~$1/g;

ok(tainted($tainted_value), "\$tainted_value is tainted") or die('huh... %ENV has no entries? I don\'t know how to test taint without it');

my $result = Locale::Maketext::_compile("hello [_1]", $tainted_value);

pass("_compile does not hang on tainted values");

