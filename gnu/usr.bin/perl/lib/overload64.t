#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config;
    if ($Config::Config{'uvsize'} != 8) {
        print "1..0 # Skip -- Perl configured with 32-bit ints\n";
        exit 0;
    }
}

$| = 1;
use Test::More 'tests' => 100;


my $ii = 36028797018963971;  # 2^55 + 3


### Tests with numerifying large positive int
{ package Oobj;
    use overload '0+' => sub { ${$_[0]} += 1; $ii },
                 'fallback' => 1;
}
my $oo = bless(\do{my $x = 0}, 'Oobj');
my $cnt = 1;

is("$oo", "$ii", '0+ overload with stringification');
is($$oo, $cnt++, 'overload called once');

is($oo>>3, $ii>>3, '0+ overload with bit shift right');
is($$oo, $cnt++, 'overload called once');

is($oo<<2, $ii<<2, '0+ overload with bit shift left');
is($$oo, $cnt++, 'overload called once');

is($oo|0xFF00, $ii|0xFF00, '0+ overload with bitwise or');
is($$oo, $cnt++, 'overload called once');

is($oo&0xFF03, $ii&0xFF03, '0+ overload with bitwise and');
is($$oo, $cnt++, 'overload called once');

ok($oo == $ii, '0+ overload with equality');
is($$oo, $cnt++, 'overload called once');

is(int($oo), $ii, '0+ overload with int()');
is($$oo, $cnt++, 'overload called once');

is(abs($oo), $ii, '0+ overload with abs()');
is($$oo, $cnt++, 'overload called once');

is(-$oo, -$ii, '0+ overload with unary minus');
is($$oo, $cnt++, 'overload called once');

is(0+$oo, $ii, '0+ overload with addition');
is($$oo, $cnt++, 'overload called once');
is($oo+0, $ii, '0+ overload with addition');
is($$oo, $cnt++, 'overload called once');
is($oo+$oo, 2*$ii, '0+ overload with addition');
$cnt++;
is($$oo, $cnt++, 'overload called once');

is(0-$oo, -$ii, '0+ overload with subtraction');
is($$oo, $cnt++, 'overload called once');
is($oo-99, $ii-99, '0+ overload with subtraction');
is($$oo, $cnt++, 'overload called once');

is(2*$oo, 2*$ii, '0+ overload with multiplication');
is($$oo, $cnt++, 'overload called once');
is($oo*3, 3*$ii, '0+ overload with multiplication');
is($$oo, $cnt++, 'overload called once');

is($oo/1, $ii, '0+ overload with division');
is($$oo, $cnt++, 'overload called once');
is($ii/$oo, 1, '0+ overload with division');
is($$oo, $cnt++, 'overload called once');

is($oo%100, $ii%100, '0+ overload with modulo');
is($$oo, $cnt++, 'overload called once');
is($ii%$oo, 0, '0+ overload with modulo');
is($$oo, $cnt++, 'overload called once');

is($oo**1, $ii, '0+ overload with exponentiation');
is($$oo, $cnt++, 'overload called once');


### Tests with numerifying large negative int
{ package Oobj2;
    use overload '0+' => sub { ${$_[0]} += 1; -$ii },
                 'fallback' => 1;
}
$oo = bless(\do{my $x = 0}, 'Oobj2');
$cnt = 1;

is(int($oo), -$ii, '0+ overload with int()');
is($$oo, $cnt++, 'overload called once');

is(abs($oo), $ii, '0+ overload with abs()');
is($$oo, $cnt++, 'overload called once');

is(-$oo, $ii, '0+ overload with unary -');
is($$oo, $cnt++, 'overload called once');

is(0+$oo, -$ii, '0+ overload with addition');
is($$oo, $cnt++, 'overload called once');
is($oo+0, -$ii, '0+ overload with addition');
is($$oo, $cnt++, 'overload called once');
is($oo+$oo, -2*$ii, '0+ overload with addition');
$cnt++;
is($$oo, $cnt++, 'overload called once');

is(0-$oo, $ii, '0+ overload with subtraction');
is($$oo, $cnt++, 'overload called once');

is(2*$oo, -2*$ii, '0+ overload with multiplication');
is($$oo, $cnt++, 'overload called once');
is($oo*3, -3*$ii, '0+ overload with multiplication');
is($$oo, $cnt++, 'overload called once');

