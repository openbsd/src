#!./perl -w
#
# testsuite for Data::Dumper
#

BEGIN {
    if ($ENV{PERL_CORE}){
        chdir 't' if -d 't';
        @INC = '../lib';
        require Config; import Config;
        if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
            print "1..0 # Skip: Data::Dumper was not built\n";
            exit 0;
        }
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
  eval "$t";
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
  $TMAX = 363; $XS = 1;
}
else {
  print "### XS extensions not loaded, will NOT run XS tests\n";
  $TMAX = 183; $XS = 0;
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

TEST q(Data::Dumper->Dump([$a,$b,$c], [qw(a b), 6]));
TEST q(Data::Dumper->Dumpxs([$a,$b,$c], [qw(a b), 6])) if $XS;


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
TEST q(Data::Dumper->Dump([$a, $b], [qw(*a b)])); # print as @a
TEST q(Data::Dumper->Dumpxs([$a, $b], [qw(*a b)])) if $XS;

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

TEST q(Data::Dumper->Dump([$b, $a], [qw(*b a)])); # print as %b
TEST q(Data::Dumper->Dumpxs([$b, $a], [qw(*b a)])) if $XS;

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
TEST q(
       $d = Data::Dumper->new([$a,$b], [qw(a b)]);
       $d->Seen({'*c' => $c});
       $d->Dump;
      );
if ($XS) {
  TEST q(
	 $d = Data::Dumper->new([$a,$b], [qw(a b)]);
	 $d->Seen({'*c' => $c});
	 $d->Dumpxs;
	);
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
TEST q( $d->Reset; $d->Dump );

TEST q( $d->Reset; $d->Dumpxs ) if $XS;

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

TEST q(Dumper($a));
TEST q(Data::Dumper::DumperX($a)) if $XS;

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
  TEST q(Dumper($a));
  TEST q(Data::Dumper::DumperX($a)) if $XS;
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
  TEST q(Dumper($foo));
}

  $WANT = <<"EOT";
#\$VAR1 = {
#  'abc\0\\'\efg' => 'mno\0',
#  'reftest' => \\\\1
#};
EOT

  {
    local $Data::Dumper::Useqq = 1;
    TEST q(Data::Dumper::DumperX($foo)) if $XS;   # cheat
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
  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])) if $XS;

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
  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])) if $XS;

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

  TEST q(Data::Dumper->Dump([\\@foo, \\%foo, \\*foo], ['*bar', '*baz', '*foo']));
  TEST q(Data::Dumper->Dumpxs([\\@foo, \\%foo, \\*foo], ['*bar', '*baz', '*foo'])) if $XS;

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

  TEST q(Data::Dumper->Dump([\\@foo, \\%foo, \\*foo], ['bar', 'baz', 'foo']));
  TEST q(Data::Dumper->Dumpxs([\\@foo, \\%foo, \\*foo], ['bar', 'baz', 'foo'])) if $XS;

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
  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['*foo', '*bar', '*baz'])) if $XS;

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

  TEST q(Data::Dumper->Dump([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz']));
  TEST q(Data::Dumper->Dumpxs([\\*foo, \\@foo, \\%foo], ['foo', 'bar', 'baz'])) if $XS;

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

  TEST q(
	 $d = Data::Dumper->new([\\%kennel, \\@dogs, $mutts],
				[qw(*kennels *dogs *mutts)] );
	 $d->Dump;
	);
  if ($XS) {
    TEST q(
	   $d = Data::Dumper->new([\\%kennel, \\@dogs, $mutts],
				  [qw(*kennels *dogs *mutts)] );
	   $d->Dumpxs;
	  );
  }
  
############# 91
##
  $WANT = <<'EOT';
#%kennels = %kennels;
#@dogs = @dogs;
#%mutts = %kennels;
EOT

  TEST q($d->Dump);
  TEST q($d->Dumpxs) if $XS;
  
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

  
  TEST q($d->Reset; $d->Dump);
  if ($XS) {
    TEST q($d->Reset; $d->Dumpxs);
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

  TEST q(
	 $d = Data::Dumper->new([\\@dogs, \\%kennel, $mutts],
				[qw(*dogs *kennels *mutts)] );
	 $d->Dump;
	);
  if ($XS) {
    TEST q(
	   $d = Data::Dumper->new([\\@dogs, \\%kennel, $mutts],
				  [qw(*dogs *kennels *mutts)] );
	   $d->Dumpxs;
	  );
  }
  
############# 109
##
  TEST q($d->Reset->Dump);
  if ($XS) {
    TEST q($d->Reset->Dumpxs);
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

  TEST q(
	 $d = Data::Dumper->new( [\@dogs, \%kennel], [qw(*dogs *kennels)] );
	 $d->Deepcopy(1)->Dump;
	);
  if ($XS) {
    TEST q($d->Reset->Dumpxs);
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

TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'b' => \&z})->Dump;);
TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'b' => \&z})->Dumpxs;)
	if $XS;

