#!/usr/bin/perl -w

# copied over from JSON::PP::XS and modified to use JSON::PP

use strict;

use Test::More;
BEGIN { plan tests => 697 };
BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }


use JSON::PP;

if ( $] >= 5.006 ) {

eval <<'TEST' or die "Failed to eval test code for version $]: $@";

sub splitter {
   my ($coder, $text) = @_;

   $coder->canonical(1) if $] >= 5.017009;

   for (0 .. length $text) {
      my $a = substr $text, 0, $_;
      my $b = substr $text, $_;

      $coder->incr_parse ($a);
      $coder->incr_parse ($b);

      my $data = $coder->incr_parse;
      ok ($data);
      is ($coder->encode ($data), $coder->encode ($coder->decode ($text)), "data");
      ok ($coder->incr_text =~ /^\s*$/, "tailws");
   }
}



splitter +JSON::PP->new              , '  ["x\\"","\\u1000\\\\n\\nx",1,{"\\\\" :5 , "": "x"}]';
splitter +JSON::PP->new              , '[ "x\\"","\\u1000\\\\n\\nx" , 1,{"\\\\ " :5 , "": " x"} ] ';
splitter +JSON::PP->new->allow_nonref, '"test"';
splitter +JSON::PP->new->allow_nonref, ' "5" ';



{
   my $text = '[5],{"":1} , [ 1,2, 3], {"3":null}';
   my $coder = new JSON::PP;
   for (0 .. length $text) {
      my $a = substr $text, 0, $_;
      my $b = substr $text, $_;

      $coder->incr_parse ($a);
      $coder->incr_parse ($b);

      my $j1 = $coder->incr_parse; ok ($coder->incr_text =~ s/^\s*,//, "cskip1");
      my $j2 = $coder->incr_parse; ok ($coder->incr_text =~ s/^\s*,//, "cskip2");
      my $j3 = $coder->incr_parse; ok ($coder->incr_text =~ s/^\s*,//, "cskip3");
      my $j4 = $coder->incr_parse; ok ($coder->incr_text !~ s/^\s*,//, "cskip4");
      my $j5 = $coder->incr_parse; ok ($coder->incr_text !~ s/^\s*,//, "cskip5");

      ok ('[5]' eq encode_json $j1, "cjson1");
      ok ('{"":1}' eq encode_json $j2, "cjson2");
      ok ('[1,2,3]' eq encode_json $j3, "cjson3");
      ok ('{"3":null}' eq encode_json $j4, "cjson4");
      ok (!defined $j5, "cjson5");
   }
}

{
   my $text = '[x][5]';
   my $coder = new JSON::PP;
   $coder->incr_parse ($text);
   ok (!eval { $coder->incr_parse }, "sparse1");
   ok (!eval { $coder->incr_parse }, "sparse2");
   $coder->incr_skip;
   ok ('[5]' eq $coder->encode (scalar $coder->incr_parse), "sparse3");
}

1
TEST


}
else {


eval <<'TEST' or die "Failed to eval test code for version $]: $@";

my $incr_text;

sub splitter {
   my ($coder, $text) = @_;

   for (0 .. length $text) {
      my $a = substr $text, 0, $_;
      my $b = substr $text, $_;

      $coder->incr_parse ($a);
      $coder->incr_parse ($b);

      my $data = $coder->incr_parse;
      ok ($data);
      ok ($coder->encode ($data) eq $coder->encode ($coder->decode ($text)), "data");
      ok (($incr_text = $coder->incr_text) =~ /^\s*$/, "tailws");
   }
}

splitter +JSON::PP->new              , '  ["x\\"","\\u1000\\\\n\\nx",1,{"\\\\" :5 , "": "x"}]';
splitter +JSON::PP->new              , '[ "x\\"","\\u1000\\\\n\\nx" , 1,{"\\\\ " :5 , "": " x"} ] ';
splitter +JSON::PP->new->allow_nonref, '"test"';
splitter +JSON::PP->new->allow_nonref, ' "5" ';


{
   my $text = '[5],{"":1} , [ 1,2, 3], {"3":null}';
   my $coder = new JSON::PP;
   for (0 .. length $text) {
      my $a = substr $text, 0, $_;
      my $b = substr $text, $_;

      $coder->incr_parse ($a);
      $coder->incr_parse ($b);

      my $j1 = $coder->incr_parse; ok ( $coder->incr_text(  ($incr_text = $coder->incr_text) =~ s/^\s*,// and $incr_text ), "cskip1");
      my $j2 = $coder->incr_parse; ok ( $coder->incr_text(  ($incr_text = $coder->incr_text) =~ s/^\s*,// and $incr_text ), "cskip2");
      my $j3 = $coder->incr_parse; ok ( $coder->incr_text(  ($incr_text = $coder->incr_text) =~ s/^\s*,// and $incr_text ), "cskip3");
      my $j4 = $coder->incr_parse; ok (($incr_text = $coder->incr_text) !~ s/^\s*,//, "cskip4");
      my $j5 = $coder->incr_parse; ok (($incr_text = $coder->incr_text) !~ s/^\s*,//, "cskip5");

      ok ('[5]' eq encode_json $j1, "cjson1");
      ok ('{"":1}' eq encode_json $j2, "cjson2");
      ok ('[1,2,3]' eq encode_json $j3, "cjson3");
      ok ('{"3":null}' eq encode_json $j4, "cjson4");
      ok (!defined $j5, "cjson5");
   }
}

{
   my $text = '[x][5]';
   my $coder = new JSON::PP;
   $coder->incr_parse ($text);
   ok (!eval { $coder->incr_parse }, "sparse1");
   ok (!eval { $coder->incr_parse }, "sparse2");
   $coder->incr_skip;
   ok ('[5]' eq $coder->encode (scalar $coder->incr_parse), "sparse3");
}


TEST

} # for 5.005




{
   my $coder = JSON::PP->new->max_size (5);
   ok (!$coder->incr_parse ("[    "), "incsize1");
   eval q{ !$coder->incr_parse ("]  ") }; ok ($@ =~ /6 bytes/, "incsize2 $@");
}

{
   my $coder = JSON::PP->new->max_depth (3);
   ok (!$coder->incr_parse ("[[["), "incdepth1");
   eval q{ !$coder->incr_parse (" [] ") }; ok ($@ =~ /maximum nesting/, "incdepth2 $@");
}

{
   my $coder = JSON::PP->new;

   my $res = eval { $coder->incr_parse("]") };
   my $e = $@; # test more clobbers $@, we need it twice

   ok(!$res, "unbalanced bracket" );
   ok($e, "got error");
   like( $e, qr/malformed/, "malformed json string error" );

   $coder->incr_skip;

   is_deeply(eval { $coder->incr_parse("[42]") }, [42], "valid data after incr_skip");
}

