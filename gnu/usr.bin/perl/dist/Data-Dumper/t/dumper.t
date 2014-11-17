#!./perl -w
#
# testsuite for Data::Dumper
#

BEGIN {
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
	print "1..0 # Skip: Data::Dumper was not built\n";
	exit 0;
    }
}

# Since Perl 5.8.1 because otherwise hash ordering is really random.
local $Data::Dumper::Sortkeys = 1;

use Data::Dumper;
use Config;
my $Is_ebcdic = defined($Config{'ebcdic'}) && $Config{'ebcdic'} eq 'define';

$Data::Dumper::Pad = "#";
my $TMAX;
my $XS;
my $TNUM = 0;
my $WANT = '';

sub TEST {
  my $string = shift;
  my $name = shift;
  my $t = eval $string;
  ++$TNUM;
  $t =~ s/([A-Z]+)\(0x[0-9a-f]+\)/$1(0xdeadbeef)/g
    if ($WANT =~ /deadbeef/);
  if ($Is_ebcdic) {
    # these data need massaging with non ascii character sets
    # because of hashing order differences
    $WANT = join("\n",sort(split(/\n/,$WANT)));
    $WANT =~ s/\,$//mg;
    $t    = join("\n",sort(split(/\n/,$t)));
    $t    =~ s/\,$//mg;
  }
  $name = $name ? " - $name" : '';
  print( ($t eq $WANT and not $@) ? "ok $TNUM$name\n"
    : "not ok $TNUM$name\n--Expected--\n$WANT\n--Got--\n$@$t\n");

  ++$TNUM;
  if ($Is_ebcdic) { # EBCDIC.
    if ($TNUM == 311 || $TNUM == 314) {
      eval $string;
    } else {
      eval $t;
    }
  } else {
    eval "$t";
  }
  print $@ ? "not ok $TNUM\n# \$@ says: $@\n" : "ok $TNUM\n";

  $t = eval $string;
  ++$TNUM;
  $t =~ s/([A-Z]+)\(0x[0-9a-f]+\)/$1(0xdeadbeef)/g
    if ($WANT =~ /deadbeef/);
  if ($Is_ebcdic) {
    # here too there are hashing order differences
    $WANT = join("\n",sort(split(/\n/,$WANT)));
    $WANT =~ s/\,$//mg;
    $t    = join("\n",sort(split(/\n/,$t)));
    $t    =~ s/\,$//mg;
  }
  print( ($t eq $WANT and not $@) ? "ok $TNUM\n"
    : "not ok $TNUM\n--Expected--\n$WANT\n--Got--\n$@$t\n");
}

sub SKIP_TEST {
  my $reason = shift;
  ++$TNUM; print "ok $TNUM # skip $reason\n";
  ++$TNUM; print "ok $TNUM # skip $reason\n";
  ++$TNUM; print "ok $TNUM # skip $reason\n";
}

# Force Data::Dumper::Dump to use perl. We test Dumpxs explicitly by calling
# it direct. Out here it lets us knobble the next if to test that the perl
# only tests do work (and count correctly)
$Data::Dumper::Useperl = 1;
if (defined &Data::Dumper::Dumpxs) {
  print "### XS extension loaded, will run XS tests\n";
  $TMAX = 432; $XS = 1;
}
else {
  print "### XS extensions not loaded, will NOT run XS tests\n";
  $TMAX = 216; $XS = 0;
}

print "1..$TMAX\n";

#XXXif (0) {
#############
#############

@c = ('c');
$c = \@c;
$b = {};
$a = [1, $b, $c];
$b->{a} = $a;
$b->{b} = $a->[1];
$b->{c} = $a->[2];

############# 1
##
$WANT = <<'EOT';
#$a = [
#       1,
#       {
#         'a' => $a,
#         'b' => $a->[1],
#         'c' => [
#                  'c'
#                ]
#       },
#       $a->[1]{'c'}
#     ];
#$b = $a->[1];
#$6 = $a->[1]{'c'};
EOT

TEST (q(Data::Dumper->Dump([$a,$b,$c], [qw(a b), 6])),
    'basic test with names: Dump()');
TEST (q(Data::Dumper->Dumpxs([$a,$b,$c], [qw(a b), 6])),
    'basic test with names: Dumpxs()')
    if $XS;

SCOPE: {
    local $Data::Dumper::Sparseseen = 1;
    TEST (q(Data::Dumper->Dump([$a,$b,$c], [qw(a b), 6])),
        'Sparseseen with names: Dump()');
    TEST (q(Data::Dumper->Dumpxs([$a,$b,$c], [qw(a b), 6])),
        'Sparseseen with names: Dumpxs()')
        if $XS;
}


############# 7
##
$WANT = <<'EOT';
#@a = (
#       1,
#       {
#         'a' => [],
#         'b' => {},
#         'c' => [
#                  'c'
#                ]
#       },
#       []
#     );
#$a[1]{'a'} = \@a;
#$a[1]{'b'} = $a[1];
#$a[2] = $a[1]{'c'};
#$b = $a[1];
EOT

$Data::Dumper::Purity = 1;         # fill in the holes for eval
TEST (q(Data::Dumper->Dump([$a, $b], [qw(*a b)])),
    'Purity: basic test with dereferenced array: Dump()'); # print as @a
TEST (q(Data::Dumper->Dumpxs([$a, $b], [qw(*a b)])),
    'Purity: basic test with dereferenced array: Dumpxs()')
    if $XS;

SCOPE: {
  local $Data::Dumper::Sparseseen = 1;
  TEST (q(Data::Dumper->Dump([$a, $b], [qw(*a b)])),
    'Purity: Sparseseen with dereferenced array: Dump()'); # print as @a
  TEST (q(Data::Dumper->Dumpxs([$a, $b], [qw(*a b)])),
    'Purity: Sparseseen with dereferenced array: Dumpxs()')
    if $XS;
}

############# 13
##
$WANT = <<'EOT';
#%b = (
#       'a' => [
#                1,
#                {},
#                [
#                  'c'
#                ]
#              ],
#       'b' => {},
#       'c' => []
#     );
#$b{'a'}[1] = \%b;
#$b{'b'} = \%b;
#$b{'c'} = $b{'a'}[2];
#$a = $b{'a'};
EOT

TEST (q(Data::Dumper->Dump([$b, $a], [qw(*b a)])),
    'basic test with dereferenced hash: Dump()'); # print as %b
TEST (q(Data::Dumper->Dumpxs([$b, $a], [qw(*b a)])),
    'basic test with dereferenced hash: Dumpxs()')
    if $XS;

############# 19
##
$WANT = <<'EOT';
#$a = [
#  1,
#  {
#    'a' => [],
#    'b' => {},
#    'c' => []
#  },
#  []
#];
#$a->[1]{'a'} = $a;
#$a->[1]{'b'} = $a->[1];
#$a->[1]{'c'} = \@c;
#$a->[2] = \@c;
#$b = $a->[1];
EOT

