#!perl
use strict;
use warnings;

require './test.pl';

plan(tests => 7);

{
    no warnings 'deprecated';
    print <<;   # Yow!
ok 1

    # previous line intentionally left blank.

    my $yow = "ok 2";
    print <<;   # Yow!
$yow

    # previous line intentionally left blank.
}

curr_test(3);


{
    my %foo = (aap => "monkey");
    my $foo = '';
    is("@{[$foo{'aap'}]}", 'monkey', 'interpolation of hash lookup with space between lexical variable and subscript');
    is("@{[$foo {'aap'}]}", 'monkey', 'interpolation of hash lookup with space between lexical variable and subscript - test for [perl #70091]');

# Original bug report [perl #70091]
#  #!perl
#  use warnings;
#  my %foo;
#  my $foo = '';
#  (my $tmp = $foo) =~ s/^/$foo {$0}/e;
#  __END__
#
#  This program causes a segfault with 5.10.0 and 5.10.1.
#
#  The space between '$foo' and '{' is essential, which is why piping
#  it through perl -MO=Deparse "fixes" it.
#

}

{
 delete local $ENV{PERL_UNICODE};
 fresh_perl_is(
  'BEGIN{ ++$_ for @INC{"charnames.pm","_charnames.pm"} } "\N{a}"',
  'Constant(\N{a}) unknown at - line 1, within string' . "\n"
 ."Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  'correct output (and no crash) when charnames cannot load for \N{...}'
 );
}
fresh_perl_is(
  'BEGIN{ ++$_ for @INC{"charnames.pm","_charnames.pm"};
          $^H{charnames} = "foo" } "\N{a}"',
  "Undefined subroutine &main::foo called at - line 2.\n"
 ."Propagated at - line 2, within string\n"
 ."Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  'no crash when charnames cannot load and %^H holds string'
);
fresh_perl_is(
  'BEGIN{ ++$_ for @INC{"charnames.pm","_charnames.pm"};
          $^H{charnames} = \"foo" } "\N{a}"',
  "Not a CODE reference at - line 2.\n"
 ."Propagated at - line 2, within string\n"
 ."Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  'no crash when charnames cannot load and %^H holds string reference'
);
