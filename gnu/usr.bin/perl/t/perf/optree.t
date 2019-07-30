#!./perl

# Use B to test that optimisations are not inadvertently removed,
# by examining particular nodes in the optree.

BEGIN {
    chdir 't';
    require './test.pl';
    skip_all_if_miniperl("No B under miniperl");
    @INC = '../lib';
}

plan 54;

use v5.10; # state
use B qw(svref_2object
         OPpASSIGN_COMMON_SCALAR
         OPpASSIGN_COMMON_RC1
         OPpASSIGN_COMMON_AGG
      );


# Test that OP_AASSIGN gets the appropriate
# OPpASSIGN_COMMON* flags set.
#
# Too few flags set is likely to cause code to misbehave;
# too many flags set unnecessarily slows things down.
# See also the tests in t/op/aassign.t

for my $test (
    # Each anon array contains:
    # [
    #   expected flags:
    #      a 3 char string, each char showing whether we expect a
    #      particular flag to be set:
    #           '-' indicates any char not set, while
    #           'S':  char 0: OPpASSIGN_COMMON_SCALAR,
    #           'R':  char 1: OPpASSIGN_COMMON_RC1,
    #           'A'   char 2: OPpASSIGN_COMMON_AGG,
    #   code to eval,
    #   description,
    # ]

    [ "---", '() = (1, $x, my $y, @z, f($p))', 'no LHS' ],
    [ "---", '(undef, $x, my $y, @z, ($a ? $b : $c)) = ()', 'no RHS' ],
    [ "---", '(undef, $x, my $y, @z, ($a ? $b : $c)) = (1,2)', 'safe RHS' ],
    [ "---", 'my @a = (1,2)', 'safe RHS: my array' ],
    [ "---", 'my %h = (1,2)', 'safe RHS: my hash' ],
    [ "---", 'my ($a,$b,$c,$d) = 1..6; ($a,$b) = ($c,$d);', 'non-common lex' ],
    [ "---", '($x,$y) = (1,2)', 'pkg var LHS only' ],
    [ "---", 'my $p; my ($x,$y) = ($p, $p)', 'my; dup lex var on RHS' ],
    [ "---", 'my $p; my ($x,$y); ($x,$y) = ($p, $p)', 'dup lex var on RHS' ],
    [ "---", 'my ($self) = @_', 'LHS lex scalar only' ],
    [ "--A", 'my ($self, @rest) = @_', 'LHS lex mixed' ],
    [ "-R-", 'my ($x,$y) = ($p, $q)', 'pkg var RHS only' ],
    [ "S--", '($x,$y) = ($p, $q)', 'pkg scalar both sides' ],
    [ "--A", 'my (@a, @b); @a = @b', 'lex ary both sides' ],
    [ "-R-", 'my ($x,$y,$z,@a); ($x,$y,$z) = @a ', 'lex vars to lex ary' ],
    [ "--A", '@a = @b', 'pkg ary both sides' ],
    [ "--A", 'my (%a,%b); %a = %b', 'lex hash both sides' ],
    [ "--A", '%a = %b', 'pkg hash both sides' ],
    [ "--A", 'my $x; @a = ($a[0], $a[$x])', 'common ary' ],
    [ "--A", 'my ($x,@a); @a = ($a[0], $a[$x])', 'common lex ary' ],
    [ "S-A", 'my $x; ($a[$x], $a[0]) = ($a[0], $a[$x])', 'common ary elems' ],
    [ "S-A", 'my ($x,@a); ($a[$x], $a[0]) = ($a[0], $a[$x])',
                                                    'common lex ary elems' ],
    [ "--A", 'my $x; my @a = @$x', 'lex ary may have stuff' ],
    [ "-RA", 'my $x; my ($b, @a) = @$x', 'lex ary may have stuff' ],
    [ "--A", 'my $x; my %a = @$x', 'lex hash may have stuff' ],
    [ "-RA", 'my $x; my ($b, %a) = @$x', 'lex hash may have stuff' ],
    [ "--A", 'my (@a,@b); @a = ($b[0])', 'lex ary and elem' ],
    [ "S-A", 'my @a; ($a[1],$a[0]) = @a', 'lex ary and elem' ],
    [ "--A", 'my @x; @y = $x[0]', 'pkg ary from lex elem' ],
    [ "---", '(undef,$x) = f()', 'single scalar on LHS' ],
    [ "---", '($x,$y) = ($x)', 'single scalar on RHS, no AGG' ],
    [ "--A", '($x,@b) = ($x)', 'single scalar on RHS' ],
) {
    my ($exp, $code, $desc) = @$test;
    my $sub = eval "sub { $code }"
        or die
            "aassign eval('$code') failed: this test needs to be rewritten:\n"
            . $@;

    my $last_expr = svref_2object($sub)->ROOT->first->last;
    if ($last_expr->name ne 'aassign') {
        die "Expected aassign but found ", $last_expr->name,
            "; this test needs to be rewritten" 
    }
    my $got =
        (($last_expr->private & OPpASSIGN_COMMON_SCALAR) ? 'S' : '-')
      . (($last_expr->private & OPpASSIGN_COMMON_RC1)    ? 'R' : '-')
      . (($last_expr->private & OPpASSIGN_COMMON_AGG)    ? 'A' : '-');
    is $got, $exp,  "OPpASSIGN_COMMON: $desc: '$code'";
}    