$Data::Dumper::Indent = 1;
TEST (q(
       $d = Data::Dumper->new([$a,$b], [qw(a b)]);
       $d->Seen({'*c' => $c});
       $d->Dump;
      ),
      'Indent: Seen: Dump()');
if ($XS) {
  TEST (q(
	 $d = Data::Dumper->new([$a,$b], [qw(a b)]);
	 $d->Seen({'*c' => $c});
	 $d->Dumpxs;
     ),
      'Indent: Seen: Dumpxs()');
}


############# 25
##
$WANT = <<'EOT';
#$a = [
#       #0
#       1,
#       #1
#       {
#         a => $a,
#         b => $a->[1],
#         c => [
#                #0
#                'c'
#              ]
#       },
#       #2
#       $a->[1]{c}
#     ];
#$b = $a->[1];
EOT

$d->Indent(3);
$d->Purity(0)->Quotekeys(0);
TEST (q( $d->Reset; $d->Dump ),
    'Indent(3): Purity(0)->Quotekeys(0): Dump()');

TEST (q( $d->Reset; $d->Dumpxs ),
    'Indent(3): Purity(0)->Quotekeys(0): Dumpxs()')
    if $XS;

############# 31
##
$WANT = <<'EOT';
#$VAR1 = [
#  1,
#  {
#    'a' => [],
#    'b' => {},
#    'c' => [
#      'c'
#    ]
#  },
#  []
#];
#$VAR1->[1]{'a'} = $VAR1;
#$VAR1->[1]{'b'} = $VAR1->[1];
#$VAR1->[2] = $VAR1->[1]{'c'};
EOT

TEST (q(Dumper($a)), 'Dumper');
TEST (q(Data::Dumper::DumperX($a)), 'DumperX') if $XS;

############# 37
##
$WANT = <<'EOT';
#[
#  1,
#  {
#    a => $VAR1,
#    b => $VAR1->[1],
#    c => [
#      'c'
#    ]
#  },
#  $VAR1->[1]{c}
#]
EOT

{
  local $Data::Dumper::Purity = 0;
  local $Data::Dumper::Quotekeys = 0;
  local $Data::Dumper::Terse = 1;
  TEST (q(Dumper($a)),
    'Purity 0: Quotekeys 0: Terse 1: Dumper');
  TEST (q(Data::Dumper::DumperX($a)),
    'Purity 0: Quotekeys 0: Terse 1: DumperX')
    if $XS;
}


############# 43
##
$WANT = <<'EOT';
#$VAR1 = {
#  "abc\0'\efg" => "mno\0",
#  "reftest" => \\1
#};
EOT

$foo = { "abc\000\'\efg" => "mno\000",
         "reftest" => \\1,
       };
{
  local $Data::Dumper::Useqq = 1;
  TEST (q(Dumper($foo)), 'Useqq: Dumper');
  TEST (q(Data::Dumper::DumperX($foo)), 'Useqq: DumperX') if $XS;
}



#############
#############

{
  package main;
  use Data::Dumper;
  $foo = 5;
  @foo = (-10,\*foo);
  %foo = (a=>1,b=>\$foo,c=>\@foo);
  $foo{d} = \%foo;
  $foo[2] = \%foo;

############# 49
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#*::foo = \5;
#*::foo = [
#           #0
#           -10,
#           #1
#           do{my $o},
#           #2
#           {
#             'a' => 1,
#             'b' => do{my $o},
#             'c' => [],
#             'd' => {}
#           }
#         ];
#*::foo{ARRAY}->[1] = $foo;
#*::foo{ARRAY}->[2]{'b'} = *::foo{SCALAR};
#*::foo{ARRAY}->[2]{'c'} = *::foo{ARRAY};
#*::foo{ARRAY}->[2]{'d'} = *::foo{ARRAY}->[2];
#*::foo = *::foo{ARRAY}->[2];
#@bar = @{*::foo{ARRAY}};
#%baz = %{*::foo{ARRAY}->[2]};
EOT

  $Data::Dumper::Purity = 1;
  $Data::Dumper::Indent = 3;
  TEST (q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])),
    'Purity 1: Indent 3: Dump()');
  TEST (q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])),
    'Purity 1: Indent 3: Dumpxs()')
    if $XS;

############# 55
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#*::foo = \5;
#*::foo = [
#  -10,
#  do{my $o},
#  {
#    'a' => 1,
#    'b' => do{my $o},
#    'c' => [],
#    'd' => {}
#  }
#];
#*::foo{ARRAY}->[1] = $foo;
#*::foo{ARRAY}->[2]{'b'} = *::foo{SCALAR};
#*::foo{ARRAY}->[2]{'c'} = *::foo{ARRAY};
#*::foo{ARRAY}->[2]{'d'} = *::foo{ARRAY}->[2];
#*::foo = *::foo{ARRAY}->[2];
#$bar = *::foo{ARRAY};
#$baz = *::foo{ARRAY}->[2];
EOT

  $Data::Dumper::Indent = 1;
  TEST (q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])),
    'Purity 1: Indent 1: Dump()');
  TEST (q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])),
    'Purity 1: Indent 1: Dumpxs()')
    if $XS;

############# 61
##
  $WANT = <<'EOT';
#@bar = (
#  -10,
#  \*::foo,
#  {}
#);
#*::foo = \5;
#*::foo = \@bar;
#*::foo = {
#  'a' => 1,
#  'b' => do{my $o},
#  'c' => [],
#  'd' => {}
#};
#*::foo{HASH}->{'b'} = *::foo{SCALAR};
#*::foo{HASH}->{'c'} = \@bar;
#*::foo{HASH}->{'d'} = *::foo{HASH};
#$bar[2] = *::foo{HASH};
#%baz = %{*::foo{HASH}};
#$foo = $bar[1];
EOT

  TEST (q(Data::Dumper->Dump([\\@foo, \\%foo, \\*foo], ['*bar', '*baz', '*foo'])),
    'array|hash|glob dereferenced: Dump()');
  TEST (q(Data::Dumper->Dumpxs([\\@foo, \\%foo, \\*foo], ['*bar', '*baz', '*foo'])),
    'array|hash|glob dereferenced: Dumpxs()')
    if $XS;

############# 67
##
  $WANT = <<'EOT';
#$bar = [
#  -10,
#  \*::foo,
#  {}
#];
#*::foo = \5;
#*::foo = $bar;
#*::foo = {
#  'a' => 1,
#  'b' => do{my $o},
#  'c' => [],
#  'd' => {}
#};
#*::foo{HASH}->{'b'} = *::foo{SCALAR};
#*::foo{HASH}->{'c'} = $bar;
#*::foo{HASH}->{'d'} = *::foo{HASH};
#$bar->[2] = *::foo{HASH};
#$baz = *::foo{HASH};
#$foo = $bar->[1];
EOT

  TEST (q(Data::Dumper->Dump([\\@foo, \\%foo, \\*foo], ['bar', 'baz', 'foo'])),
    'array|hash|glob: not dereferenced: Dump()');
  TEST (q(Data::Dumper->Dumpxs([\\@foo, \\%foo, \\*foo], ['bar', 'baz', 'foo'])),
    'array|hash|glob: not dereferenced: Dumpxs()')
    if $XS;

