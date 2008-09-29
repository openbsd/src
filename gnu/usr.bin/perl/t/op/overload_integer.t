#!./perl

use strict;
use warnings;

print "1..2\n";

package Foo;

use overload; 

sub import
{
    overload::constant 'integer' => sub { return shift; };
}

package main;

BEGIN { $INC{'Foo.pm'} = "/lib/Foo.pm" }

use Foo;

my $result = eval "5+6";

my $error = $@;

my $label = "No exception was thrown with an overload::constant 'integer' inside an eval.";
# TEST
if ($error eq "")
{
    print "ok 1 - $label\n"
}
else
{
    print "not ok 1 - $label\n";
    print "# Error is $error\n";
}

$label = "Correct solution";

if (!defined($result))
{
    $result = "";
}
# TEST
if ($result eq 11)
{
    print "ok 2 - $label\n";
}
else
{
    print "not ok 2 - $label\n";
    print "# Result is $result\n";
}