# join -> stringify/const

for (['CONSTANT', sub {          join "foo", $_ }],
     ['$var'    , sub {          join  $_  , $_ }],
     ['$myvar'  , sub { my $var; join  $var, $_ }],
) {
    my($sep,$sub) = @$_;
    my $last_expr = svref_2object($sub)->ROOT->first->last;
    is $last_expr->name, 'stringify',
      "join($sep, \$scalar) optimised to stringify";
}

for (['CONSTANT', sub {          join "foo", "bar"    }, 0, "bar"    ],
     ['CONSTANT', sub {          join "foo", "bar", 3 }, 1, "barfoo3"],
     ['$var'    , sub {          join  $_  , "bar"    }, 0, "bar"    ],
     ['$myvar'  , sub { my $var; join  $var, "bar"    }, 0, "bar"    ],
) {
    my($sep,$sub,$is_list,$expect) = @$_;
    my $last_expr = svref_2object($sub)->ROOT->first->last;
    my $tn = "join($sep, " . ($is_list?'list of constants':'const') . ")";
    is $last_expr->name, 'const', "$tn optimised to constant";
    is $sub->(), $expect, "$tn folded correctly";
}


# list+pushmark in list context elided out of the execution chain
is svref_2object(sub { () = ($_, ($_, $_)) })
    ->START # nextstate
    ->next  # pushmark
    ->next  # gvsv
    ->next  # should be gvsv, not pushmark
  ->name, 'gvsv',
  "list+pushmark in list context where list's elder sibling is a null";


# nextstate multiple times becoming one nextstate

is svref_2object(sub { 0;0;0;0;0;0;time })->START->next->name, 'time',
  'multiple nextstates become one';


# pad[ahs]v state declarations in void context 

is svref_2object(sub{state($foo,@fit,%far);state $bar;state($a,$b); time})
    ->START->next->name, 'time',
  'pad[ahs]v state declarations in void context';


# pushmark-padsv-padav-padhv in list context --> padrange

{
    my @ops;
    my $sub = sub { \my( $f, @f, %f ) };
    my $op = svref_2object($sub)->START;
    push(@ops, $op->name), $op = $op->next while $$op;
    is "@ops", "nextstate padrange refgen leavesub", 'multi-type padrange'
}


# rv2[ahs]v in void context

is svref_2object(sub { our($foo,@fit,%far); our $bar; our($a,$b); time })
    ->START->next->name, 'time',
  'rv2[ahs]v in void context';


# split to array

for(['@pkgary'      , '@_'       ],
    ['@lexary'      , 'my @a; @a'],
    ['my(@array)'   , 'my(@a)'   ],
    ['local(@array)', 'local(@_)'],
    ['@{...}'       , '@{\@_}'   ],
){
    my($tn,$code) = @$_;
    my $sub = eval "sub { $code = split }";
    my $split = svref_2object($sub)->ROOT->first->last;
    is $split->name, 'split', "$tn = split swallows up the assignment";
}


# stringify with join kid --> join
is svref_2object(sub { "@_" })->ROOT->first->last->name, 'join',
  'qq"@_" optimised from stringify(join(...)) to join(...)';