############# 73
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#@bar = (
#  -10,
#  $foo,
#  {
#    a => 1,
#    b => \5,
#    c => \@bar,
#    d => $bar[2]
#  }
#);
#%baz = %{$bar[2]};
EOT

  $Data::Dumper::Purity = 0;
  $Data::Dumper::Quotekeys = 0;
  TEST (q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])),
    'Purity 0: Quotekeys 0: dereferenced: Dump()');
  TEST (q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])),
    'Purity 0: Quotekeys 0: dereferenced: Dumpxs')
    if $XS;

############# 79
##
  $WANT = <<'EOT';
#$foo = \*::foo;
#$bar = [
#  -10,
#  $foo,
#  {
#    a => 1,
#    b => \5,
#    c => $bar,
#    d => $bar->[2]
#  }
#];
#$baz = $bar->[2];
EOT

  TEST (q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])),
    'Purity 0: Quotekeys 0: not dereferenced: Dump()');
  TEST (q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])),
    'Purity 0: Quotekeys 0: not dereferenced: Dumpxs()')
    if $XS;

}

#############
#############
{
  package main;
  @dogs = ( 'Fido', 'Wags' );
  %kennel = (
            First => \$dogs[0],
            Second =>  \$dogs[1],
           );
  $dogs[2] = \%kennel;
  $mutts = \%kennel;
  $mutts = $mutts;         # avoid warning

############# 85
##
  $WANT = <<'EOT';
#%kennels = (
#  First => \'Fido',
#  Second => \'Wags'
#);
#@dogs = (
#  ${$kennels{First}},
#  ${$kennels{Second}},
#  \%kennels
#);
#%mutts = %kennels;
EOT

  TEST (q(
	 $d = Data::Dumper->new([\\%kennel, \\@dogs, $mutts],
				[qw(*kennels *dogs *mutts)] );
	 $d->Dump;
	),
    'constructor: hash|array|scalar: Dump()');
  if ($XS) {
    TEST (q(
	   $d = Data::Dumper->new([\\%kennel, \\@dogs, $mutts],
				  [qw(*kennels *dogs *mutts)] );
	   $d->Dumpxs;
	  ),
      'constructor: hash|array|scalar: Dumpxs()');
  }

############# 91
##
  $WANT = <<'EOT';
#%kennels = %kennels;
#@dogs = @dogs;
#%mutts = %kennels;
EOT

  TEST q($d->Dump), 'object call: Dump';
  TEST q($d->Dumpxs), 'object call: Dumpxs' if $XS;

############# 97
##
  $WANT = <<'EOT';
#%kennels = (
#  First => \'Fido',
#  Second => \'Wags'
#);
#@dogs = (
#  ${$kennels{First}},
#  ${$kennels{Second}},
#  \%kennels
#);
#%mutts = %kennels;
EOT

  TEST q($d->Reset; $d->Dump), 'Reset and Dump separate calls';
  if ($XS) {
    TEST (q($d->Reset; $d->Dumpxs), 'Reset and Dumpxs separate calls');
  }

############# 103
##
  $WANT = <<'EOT';
#@dogs = (
#  'Fido',
#  'Wags',
#  {
#    First => \$dogs[0],
#    Second => \$dogs[1]
#  }
#);
#%kennels = %{$dogs[2]};
#%mutts = %{$dogs[2]};
EOT

  TEST (q(
	 $d = Data::Dumper->new([\\@dogs, \\%kennel, $mutts],
				[qw(*dogs *kennels *mutts)] );
	 $d->Dump;
	),
    'constructor: array|hash|scalar: Dump()');
  if ($XS) {
    TEST (q(
	   $d = Data::Dumper->new([\\@dogs, \\%kennel, $mutts],
				  [qw(*dogs *kennels *mutts)] );
	   $d->Dumpxs;
	  ),
	'constructor: array|hash|scalar: Dumpxs()');
  }

############# 109
##
  TEST q($d->Reset->Dump), 'Reset Dump chained';
  if ($XS) {
    TEST q($d->Reset->Dumpxs), 'Reset Dumpxs chained';
  }

############# 115
##
  $WANT = <<'EOT';
#@dogs = (
#  'Fido',
#  'Wags',
#  {
#    First => \'Fido',
#    Second => \'Wags'
#  }
#);
#%kennels = (
#  First => \'Fido',
#  Second => \'Wags'
#);
EOT

  TEST (q(
	 $d = Data::Dumper->new( [\@dogs, \%kennel], [qw(*dogs *kennels)] );
	 $d->Deepcopy(1)->Dump;
	),
    'Deepcopy(1): Dump');
  if ($XS) {
#    TEST 'q($d->Reset->Dumpxs);
    TEST (q(
	    $d = Data::Dumper->new( [\@dogs, \%kennel], [qw(*dogs *kennels)] );
	    $d->Deepcopy(1)->Dumpxs;
    ),
    'Deepcopy(1): Dumpxs');
  }

}

{

sub z { print "foo\n" }
$c = [ \&z ];

############# 121
##
  $WANT = <<'EOT';
#$a = $b;
#$c = [
#  $b
#];
EOT

TEST (q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'b' => \&z})->Dump;),
    'Seen: scalar: Dump');
TEST (q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'b' => \&z})->Dumpxs;),
    'Seen: scalar: Dumpxs')
	if $XS;

############# 127
##
  $WANT = <<'EOT';
#$a = \&b;
#$c = [
#  \&b
#];
EOT

TEST (q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'*b' => \&z})->Dump;),
    'Seen: glob: Dump');
TEST (q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'*b' => \&z})->Dumpxs;),
    'Seen: glob: Dumpxs')
	if $XS;

############# 133
##
  $WANT = <<'EOT';
#*a = \&b;
#@c = (
#  \&b
#);
EOT

TEST (q(Data::Dumper->new([\&z,$c],['*a','*c'])->Seen({'*b' => \&z})->Dump;),
    'Seen: glob: dereference: Dump');
TEST (q(Data::Dumper->new([\&z,$c],['*a','*c'])->Seen({'*b' =>
\&z})->Dumpxs;),
    'Seen: glob: derference: Dumpxs')
	if $XS;

}

{
  $a = [];
  $a->[1] = \$a->[0];

############# 139
##
  $WANT = <<'EOT';
#@a = (
#  undef,
#  do{my $o}
#);
#$a[1] = \$a[0];
EOT

TEST (q(Data::Dumper->new([$a],['*a'])->Purity(1)->Dump;),
    'Purity(1): dereference: Dump');
TEST (q(Data::Dumper->new([$a],['*a'])->Purity(1)->Dumpxs;),
    'Purity(1): dereference: Dumpxs')
	if $XS;
}