############# 127
##
  $WANT = <<'EOT';
#$a = \&b;
#$c = [
#  \&b
#];
EOT

TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'*b' => \&z})->Dump;);
TEST q(Data::Dumper->new([\&z,$c],['a','c'])->Seen({'*b' => \&z})->Dumpxs;)
	if $XS;

############# 133
##
  $WANT = <<'EOT';
#*a = \&b;
#@c = (
#  \&b
#);
EOT

TEST q(Data::Dumper->new([\&z,$c],['*a','*c'])->Seen({'*b' => \&z})->Dump;);
TEST q(Data::Dumper->new([\&z,$c],['*a','*c'])->Seen({'*b' => \&z})->Dumpxs;)
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

TEST q(Data::Dumper->new([$a],['*a'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a],['*a'])->Purity(1)->Dumpxs;)
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

TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dumpxs;)
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

TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b],['a','b'])->Purity(1)->Dumpxs;)
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

TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Purity(1)->Dumpxs;)
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

TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(4)->Dump;);
TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(4)->Dumpxs;)
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

TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(1)->Dump;);
TEST q(Data::Dumper->new([$a,$b,$c],['a','b','c'])->Maxdepth(1)->Dumpxs;)
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

TEST q(Data::Dumper->new([$b],['b'])->Purity(0)->Dump;);
TEST q(Data::Dumper->new([$b],['b'])->Purity(0)->Dumpxs;)
	if $XS;

############# 181
##
  $WANT = <<'EOT';
#$b = [
#  \do{my $o}
#];
#${$b->[0]} = $b->[0];
EOT


TEST q(Data::Dumper->new([$b],['b'])->Purity(1)->Dump;);
TEST q(Data::Dumper->new([$b],['b'])->Purity(1)->Dumpxs;)
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

TEST q(Data::Dumper->new([$a])->Dump;);
TEST q(Data::Dumper->new([$a])->Dumpxs;)
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

# perl code does keys and values as numbers if possible
TEST q(Data::Dumper->new([$c])->Dump;);
# XS code always does them as strings
$WANT =~ s/ (\d+)/ '$1'/gs;
TEST q(Data::Dumper->new([$c])->Dumpxs;)
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

TEST q(Data::Dumper->new([[$c, $d]])->Dump;);
$WANT =~ s/ (\d+)/ '$1'/gs;
TEST q(Data::Dumper->new([[$c, $d]])->Dumpxs;)
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
    TEST q(Data::Dumper->new([{ foo => sub { print "foo"; } }])->Dump);
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
  $WANT=$WANT_XS_N;
  TEST q(Data::Dumper->new(\@numbers)->Dumpxs), 'XS Numbers';
  TEST q(Data::Dumper->new(\@numbers_s)->Dumpxs), 'XS Numbers PV';
  $WANT=$WANT_XS_I;
  TEST q(Data::Dumper->new(\@numbers_i)->Dumpxs), 'XS Numbers IV';
  TEST q(Data::Dumper->new(\@numbers_is)->Dumpxs), 'XS Numbers IV,PV';
  $WANT=$WANT_XS_N;
  TEST q(Data::Dumper->new(\@numbers_n)->Dumpxs), 'XS Numbers NV';
  TEST q(Data::Dumper->new(\@numbers_ns)->Dumpxs), 'XS Numbers NV,PV';
  $WANT=$WANT_XS_I;
  TEST q(Data::Dumper->new(\@numbers_ni)->Dumpxs), 'XS Numbers NV,IV';
  TEST q(Data::Dumper->new(\@numbers_nis)->Dumpxs), 'XS Numbers NV,IV,PV';

  $WANT=$WANT_XS_S;
  TEST q(Data::Dumper->new(\@strings)->Dumpxs), 'XS Strings';
  TEST q(Data::Dumper->new(\@strings_s)->Dumpxs), 'XS Strings PV';
  # This one used to really mess up. New code actually emulates the .pm code
  $WANT=$WANT_PL_S;
  TEST q(Data::Dumper->new(\@strings_i)->Dumpxs), 'XS Strings IV';
  TEST q(Data::Dumper->new(\@strings_is)->Dumpxs), 'XS Strings IV,PV';
  $WANT=$WANT_XS_S;
  TEST q(Data::Dumper->new(\@strings_n)->Dumpxs), 'XS Strings NV';
  TEST q(Data::Dumper->new(\@strings_ns)->Dumpxs), 'XS Strings NV,PV';
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
      TEST q(Data::Dumper->Dump([\\*ping, \\%ping], ['*ping', '*pong']));
      TEST q(Data::Dumper->Dumpxs([\\*ping, \\%ping], ['*ping', '*pong'])) if $XS;
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
    TEST q(Data::Dumper->Dump([\@foo])), 'Richard Clamp, Message-Id: <20030104005247.GA27685@mirth.demon.co.uk>';
    TEST q(Data::Dumper->Dumpxs([\@foo])) if $XS;
}


