#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use warnings;
use vars qw{ @warnings };
BEGIN {				# ...and save 'em for later
    $SIG{'__WARN__'} = sub { push @warnings, @_ }
}
END { print STDERR @warnings }


use strict;
use Test::More tests => 81;
my $TB = Test::More->builder;

BEGIN { use_ok('constant'); }

use constant PI		=> 4 * atan2 1, 1;

ok defined PI,                          'basic scalar constant';
is substr(PI, 0, 7), '3.14159',         '    in substr()';

sub deg2rad { PI * $_[0] / 180 }

my $ninety = deg2rad 90;

cmp_ok abs($ninety - 1.5707), '<', 0.0001, '    in math expression';

use constant UNDEF1	=> undef;	# the right way
use constant UNDEF2	=>	;	# the weird way
use constant 'UNDEF3'		;	# the 'short' way
use constant EMPTY	=> ( )  ;	# the right way for lists

is UNDEF1, undef,       'right way to declare an undef';
is UNDEF2, undef,       '    weird way';
is UNDEF3, undef,       '    short way';

# XXX Why is this way different than the other ones?
my @undef = UNDEF1;
is @undef, 1;
is $undef[0], undef;

@undef = UNDEF2;
is @undef, 0;
@undef = UNDEF3;
is @undef, 0;
@undef = EMPTY;
is @undef, 0;

use constant COUNTDOWN	=> scalar reverse 1, 2, 3, 4, 5;
use constant COUNTLIST	=> reverse 1, 2, 3, 4, 5;
use constant COUNTLAST	=> (COUNTLIST)[-1];

is COUNTDOWN, '54321';
my @cl = COUNTLIST;
is @cl, 5;
is COUNTDOWN, join '', @cl;
is COUNTLAST, 1;
is((COUNTLIST)[1], 4);

use constant ABC	=> 'ABC';
is "abc${\( ABC )}abc", "abcABCabc";

use constant DEF	=> 'D', 'E', chr ord 'F';
is "d e f @{[ DEF ]} d e f", "d e f D E F d e f";