{
  $a = \\\\\'foo';
  $b = $$$a;

############# 145
##
  $WANT = <<'EOT';
#$a = \\\\\'foo';
#$b = ${${$a}};
EOT

TEST (q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dump;),
    'Purity(1): not dereferenced: Dump');
TEST (q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dumpxs;),
    'Purity(1): not dereferenced: Dumpxs')
	if $XS;
}

{
  $a = [{ a => \$b }, { b => undef }];
  $b = [{ c => \$b }, { d => \$a }];

############# 151
##
  $WANT = <<'EOT';
#$a = [
#  {
#    a => \[
#        {
#          c => do{my $o}
#        },
#        {
#          d => \[]
#        }
#      ]
#  },
#  {
#    b => undef
#  }
#];
#${$a->[0]{a}}->[0]->{c} = $a->[0]{a};
#${${$a->[0]{a}}->[1]->{d}} = $a;
#$b = ${$a->[0]{a}};
EOT

TEST (q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dump;),
    'Purity(1): Dump again');
TEST (q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dumpxs;),
    'Purity(1); Dumpxs again')
	if $XS;
}

{
  $a = [[[[\\\\\'foo']]]];
  $b = $a->[0][0];
  $c = $${$b->[0][0]};

############# 157
##
  $WANT = <<'EOT';
#$a = [
#  [
#    [
#      [
#        \\\\\'foo'
#      ]
#    ]
#  ]
#];
#$b = $a->[0][0];
#$c = ${${$a->[0][0][0][0]}};
EOT

TEST (q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Purity(1)->Dump;),
    'Purity(1): Dump: 3 elements');
TEST (q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Purity(1)->Dumpxs;),
    'Purity(1): Dumpxs: 3 elements')
	if $XS;
}

{
    $f = "pearl";
    $e = [        $f ];
    $d = { 'e' => $e };
    $c = [        $d ];
    $b = { 'c' => $c };
    $a = { 'b' => $b };

############# 163
##
  $WANT = <<'EOT';
#$a = {
#  b => {
#    c => [
#      {
#        e => 'ARRAY(0xdeadbeef)'
#      }
#    ]
#  }
#};
#$b = $a->{b};
#$c = $a->{b}{c};
EOT

TEST (q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(4)->Dump;),
    'Maxdepth(4): Dump()');
TEST (q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(4)->Dumpxs;),
    'Maxdepth(4): Dumpxs()')
	if $XS;

############# 169
##
  $WANT = <<'EOT';
#$a = {
#  b => 'HASH(0xdeadbeef)'
#};
#$b = $a->{b};
#$c = [
#  'HASH(0xdeadbeef)'
#];
EOT

TEST (q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(1)->Dump;),
    'Maxdepth(1): Dump()');
TEST (q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(1)->Dumpxs;),
    'Maxdepth(1): Dumpxs()')
	if $XS;
}

{
    $a = \$a;
    $b = [$a];

############# 175
##
  $WANT = <<'EOT';
#$b = [
#  \$b->[0]
#];
EOT

TEST (q(Data::Dumper->new([$b],['b'])->Purity(0)->Dump;),
    'Purity(0): Dump()');
TEST (q(Data::Dumper->new([$b],['b'])->Purity(0)->Dumpxs;),
    'Purity(0): Dumpxs()')
	if $XS;

############# 181
##
  $WANT = <<'EOT';
#$b = [
#  \do{my $o}
#];
#${$b->[0]} = $b->[0];
EOT


TEST (q(Data::Dumper->new([$b],['b'])->Purity(1)->Dump;),
    'Purity(1): Dump()');
TEST (q(Data::Dumper->new([$b],['b'])->Purity(1)->Dumpxs;),
    'Purity(1): Dumpxs')
	if $XS;
}

{
  $a = "\x{09c10}";
############# 187
## XS code was adding an extra \0
  $WANT = <<'EOT';
#$a = "\x{9c10}";
EOT

  if($] >= 5.007) {
    TEST q(Data::Dumper->Dump([$a], ['a'])), "\\x{9c10}";
  } else {
    SKIP_TEST "Incomplete support for UTF-8 in old perls";
  }
  TEST q(Data::Dumper->Dumpxs([$a], ['a'])), "XS \\x{9c10}"
	if $XS;
}

{
  $i = 0;
  $a = { map { ("$_$_$_", ++$i) } 'I'..'Q' };

############# 193
##
  $WANT = <<'EOT';
#$VAR1 = {
#  III => 1,
#  JJJ => 2,
#  KKK => 3,
#  LLL => 4,
#  MMM => 5,
#  NNN => 6,
#  OOO => 7,
#  PPP => 8,
#  QQQ => 9
#};
EOT

TEST (q(Data::Dumper->new([$a])->Dump;),
    'basic test without names: Dump()');
TEST (q(Data::Dumper->new([$a])->Dumpxs;),
    'basic test without names: Dumpxs()')
	if $XS;
}

{
  $i = 5;
  $c = { map { (++$i, "$_$_$_") } 'I'..'Q' };
  local $Data::Dumper::Sortkeys = \&sort199;
  sub sort199 {
    my $hash = shift;
    return [ sort { $b <=> $a } keys %$hash ];
  }

############# 199
##
  $WANT = <<'EOT';
#$VAR1 = {
#  14 => 'QQQ',
#  13 => 'PPP',
#  12 => 'OOO',
#  11 => 'NNN',
#  10 => 'MMM',
#  9 => 'LLL',
#  8 => 'KKK',
#  7 => 'JJJ',
#  6 => 'III'
#};
EOT

TEST q(Data::Dumper->new([$c])->Dump;), "sortkeys sub";
TEST q(Data::Dumper->new([$c])->Dumpxs;), "sortkeys sub (XS)"
	if $XS;
}

{
  $i = 5;
  $c = { map { (++$i, "$_$_$_") } 'I'..'Q' };
  $d = { reverse %$c };
  local $Data::Dumper::Sortkeys = \&sort205;
  sub sort205 {
    my $hash = shift;
    return [
      $hash eq $c ? (sort { $a <=> $b } keys %$hash)
		  : (reverse sort keys %$hash)
    ];
  }

############# 205
##
  $WANT = <<'EOT';
#$VAR1 = [
#  {
#    6 => 'III',
#    7 => 'JJJ',
#    8 => 'KKK',
#    9 => 'LLL',
#    10 => 'MMM',
#    11 => 'NNN',
#    12 => 'OOO',
#    13 => 'PPP',
#    14 => 'QQQ'
#  },
#  {
#    QQQ => 14,
#    PPP => 13,
#    OOO => 12,
#    NNN => 11,
#    MMM => 10,
#    LLL => 9,
#    KKK => 8,
#    JJJ => 7,
#    III => 6
#  }
#];
EOT

TEST q(Data::Dumper->new([[$c, $d]])->Dump;), "more sortkeys sub";
# the XS code does number values as strings
$WANT =~ s/ (\d+)(,?)$/ '$1'$2/gm;
TEST q(Data::Dumper->new([[$c, $d]])->Dumpxs;), "more sortkeys sub (XS)"
	if $XS;
}