is($oo/1, -$ii, '0+ overload with division');
is($$oo, $cnt++, 'overload called once');
is($ii/$oo, -1, '0+ overload with division');
is($$oo, $cnt++, 'overload called once');

is($oo%100, (-$ii)%100, '0+ overload with modulo');
is($$oo, $cnt++, 'overload called once');
is($ii%$oo, 0, '0+ overload with modulo');
is($$oo, $cnt++, 'overload called once');

is($oo**1, -$ii, '0+ overload with exponentiation');
is($$oo, $cnt++, 'overload called once');

### Tests with overloading but no fallback
{ package Oobj3;
    use overload
        'int' => sub { ${$_[0]} += 1; $ii },
        'abs' => sub { ${$_[0]} += 1; $ii },
        'neg' => sub { ${$_[0]} += 1; -$ii },
        '+' => sub {
            ${$_[0]} += 1;
            my $res = (ref($_[0]) eq __PACKAGE__) ? $ii : $_[0];
            $res   += (ref($_[1]) eq __PACKAGE__) ? $ii : $_[1];
        },
        '-' => sub {
            ${$_[0]} += 1;
            my ($l, $r) = ($_[2]) ? (1, 0) : (0, 1);
            my $res = (ref($_[$l]) eq __PACKAGE__) ? $ii : $_[$l];
            $res   -= (ref($_[$r]) eq __PACKAGE__) ? $ii : $_[$r];
        },
        '*' => sub {
            ${$_[0]} += 1;
            my $res = (ref($_[0]) eq __PACKAGE__) ? $ii : $_[0];
            $res   *= (ref($_[1]) eq __PACKAGE__) ? $ii : $_[1];
        },
        '/' => sub {
            ${$_[0]} += 1;
            my ($l, $r) = ($_[2]) ? (1, 0) : (0, 1);
            my $res = (ref($_[$l]) eq __PACKAGE__) ? $ii+1 : $_[$l];
            $res   /= (ref($_[$r]) eq __PACKAGE__) ? $ii+1 : $_[$r];
        },
        '%' => sub {
            ${$_[0]} += 1;
            my ($l, $r) = ($_[2]) ? (1, 0) : (0, 1);
            my $res = (ref($_[$l]) eq __PACKAGE__) ? $ii : $_[$l];
            $res   %= (ref($_[$r]) eq __PACKAGE__) ? $ii : $_[$r];
        },
        '**' => sub {
            ${$_[0]} += 1;
            my ($l, $r) = ($_[2]) ? (1, 0) : (0, 1);
            my $res = (ref($_[$l]) eq __PACKAGE__) ? $ii : $_[$l];
            $res  **= (ref($_[$r]) eq __PACKAGE__) ? $ii : $_[$r];
        },
}
$oo = bless(\do{my $x = 0}, 'Oobj3');
$cnt = 1;

is(int($oo), $ii, 'int() overload');
is($$oo, $cnt++, 'overload called once');

is(abs($oo), $ii, 'abs() overload');
is($$oo, $cnt++, 'overload called once');

is(-$oo, -$ii, 'neg overload');
is($$oo, $cnt++, 'overload called once');

is(0+$oo, $ii, '+ overload');
is($$oo, $cnt++, 'overload called once');
is($oo+0, $ii, '+ overload');
is($$oo, $cnt++, 'overload called once');
is($oo+$oo, 2*$ii, '+ overload');
is($$oo, $cnt++, 'overload called once');

is(0-$oo, -$ii, '- overload');
is($$oo, $cnt++, 'overload called once');
is($oo-99, $ii-99, '- overload');
is($$oo, $cnt++, 'overload called once');

is($oo*2, 2*$ii, '* overload');
is($$oo, $cnt++, 'overload called once');
is(-3*$oo, -3*$ii, '* overload');
is($$oo, $cnt++, 'overload called once');

is($oo/2, ($ii+1)/2, '/ overload');
is($$oo, $cnt++, 'overload called once');
is(($ii+1)/$oo, 1, '/ overload');
is($$oo, $cnt++, 'overload called once');

is($oo%100, $ii%100, '% overload');
is($$oo, $cnt++, 'overload called once');
is($ii%$oo, 0, '% overload');
is($$oo, $cnt++, 'overload called once');

is($oo**1, $ii, '** overload');
is($$oo, $cnt++, 'overload called once');

# EOF
