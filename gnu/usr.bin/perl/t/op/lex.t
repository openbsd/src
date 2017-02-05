#!perl

# Tests too complex for t/base/lex.t

use strict;
use warnings;

BEGIN { chdir 't' if -d 't'; require './test.pl'; }

plan(tests => 25);

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

# not fresh_perl_is, as it seems to hide the error
is runperl(
    nolib => 1, # -Ilib may also hide the error
    progs => [
      '*{',
      '         XS::APItest::gv_fetchmeth_type()',
      '}'
    ],
    stderr => 1,
   ),
  "Undefined subroutine &XS::APItest::gv_fetchmeth_type called at -e line "
 ."2.\n",
  'no buffer corruption with multiline *{...expr...}'
;

$_ = "rhubarb";
is ${no strict; \$_}, "rhubarb", '${no strict; ...}';
is join("", map{no strict; "rhu$_" } "barb"), 'rhubarb',
  'map{no strict;...}';

# [perl #123753]
fresh_perl_is(
  '$eq = "ok\n"; print $' . "\0eq\n",
  "ok\n",
   { stderr => 1 },
  '$ <null> ident'
);
fresh_perl_is(
  '@eq = "ok\n"; print @' . "\0eq\n",
  "ok\n",
   { stderr => 1 },
  '@ <null> ident'
);
fresh_perl_is(
  '%eq = ("o"=>"k\n"); print %' . "\0eq\n",
  "ok\n",
   { stderr => 1 },
  '% <null> ident'
);
fresh_perl_is(
  'sub eq { "ok\n" } print &' . "\0eq\n",
  "ok\n",
   { stderr => 1 },
  '& <null> ident'
);
fresh_perl_is(
  '$eq = "ok\n"; print ${*' . "\0eq{SCALAR}}\n",
  "ok\n",
   { stderr => 1 },
  '* <null> ident'
);
SKIP: {
    skip "Different output on EBCDIC (presumably)", 2 if $::IS_EBCDIC;
    fresh_perl_is(
      qq'"ab}"ax;&\0z\x8Ao}\x82x;', <<gibberish,
Bareword found where operator expected at - line 1, near ""ab}"ax"
	(Missing operator before ax?)
syntax error at - line 1, near ""ab}"ax"
Unrecognized character \\x8A; marked by <-- HERE after ab}"ax;&\0z<-- HERE near column 12 at - line 1.
gibberish
       { stderr => 1 },
      'gibberish containing &\0z - used to crash [perl #123753]'
    );
    fresh_perl_is(
      qq'"ab}"ax;&{+z}\x8Ao}\x82x;', <<gibberish,
Bareword found where operator expected at - line 1, near ""ab}"ax"
	(Missing operator before ax?)
syntax error at - line 1, near ""ab}"ax"
Unrecognized character \\x8A; marked by <-- HERE after }"ax;&{+z}<-- HERE near column 14 at - line 1.
gibberish
       { stderr => 1 },
      'gibberish containing &{+z} - used to crash [perl #123753]'
    );
}

fresh_perl_is(
  '/$a[/<<a',
  "Missing right curly or square bracket at - line 1, within pattern\n" .
  "syntax error at - line 1, at EOF\n" .
  "Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  '/$a[/<<a with no newline [perl #123712]'
);
fresh_perl_is(
  '/$a[m||/<<a',
  "Missing right curly or square bracket at - line 1, within pattern\n" .
  "syntax error at - line 1, at EOF\n" .
  "Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  '/$a[m||/<<a with no newline [perl #123712]'
);

fresh_perl_is(
  '"@{"',
  "Missing right curly or square bracket at - line 1, within string\n" .
  "syntax error at - line 1, at EOF\n" .
  "Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  '"@{" [perl #123712]'
);

fresh_perl_is(
  '/$0{}/',
  'syntax error at - line 1, near "{}"' . "\n" .
  "Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  '/$0{}/ with no newline [perl #123802]'
);
fresh_perl_is(
  '"\L\L"',
  'syntax error at - line 1, near "\L\L"' . "\n" .
  "Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  '"\L\L" with no newline [perl #123802]'
);
fresh_perl_is(
  '<\L\L>',
  'syntax error at - line 1, near "\L\L"' . "\n" .
  "Execution of - aborted due to compilation errors.\n",
   { stderr => 1 },
  '<\L\L> with no newline [perl #123802]'
);

is eval "qq'@\x{ff13}'", "\@\x{ff13}",
  '"@<fullwidth digit>" [perl #123963]';

fresh_perl_is(
  "s;\@{<<a;\n",
  "Can't find string terminator \"a\" anywhere before EOF at - line 1.\n",
   { stderr => 1 },
  's;@{<<a; [perl #123995]'
);