{
  local $Data::Dumper::Deparse = 1;
  local $Data::Dumper::Indent = 2;

############# 211
##
  $WANT = <<'EOT';
#$VAR1 = {
#          foo => sub {
#                     print 'foo';
#                 }
#        };
EOT

  if(" $Config{'extensions'} " !~ m[ B ]) {
    SKIP_TEST "Perl configured without B module";
  } else {
    TEST (q(Data::Dumper->new([{ foo => sub { print "foo"; } }])->Dump),
        'Deparse 1: Indent 2; Dump()');
  }
}

############# 214
##

# This is messy.
# The controls (bare numbers) are stored either as integers or floating point.
# [depending on whether the tokeniser sees things like ".".
# The peephole optimiser only runs for constant folding, not single constants,
# so I already have some NVs, some IVs
# The string versions are not. They are all PV

# This is arguably all far too chummy with the implementation, but I really
# want to ensure that we don't go wrong when flags on scalars get as side
# effects of reading them.

# These tests are actually testing the precise output of the current
# implementation, so will most likely fail if the implementation changes,
# even if the new implementation produces different but correct results.
# It would be nice to test for wrong answers, but I can't see how to do that,
# so instead I'm checking for unexpected answers. (ie -2 becoming "-2" is not
# wrong, but I can't see an easy, reliable way to code that knowledge)

# Numbers (seen by the tokeniser as numbers, stored as numbers.
  @numbers =
  (
   0, +1, -2, 3.0, +4.0, -5.0, 6.5, +7.5, -8.5,
    9,  +10,  -11,  12.0,  +13.0,  -14.0,  15.5,  +16.25,  -17.75,
  );
# Strings
  @strings =
  (
   "0", "+1", "-2", "3.0", "+4.0", "-5.0", "6.5", "+7.5", "-8.5", " 9",
   " +10", " -11", " 12.0", " +13.0", " -14.0", " 15.5", " +16.25", " -17.75",
  );

# The perl code always does things the same way for numbers.
  $WANT_PL_N = <<'EOT';
#$VAR1 = 0;
#$VAR2 = 1;
#$VAR3 = -2;
#$VAR4 = 3;
#$VAR5 = 4;
#$VAR6 = -5;
#$VAR7 = '6.5';
#$VAR8 = '7.5';
#$VAR9 = '-8.5';
#$VAR10 = 9;
#$VAR11 = 10;
#$VAR12 = -11;
#$VAR13 = 12;
#$VAR14 = 13;
#$VAR15 = -14;
#$VAR16 = '15.5';
#$VAR17 = '16.25';
#$VAR18 = '-17.75';
EOT
# The perl code knows that 0 and -2 stringify exactly back to the strings,
# so it dumps them as numbers, not strings.
  $WANT_PL_S = <<'EOT';
#$VAR1 = 0;
#$VAR2 = '+1';
#$VAR3 = -2;
#$VAR4 = '3.0';
#$VAR5 = '+4.0';
#$VAR6 = '-5.0';
#$VAR7 = '6.5';
#$VAR8 = '+7.5';
#$VAR9 = '-8.5';
#$VAR10 = ' 9';
#$VAR11 = ' +10';
#$VAR12 = ' -11';
#$VAR13 = ' 12.0';
#$VAR14 = ' +13.0';
#$VAR15 = ' -14.0';
#$VAR16 = ' 15.5';
#$VAR17 = ' +16.25';
#$VAR18 = ' -17.75';
EOT

# The XS code differs.
# These are the numbers as seen by the tokeniser. Constants aren't folded
# (which makes IVs where possible) so values the tokeniser thought were
# floating point are stored as NVs. The XS code outputs these as strings,
# but as it has converted them from NVs, leading + signs will not be there.
  $WANT_XS_N = <<'EOT';
#$VAR1 = 0;
#$VAR2 = 1;
#$VAR3 = -2;
#$VAR4 = '3';
#$VAR5 = '4';
#$VAR6 = '-5';
#$VAR7 = '6.5';
#$VAR8 = '7.5';
#$VAR9 = '-8.5';
#$VAR10 = 9;
#$VAR11 = 10;
#$VAR12 = -11;
#$VAR13 = '12';
#$VAR14 = '13';
#$VAR15 = '-14';
#$VAR16 = '15.5';
#$VAR17 = '16.25';
#$VAR18 = '-17.75';
EOT

# These are the strings as seen by the tokeniser. The XS code will output
# these for all cases except where the scalar has been used in integer context
  $WANT_XS_S = <<'EOT';
#$VAR1 = '0';
#$VAR2 = '+1';
#$VAR3 = '-2';
#$VAR4 = '3.0';
#$VAR5 = '+4.0';
#$VAR6 = '-5.0';
#$VAR7 = '6.5';
#$VAR8 = '+7.5';
#$VAR9 = '-8.5';
#$VAR10 = ' 9';
#$VAR11 = ' +10';
#$VAR12 = ' -11';
#$VAR13 = ' 12.0';
#$VAR14 = ' +13.0';
#$VAR15 = ' -14.0';
#$VAR16 = ' 15.5';
#$VAR17 = ' +16.25';
#$VAR18 = ' -17.75';
EOT

# These are the numbers as IV-ized by &
# These will differ from WANT_XS_N because now IV flags will be set on all
# values that were actually integer, and the XS code will then output these
# as numbers not strings.
  $WANT_XS_I = <<'EOT';
#$VAR1 = 0;
#$VAR2 = 1;
#$VAR3 = -2;
#$VAR4 = 3;
#$VAR5 = 4;
#$VAR6 = -5;
#$VAR7 = '6.5';
#$VAR8 = '7.5';
#$VAR9 = '-8.5';
#$VAR10 = 9;
#$VAR11 = 10;
#$VAR12 = -11;
#$VAR13 = 12;
#$VAR14 = 13;
#$VAR15 = -14;
#$VAR16 = '15.5';
#$VAR17 = '16.25';
#$VAR18 = '-17.75';
EOT

# Some of these tests will be redundant.
@numbers_s = @numbers_i = @numbers_is = @numbers_n = @numbers_ns = @numbers_ni
  = @numbers_nis = @numbers;
@strings_s = @strings_i = @strings_is = @strings_n = @strings_ns = @strings_ni
  = @strings_nis = @strings;
# Use them in an integer context
foreach (@numbers_i, @numbers_ni, @numbers_nis, @numbers_is,
         @strings_i, @strings_ni, @strings_nis, @strings_is) {
  my $b = sprintf "%d", $_;
}
# Use them in a floating point context
foreach (@numbers_n, @numbers_ni, @numbers_nis, @numbers_ns,
         @strings_n, @strings_ni, @strings_nis, @strings_ns) {
  my $b = sprintf "%e", $_;
}
# Use them in a string context
foreach (@numbers_s, @numbers_is, @numbers_nis, @numbers_ns,
         @strings_s, @strings_is, @strings_nis, @strings_ns) {
  my $b = sprintf "%s", $_;
}