use constant SINGLE	=> "'";
use constant DOUBLE	=> '"';
use constant BACK	=> '\\';
my $tt = BACK . SINGLE . DOUBLE ;
is $tt, q(\\'");

use constant MESS	=> q('"'\\"'"\\);
is MESS, q('"'\\"'"\\);
is length(MESS), 8;

use constant TRAILING	=> '12 cats';
{
    no warnings 'numeric';
    cmp_ok TRAILING, '==', 12;
}
is TRAILING, '12 cats';

use constant LEADING	=> " \t1234";
cmp_ok LEADING, '==', 1234;
is LEADING, " \t1234";

use constant ZERO1	=> 0;
use constant ZERO2	=> 0.0;
use constant ZERO3	=> '0.0';
is ZERO1, '0';
is ZERO2, '0';
is ZERO3, '0.0';

{
    package Other;
    use constant PI	=> 3.141;
}

cmp_ok(abs(PI - 3.1416), '<', 0.0001);
is Other::PI, 3.141;

use constant E2BIG => $! = 7;
cmp_ok E2BIG, '==', 7;
# This is something like "Arg list too long", but the actual message
# text may vary, so we can't test much better than this.
cmp_ok length(E2BIG), '>', 6;

is @warnings, 0 or diag join "\n", "unexpected warning", @warnings;
@warnings = ();		# just in case
undef &PI;
ok @warnings && ($warnings[0] =~ /Constant sub.* undefined/) or
  diag join "\n", "unexpected warning", @warnings;
shift @warnings;

is @warnings, 0, "unexpected warning";

my $curr_test = $TB->current_test;
use constant CSCALAR	=> \"ok 37\n";
use constant CHASH	=> { foo => "ok 38\n" };
use constant CARRAY	=> [ undef, "ok 39\n" ];
use constant CPHASH	=> [ { foo => 1 }, "ok 40\n" ];
use constant CCODE	=> sub { "ok $_[0]\n" };

print ${+CSCALAR};
print CHASH->{foo};
print CARRAY->[1];
print CPHASH->{foo};
print CCODE->($curr_test+5);

$TB->current_test($curr_test+5);

eval q{ CPHASH->{bar} };
like $@, qr/^No such pseudo-hash field/, "test missing pseudo-hash field";

eval q{ CCODE->{foo} };
ok scalar($@ =~ /^Constant is not a HASH/);


# Allow leading underscore
use constant _PRIVATE => 47;
is _PRIVATE, 47;

# Disallow doubled leading underscore
eval q{
    use constant __DISALLOWED => "Oops";
};
like $@, qr/begins with '__'/;

# Check on declared() and %declared. This sub should be EXACTLY the
# same as the one quoted in the docs!
sub declared ($) {
    use constant 1.01;              # don't omit this!
    my $name = shift;
    $name =~ s/^::/main::/;
    my $pkg = caller;
    my $full_name = $name =~ /::/ ? $name : "${pkg}::$name";
    $constant::declared{$full_name};
}

ok declared 'PI';
ok $constant::declared{'main::PI'};

ok !declared 'PIE';
ok !$constant::declared{'main::PIE'};

{
    package Other;
    use constant IN_OTHER_PACK => 42;
    ::ok ::declared 'IN_OTHER_PACK';
    ::ok $constant::declared{'Other::IN_OTHER_PACK'};
    ::ok ::declared 'main::PI';
    ::ok $constant::declared{'main::PI'};
}

ok declared 'Other::IN_OTHER_PACK';
ok $constant::declared{'Other::IN_OTHER_PACK'};

@warnings = ();
eval q{
    no warnings;
    use warnings 'constant';
    use constant 'BEGIN' => 1 ;
    use constant 'INIT' => 1 ;
    use constant 'CHECK' => 1 ;
    use constant 'END' => 1 ;
    use constant 'DESTROY' => 1 ;
    use constant 'AUTOLOAD' => 1 ;
    use constant 'STDIN' => 1 ;
    use constant 'STDOUT' => 1 ;
    use constant 'STDERR' => 1 ;
    use constant 'ARGV' => 1 ;
    use constant 'ARGVOUT' => 1 ;
    use constant 'ENV' => 1 ;
    use constant 'INC' => 1 ;
    use constant 'SIG' => 1 ;
};

is @warnings, 15 ;
my @Expected_Warnings = 
  (
   qr/^Constant name 'BEGIN' is a Perl keyword at/,
   qr/^Constant subroutine BEGIN redefined at/,
   qr/^Constant name 'INIT' is a Perl keyword at/,
   qr/^Constant name 'CHECK' is a Perl keyword at/,
   qr/^Constant name 'END' is a Perl keyword at/,
   qr/^Constant name 'DESTROY' is a Perl keyword at/,
   qr/^Constant name 'AUTOLOAD' is a Perl keyword at/,
   qr/^Constant name 'STDIN' is forced into package main:: a/,
   qr/^Constant name 'STDOUT' is forced into package main:: at/,
   qr/^Constant name 'STDERR' is forced into package main:: at/,
   qr/^Constant name 'ARGV' is forced into package main:: at/,
   qr/^Constant name 'ARGVOUT' is forced into package main:: at/,
   qr/^Constant name 'ENV' is forced into package main:: at/,
   qr/^Constant name 'INC' is forced into package main:: at/,
   qr/^Constant name 'SIG' is forced into package main:: at/,
);
for my $idx (0..$#warnings) {
    like $warnings[$idx], $Expected_Warnings[$idx];
}
@warnings = ();


use constant {
	THREE  => 3,
	FAMILY => [ qw( John Jane Sally ) ],
	AGES   => { John => 33, Jane => 28, Sally => 3 },
	RFAM   => [ [ qw( John Jane Sally ) ] ],
	SPIT   => sub { shift },
	PHFAM  => [ { John => 1, Jane => 2, Sally => 3 }, 33, 28, 3 ],
};

is @{+FAMILY}, THREE;
is @{+FAMILY}, @{RFAM->[0]};
is FAMILY->[2], RFAM->[0]->[2];
is AGES->{FAMILY->[1]}, 28;
{ no warnings 'deprecated'; is PHFAM->{John}, AGES->{John}; }
is PHFAM->[3], AGES->{FAMILY->[2]};
is @{+PHFAM}, SPIT->(THREE+1);
is THREE**3, SPIT->(@{+FAMILY}**3);
is AGES->{FAMILY->[THREE-1]}, PHFAM->[THREE];

# Allow name of digits/underscores only if it begins with underscore
{
    use warnings FATAL => 'constant';
    eval q{
        use constant _1_2_3 => 'allowed';
    };
    ok( $@ eq '' );
}
