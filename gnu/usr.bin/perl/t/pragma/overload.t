#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;

package Oscalar;
use overload ( 
				# Anonymous subroutines:
'+'	=>	sub {new Oscalar $ {$_[0]}+$_[1]},
'-'	=>	sub {new Oscalar
		       $_[2]? $_[1]-${$_[0]} : ${$_[0]}-$_[1]},
'<=>'	=>	sub {new Oscalar
		       $_[2]? $_[1]-${$_[0]} : ${$_[0]}-$_[1]},
'cmp'	=>	sub {new Oscalar
		       $_[2]? ($_[1] cmp ${$_[0]}) : (${$_[0]} cmp $_[1])},
'*'	=>	sub {new Oscalar ${$_[0]}*$_[1]},
'/'	=>	sub {new Oscalar 
		       $_[2]? $_[1]/${$_[0]} :
			 ${$_[0]}/$_[1]},
'%'	=>	sub {new Oscalar
		       $_[2]? $_[1]%${$_[0]} : ${$_[0]}%$_[1]},
'**'	=>	sub {new Oscalar
		       $_[2]? $_[1]**${$_[0]} : ${$_[0]}-$_[1]},

qw(
""	stringify
0+	numify)			# Order of arguments unsignificant
);

sub new {
  my $foo = $_[1];
  bless \$foo, $_[0];
}

sub stringify { "${$_[0]}" }
sub numify { 0 + "${$_[0]}" }	# Not needed, additional overhead
				# comparing to direct compilation based on
				# stringify

package main;

$test = 0;
$| = 1;
print "1..",&last,"\n";

sub test {
  $test++; if (shift) {print "ok $test\n";1} else {print "not ok $test\n";0}
}

$a = new Oscalar "087";
$b= "$a";

# All test numbers in comments are off by 1.
# So much for hard-wiring them in :-) To fix this:
test(1);			# 1

test ($b eq $a);		# 2
test ($b eq "087");		# 3
test (ref $a eq "Oscalar");	# 4
test ($a eq $a);		# 5
test ($a eq "087");		# 6

$c = $a + 7;

test (ref $c eq "Oscalar");	# 7
test (!($c eq $a));		# 8
test ($c eq "94");		# 9

$b=$a;

test (ref $a eq "Oscalar");	# 10

$b++;

test (ref $b eq "Oscalar");	# 11
test ( $a eq "087");		# 12
test ( $b eq "88");		# 13
test (ref $a eq "Oscalar");	# 14

$c=$b;
$c-=$a;

test (ref $c eq "Oscalar");	# 15
test ( $a eq "087");		# 16
test ( $c eq "1");		# 17
test (ref $a eq "Oscalar");	# 18

$b=1;
$b+=$a;

test (ref $b eq "Oscalar");	# 19
test ( $a eq "087");		# 20
test ( $b eq "88");		# 21
test (ref $a eq "Oscalar");	# 22

eval q[ package Oscalar; use overload ('++' => sub { $ {$_[0]}++;$_[0] } ) ];

$b=$a;

test (ref $a eq "Oscalar");	# 23

$b++;

test (ref $b eq "Oscalar");	# 24
test ( $a eq "087");		# 25
test ( $b eq "88");		# 26
test (ref $a eq "Oscalar");	# 27

package Oscalar;
$dummy=bless \$dummy;		# Now cache of method should be reloaded
package main;

$b=$a;
$b++;				

test (ref $b eq "Oscalar");	# 28
test ( $a eq "087");		# 29
test ( $b eq "88");		# 30
test (ref $a eq "Oscalar");	# 31


eval q[package Oscalar; use overload ('++' => sub { $ {$_[0]} += 2; $_[0] } ) ];

$b=$a;

test (ref $a eq "Oscalar");	# 32

$b++;

test (ref $b eq "Oscalar");	# 33
test ( $a eq "087");		# 34
test ( $b eq "88");		# 35
test (ref $a eq "Oscalar");	# 36

package Oscalar;
$dummy=bless \$dummy;		# Now cache of method should be reloaded
package main;

$b++;				

test (ref $b eq "Oscalar");	# 37
test ( $a eq "087");		# 38
test ( $b eq "90");		# 39
test (ref $a eq "Oscalar");	# 40

$b=$a;
$b++;

test (ref $b eq "Oscalar");	# 41
test ( $a eq "087");		# 42
test ( $b eq "89");		# 43
test (ref $a eq "Oscalar");	# 44


test ($b? 1:0);			# 45

eval q[ package Oscalar; use overload ('=' => sub {$main::copies++; 
						   package Oscalar;
						   local $new=$ {$_[0]};
						   bless \$new } ) ];

$b=new Oscalar "$a";

test (ref $b eq "Oscalar");	# 46
test ( $a eq "087");		# 47
test ( $b eq "087");		# 48
test (ref $a eq "Oscalar");	# 49

$b++;

test (ref $b eq "Oscalar");	# 50
test ( $a eq "087");		# 51
test ( $b eq "89");		# 52
test (ref $a eq "Oscalar");	# 53
test ($copies == 0);		# 54

$b+=1;

test (ref $b eq "Oscalar");	# 55
test ( $a eq "087");		# 56
test ( $b eq "90");		# 57
test (ref $a eq "Oscalar");	# 58
test ($copies == 0);		# 59

$b=$a;
$b+=1;