# use Devel::Peek; Dump ($_) foreach @vanilla_c;

$WANT=$WANT_PL_N;
TEST q(Data::Dumper->new(\@numbers)->Dump), 'Numbers';
TEST q(Data::Dumper->new(\@numbers_s)->Dump), 'Numbers PV';
TEST q(Data::Dumper->new(\@numbers_i)->Dump), 'Numbers IV';
TEST q(Data::Dumper->new(\@numbers_is)->Dump), 'Numbers IV,PV';
TEST q(Data::Dumper->new(\@numbers_n)->Dump), 'Numbers NV';
TEST q(Data::Dumper->new(\@numbers_ns)->Dump), 'Numbers NV,PV';
TEST q(Data::Dumper->new(\@numbers_ni)->Dump), 'Numbers NV,IV';
TEST q(Data::Dumper->new(\@numbers_nis)->Dump), 'Numbers NV,IV,PV';
$WANT=$WANT_PL_S;
TEST q(Data::Dumper->new(\@strings)->Dump), 'Strings';
TEST q(Data::Dumper->new(\@strings_s)->Dump), 'Strings PV';
TEST q(Data::Dumper->new(\@strings_i)->Dump), 'Strings IV';
TEST q(Data::Dumper->new(\@strings_is)->Dump), 'Strings IV,PV';
TEST q(Data::Dumper->new(\@strings_n)->Dump), 'Strings NV';
TEST q(Data::Dumper->new(\@strings_ns)->Dump), 'Strings NV,PV';
TEST q(Data::Dumper->new(\@strings_ni)->Dump), 'Strings NV,IV';
TEST q(Data::Dumper->new(\@strings_nis)->Dump), 'Strings NV,IV,PV';
if ($XS) {
 my $nv_preserves_uv = defined $Config{d_nv_preserves_uv};
 my $nv_preserves_uv_4bits = exists($Config{nv_preserves_uv_bits}) && $Config{nv_preserves_uv_bits} >= 4;
  $WANT=$WANT_XS_N;
  TEST q(Data::Dumper->new(\@numbers)->Dumpxs), 'XS Numbers';
  TEST q(Data::Dumper->new(\@numbers_s)->Dumpxs), 'XS Numbers PV';
 if ($nv_preserves_uv || $nv_preserves_uv_4bits) {
  $WANT=$WANT_XS_I;
  TEST q(Data::Dumper->new(\@numbers_i)->Dumpxs), 'XS Numbers IV';
  TEST q(Data::Dumper->new(\@numbers_is)->Dumpxs), 'XS Numbers IV,PV';
 } else {
  SKIP_TEST "NV does not preserve 4bits";
  SKIP_TEST "NV does not preserve 4bits";
 }
  $WANT=$WANT_XS_N;
  TEST q(Data::Dumper->new(\@numbers_n)->Dumpxs), 'XS Numbers NV';
  TEST q(Data::Dumper->new(\@numbers_ns)->Dumpxs), 'XS Numbers NV,PV';
 if ($nv_preserves_uv || $nv_preserves_uv_4bits) {
  $WANT=$WANT_XS_I;
  TEST q(Data::Dumper->new(\@numbers_ni)->Dumpxs), 'XS Numbers NV,IV';
  TEST q(Data::Dumper->new(\@numbers_nis)->Dumpxs), 'XS Numbers NV,IV,PV';
 } else {
  SKIP_TEST "NV does not preserve 4bits";
  SKIP_TEST "NV does not preserve 4bits";
 }

  $WANT=$WANT_XS_S;
  TEST q(Data::Dumper->new(\@strings)->Dumpxs), 'XS Strings';
  TEST q(Data::Dumper->new(\@strings_s)->Dumpxs), 'XS Strings PV';
  # This one used to really mess up. New code actually emulates the .pm code
  $WANT=$WANT_PL_S;
  TEST q(Data::Dumper->new(\@strings_i)->Dumpxs), 'XS Strings IV';
  TEST q(Data::Dumper->new(\@strings_is)->Dumpxs), 'XS Strings IV,PV';
 if ($nv_preserves_uv || $nv_preserves_uv_4bits) {
  $WANT=$WANT_XS_S;
  TEST q(Data::Dumper->new(\@strings_n)->Dumpxs), 'XS Strings NV';
  TEST q(Data::Dumper->new(\@strings_ns)->Dumpxs), 'XS Strings NV,PV';
 } else {
  SKIP_TEST "NV does not preserve 4bits";
  SKIP_TEST "NV does not preserve 4bits";
 }
  # This one used to really mess up. New code actually emulates the .pm code
  $WANT=$WANT_PL_S;
  TEST q(Data::Dumper->new(\@strings_ni)->Dumpxs), 'XS Strings NV,IV';
  TEST q(Data::Dumper->new(\@strings_nis)->Dumpxs), 'XS Strings NV,IV,PV';
}

{
  $a = "1\n";
############# 310
## Perl code was using /...$/ and hence missing the \n.
  $WANT = <<'EOT';
my $VAR1 = '42
';
EOT

  # Can't pad with # as the output has an embedded newline.
  local $Data::Dumper::Pad = "my ";
  TEST q(Data::Dumper->Dump(["42\n"])), "number with trailing newline";
  TEST q(Data::Dumper->Dumpxs(["42\n"])), "XS number with trailing newline"
	if $XS;
}

{
  @a = (
        999999999,
        1000000000,
        9999999999,
        10000000000,
        -999999999,
        -1000000000,
        -9999999999,
        -10000000000,
        4294967295,
        4294967296,
        -2147483648,
        -2147483649,
        );
############# 316
## Perl code flips over at 10 digits.
  $WANT = <<'EOT';
#$VAR1 = 999999999;
#$VAR2 = '1000000000';
#$VAR3 = '9999999999';
#$VAR4 = '10000000000';
#$VAR5 = -999999999;
#$VAR6 = '-1000000000';
#$VAR7 = '-9999999999';
#$VAR8 = '-10000000000';
#$VAR9 = '4294967295';
#$VAR10 = '4294967296';
#$VAR11 = '-2147483648';
#$VAR12 = '-2147483649';
EOT

  TEST q(Data::Dumper->Dump(\@a)), "long integers";

  if ($XS) {
## XS code flips over at 11 characters ("-" is a char) or larger than int.
    if (~0 == 0xFFFFFFFF) {
      # 32 bit system
      $WANT = <<'EOT';
#$VAR1 = 999999999;
#$VAR2 = 1000000000;
#$VAR3 = '9999999999';
#$VAR4 = '10000000000';
#$VAR5 = -999999999;
#$VAR6 = '-1000000000';
#$VAR7 = '-9999999999';
#$VAR8 = '-10000000000';
#$VAR9 = 4294967295;
#$VAR10 = '4294967296';
#$VAR11 = '-2147483648';
#$VAR12 = '-2147483649';
EOT
    } else {
      $WANT = <<'EOT';
#$VAR1 = 999999999;
#$VAR2 = 1000000000;
#$VAR3 = 9999999999;
#$VAR4 = '10000000000';
#$VAR5 = -999999999;
#$VAR6 = '-1000000000';
#$VAR7 = '-9999999999';
#$VAR8 = '-10000000000';
#$VAR9 = 4294967295;
#$VAR10 = 4294967296;
#$VAR11 = '-2147483648';
#$VAR12 = '-2147483649';
EOT
    }
    TEST q(Data::Dumper->Dumpxs(\@a)), "XS long integers";
  }
}

