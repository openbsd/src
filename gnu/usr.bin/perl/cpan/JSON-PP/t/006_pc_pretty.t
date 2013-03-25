#! perl

# copied over from JSON::PP::PC and modified to use JSON::PP
# copied over from JSON::PP::XS and modified to use JSON::PP

use strict;
use Test::More;
BEGIN { plan tests => 9 };

BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }

use JSON::PP;

my ($js,$obj,$json);
my $pc = new JSON::PP;

$obj = {foo => "bar"};
$js = $pc->encode($obj);
is($js,q|{"foo":"bar"}|);

$obj = [10, "hoge", {foo => "bar"}];
$pc->pretty (1);
$js = $pc->encode($obj);
is($js,q|[
   10,
   "hoge",
   {
      "foo" : "bar"
   }
]
|);

$obj = { foo => [ {a=>"b"}, 0, 1, 2 ] };
$pc->pretty(0);
$js = $pc->encode($obj);
is($js,q|{"foo":[{"a":"b"},0,1,2]}|);


$obj = { foo => [ {a=>"b"}, 0, 1, 2 ] };
$pc->pretty(1);
$js = $pc->encode($obj);
is($js,q|{
   "foo" : [
      {
         "a" : "b"
      },
      0,
      1,
      2
   ]
}
|);

$obj = { foo => [ {a=>"b"}, 0, 1, 2 ] };
$pc->pretty(0);
$js = $pc->encode($obj);
is($js,q|{"foo":[{"a":"b"},0,1,2]}|);


$obj = {foo => "bar"};
$pc->indent(3); # original -- $pc->indent(1);
is($pc->encode($obj), qq|{\n   "foo":"bar"\n}\n|, "nospace");
$pc->space_after(1);
is($pc->encode($obj), qq|{\n   "foo": "bar"\n}\n|, "after");
$pc->space_before(1);
is($pc->encode($obj), qq|{\n   "foo" : "bar"\n}\n|, "both");
$pc->space_after(0);
is($pc->encode($obj), qq|{\n   "foo" :"bar"\n}\n|, "before");