test (ref $b eq "Oscalar");	# 60
test ( $a eq "087");		# 61
test ( $b eq "88");		# 62
test (ref $a eq "Oscalar");	# 63
test ($copies == 0);		# 64

$b=$a;
$b++;

test (ref $b eq "Oscalar") || print ref $b,"=ref(b)\n";	# 65
test ( $a eq "087");		# 66
test ( $b eq "89");		# 67
test (ref $a eq "Oscalar");	# 68
test ($copies == 1);		# 69

eval q[package Oscalar; use overload ('+=' => sub {$ {$_[0]} += 3*$_[1];
						   $_[0] } ) ];
$c=new Oscalar;			# Cause rehash

$b=$a;
$b+=1;

test (ref $b eq "Oscalar");	# 70
test ( $a eq "087");		# 71
test ( $b eq "90");		# 72
test (ref $a eq "Oscalar");	# 73
test ($copies == 2);		# 74

$b+=$b;

test (ref $b eq "Oscalar");	# 75
test ( $b eq "360");		# 76
test ($copies == 2);		# 77
$b=-$b;

test (ref $b eq "Oscalar");	# 78
test ( $b eq "-360");		# 79
test ($copies == 2);		# 80

$b=abs($b);

test (ref $b eq "Oscalar");	# 81
test ( $b eq "360");		# 82
test ($copies == 2);		# 83

$b=abs($b);

test (ref $b eq "Oscalar");	# 84
test ( $b eq "360");		# 85
test ($copies == 2);		# 86

eval q[package Oscalar; 
       use overload ('x' => sub {new Oscalar ( $_[2] ? "_.$_[1]._" x $ {$_[0]}
					      : "_.${$_[0]}._" x $_[1])}) ];

$a=new Oscalar "yy";
$a x= 3;
test ($a eq "_.yy.__.yy.__.yy._"); # 87

eval q[package Oscalar; 
       use overload ('.' => sub {new Oscalar ( $_[2] ? 
					      "_.$_[1].__.$ {$_[0]}._"
					      : "_.$ {$_[0]}.__.$_[1]._")}) ];

$a=new Oscalar "xx";

test ("b${a}c" eq "_._.b.__.xx._.__.c._"); # 88

# Check inheritance of overloading;
{
  package OscalarI;
  @ISA = 'Oscalar';
}

$aI = new OscalarI "$a";
test (ref $aI eq "OscalarI");	# 89
test ("$aI" eq "xx");		# 90
test ($aI eq "xx");		# 91
test ("b${aI}c" eq "_._.b.__.xx._.__.c._");		# 92

# Here we test blessing to a package updates hash

eval "package Oscalar; no overload '.'";

test ("b${a}" eq "_.b.__.xx._"); # 93
$x="1";
bless \$x, Oscalar;
test ("b${a}c" eq "bxxc");	# 94
new Oscalar 1;
test ("b${a}c" eq "bxxc");	# 95

# Negative overloading:

$na = eval { ~$a };
test($@ =~ /no method found/);	# 96

# Check AUTOLOADING:

*Oscalar::AUTOLOAD = 
  sub { *{"Oscalar::$AUTOLOAD"} = sub {"_!_" . shift() . "_!_"} ;
	goto &{"Oscalar::$AUTOLOAD"}};

eval "package Oscalar; sub comple; use overload '~' => 'comple'";

$na = eval { ~$a };		# Hash was not updated
test($@ =~ /no method found/);	# 97

bless \$x, Oscalar;

$na = eval { ~$a };		# Hash updated
warn "`$na', $@" if $@;
test !$@;			# 98
test($na eq '_!_xx_!_');	# 99

$na = 0;

$na = eval { ~$aI };		# Hash was not updated
test($@ =~ /no method found/);	# 100

bless \$x, OscalarI;

$na = eval { ~$aI };
print $@;

test !$@;			# 101
test($na eq '_!_xx_!_');	# 102

eval "package Oscalar; sub rshft; use overload '>>' => 'rshft'";

$na = eval { $aI >> 1 };	# Hash was not updated
test($@ =~ /no method found/);	# 103

bless \$x, OscalarI;

$na = 0;

$na = eval { $aI >> 1 };
print $@;

test !$@;			# 104
test($na eq '_!_xx_!_');	# 105

# warn overload::Method($a, '0+'), "\n";
test (overload::Method($a, '0+') eq \&Oscalar::numify); # 106
test (overload::Method($aI,'0+') eq \&Oscalar::numify); # 107
test (overload::Overloaded($aI)); # 108
test (!overload::Overloaded('overload')); # 109

test (! defined overload::Method($aI, '<<')); # 110
test (! defined overload::Method($a, '<')); # 111

test (overload::StrVal($aI) =~ /^OscalarI=SCALAR\(0x[\da-fA-F]+\)$/); # 112
test (overload::StrVal(\$aI) eq "@{[\$aI]}"); # 113

# Check overloading by methods (specified deep in the ISA tree).
{
  package OscalarII;
  @ISA = 'OscalarI';
  sub Oscalar::lshft {"_<<_" . shift() . "_<<_"}
  eval "package OscalarI; use overload '<<' => 'lshft', '|' => 'lshft'";
}

$aaII = "087";
$aII = \$aaII;
bless $aII, 'OscalarII';
bless \$fake, 'OscalarI';		# update the hash
test(($aI | 3) eq '_<<_xx_<<_');	# 114
# warn $aII << 3;
test(($aII << 3) eq '_<<_087_<<_');	# 115

# Last test is:
sub last {115}