#XXX}
{
    if ($Is_ebcdic) {
	$b = "Bad. XS didn't escape dollar sign";
############# 322
	$WANT = <<"EOT"; # Careful. This is '' string written inside '' here doc
#\$VAR1 = '\$b\"\@\\\\\xB1';
EOT
        $a = "\$b\"\@\\\xB1\x{100}";
	chop $a;
	TEST q(Data::Dumper->Dump([$a])), "utf8 flag with \" and \$";
	if ($XS) {
	    $WANT = <<'EOT'; # While this is "" string written inside "" here doc
#$VAR1 = "\$b\"\@\\\x{b1}";
EOT
            TEST q(Data::Dumper->Dumpxs([$a])), "XS utf8 flag with \" and \$";
	}
    } else {
	$b = "Bad. XS didn't escape dollar sign";
############# 322
	$WANT = <<"EOT"; # Careful. This is '' string written inside '' here doc
#\$VAR1 = '\$b\"\@\\\\\xA3';
EOT

        $a = "\$b\"\@\\\xA3\x{100}";
	chop $a;
	TEST q(Data::Dumper->Dump([$a])), "utf8 flag with \" and \$";
	if ($XS) {
	    $WANT = <<'EOT'; # While this is "" string written inside "" here doc
#$VAR1 = "\$b\"\@\\\x{a3}";
EOT
            TEST q(Data::Dumper->Dumpxs([$a])), "XS utf8 flag with \" and \$";
	}
  }
  # XS used to produce "$b\"' which is 4 chars, not 3. [ie wrongly qq(\$b\\\")]
############# 328
  $WANT = <<'EOT';
#$VAR1 = '$b"';
EOT

  $a = "\$b\"\x{100}";
  chop $a;
  TEST q(Data::Dumper->Dump([$a])), "utf8 flag with \" and \$";
  if ($XS) {
    TEST q(Data::Dumper->Dumpxs([$a])), "XS utf8 flag with \" and \$";
  }


  # XS used to produce 'D'oh!' which is well, D'oh!
  # Andreas found this one, which in turn discovered the previous two.
############# 334
  $WANT = <<'EOT';
#$VAR1 = 'D\'oh!';
EOT

  $a = "D'oh!\x{100}";
  chop $a;
  TEST q(Data::Dumper->Dump([$a])), "utf8 flag with '";
  if ($XS) {
    TEST q(Data::Dumper->Dumpxs([$a])), "XS utf8 flag with '";
  }
}

# Jarkko found that -Mutf8 caused some tests to fail.  Turns out that there
# was an otherwise untested code path in the XS for utf8 hash keys with purity
# 1

{
  $WANT = <<'EOT';
#$ping = \*::ping;
#*::ping = \5;
#*::ping = {
#  "\x{decaf}\x{decaf}\x{decaf}\x{decaf}" => do{my $o}
#};
#*::ping{HASH}->{"\x{decaf}\x{decaf}\x{decaf}\x{decaf}"} = *::ping{SCALAR};
#%pong = %{*::ping{HASH}};
EOT
  local $Data::Dumper::Purity = 1;
  local $Data::Dumper::Sortkeys;
  $ping = 5;
  %ping = (chr (0xDECAF) x 4  =>\$ping);
  for $Data::Dumper::Sortkeys (0, 1) {
    if($] >= 5.007) {
      TEST (q(Data::Dumper->Dump([\\*ping, \\%ping], ['*ping', '*pong'])),
        "utf8: Purity 1: Sortkeys: Dump()");
      TEST (q(Data::Dumper->Dumpxs([\\*ping, \\%ping], ['*ping', '*pong'])),
        "utf8: Purity 1: Sortkeys: Dumpxs()")
        if $XS;
    } else {
      SKIP_TEST "Incomplete support for UTF-8 in old perls";
      SKIP_TEST "Incomplete support for UTF-8 in old perls";
    }
  }
}

# XS for quotekeys==0 was not being defensive enough against utf8 flagged
# scalars

{
  $WANT = <<'EOT';
#$VAR1 = {
#  perl => 'rocks'
#};
EOT
  local $Data::Dumper::Quotekeys = 0;
  my $k = 'perl' . chr 256;
  chop $k;
  %foo = ($k => 'rocks');

  TEST q(Data::Dumper->Dump([\\%foo])), "quotekeys == 0 for utf8 flagged ASCII";
  TEST q(Data::Dumper->Dumpxs([\\%foo])),
    "XS quotekeys == 0 for utf8 flagged ASCII" if $XS;
}
############# 358
{
  $WANT = <<'EOT';
#$VAR1 = [
#  undef,
#  undef,
#  1
#];
EOT
    @foo = ();
    $foo[2] = 1;
    TEST q(Data::Dumper->Dump([\@foo])), 'Richard Clamp, Message-Id: <20030104005247.GA27685@mirth.demon.co.uk>: Dump()';
    TEST q(Data::Dumper->Dumpxs([\@foo])), 'Richard Clamp, Message-Id: <20030104005247.GA27685@mirth.demon.co.uk>: Dumpxs()'if $XS;
}

############# 364
# Make sure $obj->Dumpxs returns the right thing in list context. This was
# broken by the initial attempt to fix [perl #74170].
$WANT = <<'EOT';
#$VAR1 = [];
EOT
TEST q(join " ", new Data::Dumper [[]],[] =>->Dumpxs),
    '$obj->Dumpxs in list context'
 if $XS;

############# 366
{
  $WANT = <<'EOT';
#$VAR1 = [
#  "\0\1\2\3\4\5\6\a\b\t\n\13\f\r\16\17\20\21\22\23\24\25\26\27\30\31\32\e\34\35\36\37 !\"#\$%&'()*+,-./0123456789:;<=>?\@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\177\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377"
#];
EOT

  $foo = [ join "", map chr, 0..255 ];
  local $Data::Dumper::Useqq = 1;
  TEST (q(Dumper($foo)), 'All latin1 characters: Dumper');
  TEST (q(Data::Dumper::DumperX($foo)), 'All latin1 characters: DumperX') if $XS;
}

############# 372
{
  $WANT = <<'EOT';
#$VAR1 = [
#  "\0\1\2\3\4\5\6\a\b\t\n\13\f\r\16\17\20\21\22\23\24\25\26\27\30\31\32\e\34\35\36\37 !\"#\$%&'()*+,-./0123456789:;<=>?\@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\177\x{80}\x{81}\x{82}\x{83}\x{84}\x{85}\x{86}\x{87}\x{88}\x{89}\x{8a}\x{8b}\x{8c}\x{8d}\x{8e}\x{8f}\x{90}\x{91}\x{92}\x{93}\x{94}\x{95}\x{96}\x{97}\x{98}\x{99}\x{9a}\x{9b}\x{9c}\x{9d}\x{9e}\x{9f}\x{a0}\x{a1}\x{a2}\x{a3}\x{a4}\x{a5}\x{a6}\x{a7}\x{a8}\x{a9}\x{aa}\x{ab}\x{ac}\x{ad}\x{ae}\x{af}\x{b0}\x{b1}\x{b2}\x{b3}\x{b4}\x{b5}\x{b6}\x{b7}\x{b8}\x{b9}\x{ba}\x{bb}\x{bc}\x{bd}\x{be}\x{bf}\x{c0}\x{c1}\x{c2}\x{c3}\x{c4}\x{c5}\x{c6}\x{c7}\x{c8}\x{c9}\x{ca}\x{cb}\x{cc}\x{cd}\x{ce}\x{cf}\x{d0}\x{d1}\x{d2}\x{d3}\x{d4}\x{d5}\x{d6}\x{d7}\x{d8}\x{d9}\x{da}\x{db}\x{dc}\x{dd}\x{de}\x{df}\x{e0}\x{e1}\x{e2}\x{e3}\x{e4}\x{e5}\x{e6}\x{e7}\x{e8}\x{e9}\x{ea}\x{eb}\x{ec}\x{ed}\x{ee}\x{ef}\x{f0}\x{f1}\x{f2}\x{f3}\x{f4}\x{f5}\x{f6}\x{f7}\x{f8}\x{f9}\x{fa}\x{fb}\x{fc}\x{fd}\x{fe}\x{ff}\x{20ac}"
#];
EOT

  $foo = [ join "", map chr, 0..255, 0x20ac ];
  local $Data::Dumper::Useqq = 1;
  if ($] < 5.007) {
    print "not ok " . (++$TNUM) . " # TODO - fails under 5.6\n" for 1..3;
  }
  else {
    TEST q(Dumper($foo)),
	 'All latin1 characters with utf8 flag including a wide character: Dumper';
  }
  TEST (q(Data::Dumper::DumperX($foo)),
    'All latin1 characters with utf8 flag including a wide character: DumperX')
    if $XS;
}

############# 378
{
  # If XS cannot load, the pure-Perl version cannot deparse vstrings with
  # underscores properly.  In 5.8.0, vstrings are just strings.
  my $no_vstrings = <<'NOVSTRINGS';
#$a = \'ABC';
#$b = \'ABC';
#$c = \'ABC';
#$d = \'ABC';
NOVSTRINGS
  my $vstrings_corr = <<'VSTRINGS_CORRECT';
#$a = \v65.66.67;
#$b = \v65.66.067;
#$c = \v65.66.6_7;
#$d = \'ABC';
VSTRINGS_CORRECT
  $WANT = $] <= 5.0080001
          ? $no_vstrings
          : $vstrings_corr;

  @::_v = (
    \v65.66.67,
    \($] < 5.007 ? v65.66.67 : eval 'v65.66.067'),
    \v65.66.6_7,
    \~v190.189.188
  );
  if ($] >= 5.010) {
    TEST q(Data::Dumper->Dump(\@::_v, [qw(a b c d)])), 'vstrings';
    TEST q(Data::Dumper->Dumpxs(\@::_v, [qw(a b c d)])), 'xs vstrings'
      if $XS;
  }
  else { # Skip tests before 5.10. vstrings considered funny before
    SKIP_TEST "vstrings considered funny before 5.10.0";
    SKIP_TEST "vstrings considered funny before 5.10.0 (XS)"
      if $XS;
  }
}

############# 384
{
  # [perl #107372] blessed overloaded globs
  $WANT = <<'EOW';
#$VAR1 = bless( \*::finkle, 'overtest' );
EOW
  {
    package overtest;
    use overload fallback=>1, q\""\=>sub{"oaoaa"};
  }
  TEST q(Data::Dumper->Dump([bless \*finkle, "overtest"])),
    'blessed overloaded globs';
  TEST q(Data::Dumper->Dumpxs([\*finkle])), 'blessed overloaded globs (xs)'
    if $XS;
}
############# 390
{
  # [perl #74798] uncovered behaviour
  $WANT = <<'EOW';
#$VAR1 = "\0000";
EOW
  local $Data::Dumper::Useqq = 1;
  TEST q(Data::Dumper->Dump(["\x000"])),
    "\\ octal followed by digit";
  TEST q(Data::Dumper->Dumpxs(["\x000"])), '\\ octal followed by digit (xs)'
    if $XS;

  $WANT = <<'EOW';
#$VAR1 = "\x{100}\0000";
EOW
  local $Data::Dumper::Useqq = 1;
  TEST q(Data::Dumper->Dump(["\x{100}\x000"])),
    "\\ octal followed by digit unicode";
  TEST q(Data::Dumper->Dumpxs(["\x{100}\x000"])), '\\ octal followed by digit unicode (xs)'
    if $XS;


  $WANT = <<'EOW';
#$VAR1 = "\0\x{660}";
EOW
  TEST q(Data::Dumper->Dump(["\\x00\\x{0660}"])),
    "\\ octal followed by unicode digit";
  TEST q(Data::Dumper->Dumpxs(["\\x00\\x{0660}"])), '\\ octal followed by unicode digit (xs)'
    if $XS;

  # [perl #118933 - handling of digits
$WANT = <<'EOW';
#$VAR1 = 0;
#$VAR2 = 1;
#$VAR3 = 90;
#$VAR4 = -10;
#$VAR5 = "010";
#$VAR6 = 112345678;
#$VAR7 = "1234567890";
EOW
  TEST q(Data::Dumper->Dump([0, 1, 90, -10, "010", "112345678", "1234567890" ])),
    "numbers and number-like scalars";

  TEST q(Data::Dumper->Dumpxs([0, 1, 90, -10, "010", "112345678", "1234567890" ])),
    "numbers and number-like scalars"
    if $XS;
}
############# 426
{
  # [perl #82948]
  # re::regexp_pattern was moved to universal.c in v5.10.0-252-g192c1e2
  # and apparently backported to maint-5.10
  $WANT = $] > 5.010 ? <<'NEW' : <<'OLD';
#$VAR1 = qr/abc/;
#$VAR2 = qr/abc/i;
NEW
#$VAR1 = qr/(?-xism:abc)/;
#$VAR2 = qr/(?i-xsm:abc)/;
OLD
  TEST q(Data::Dumper->Dump([ qr/abc/, qr/abc/i ])), "qr//";
  TEST q(Data::Dumper->Dumpxs([ qr/abc/, qr/abc/i ])), "qr// xs"
    if $XS;
}
############# 432
