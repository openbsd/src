#!./perl -wT

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use Test::More tests => 21;
use Getopt::Std;

our ($warning, $opt_f, $opt_i, $opt_o, $opt_x, $opt_y, %opt);

# First we test the getopt function
@ARGV = qw(-xo -f foo -y file);
getopt('f');

is( "@ARGV", 'file',		'options removed from @ARGV (1)' );
ok( $opt_x && $opt_o && $opt_y, 'options -x, -o and -y set' );
is( $opt_f, 'foo',		q/option -f is 'foo'/ );

@ARGV = qw(-hij k -- -l m -n);
getopt 'il', \%opt;

is( "@ARGV", 'k -- -l m -n',	'options removed from @ARGV (2)' );
ok( $opt{h} && $opt{i} eq 'j',	'option -h and -i correctly set' );
ok( !defined $opt{l},		'option -l not set' );
ok( !defined $opt_i,		'$opt_i still undefined' );

# Then we try the getopts
$opt_o = $opt_i = $opt_f = undef;
@ARGV = qw(-foi -i file);

ok( getopts('oif:'),		'getopts succeeded (1)' );
is( "@ARGV", 'file',		'options removed from @ARGV (3)' );
ok( $opt_i && $opt_f eq 'oi',	'options -i and -f correctly set' );
ok( !defined $opt_o,		'option -o not set' );

%opt = (); $opt_i = undef;
@ARGV = qw(-hij -k -- -l m);

ok( getopts('hi:kl', \%opt),	'getopts succeeded (2)' );
is( "@ARGV", '-l m',		'options removed from @ARGV (4)' );
ok( $opt{h} && $opt{k},		'options -h and -k set' );
is( $opt{i}, 'j',		q/option -i is 'j'/ );
ok( !defined $opt_i,		'$opt_i still undefined' );

# Try illegal options, but avoid printing of the error message
$SIG{__WARN__} = sub { $warning = $_[0] };
@ARGV = qw(-h help);

ok( !getopts("xf:y"),		'getopts fails for an illegal option' );
ok( $warning eq "Unknown option: h\n", 'user warned' );

# Then try the Getopt::Long module

use Getopt::Long;

@ARGV = qw(--help --file foo --foo --nobar --num=5 -- file);

our ($HELP, $FILE, $FOO, $BAR, $NO);

ok( GetOptions(
	'help'   => \$HELP,
	'file:s' => \$FILE,
	'foo!'   => \$FOO,
	'bar!'   => \$BAR,
	'num:i'  => \$NO,
    ),
    'Getopt::Long::GetOptions succeeded'
);
is( "@ARGV", 'file', 'options removed from @ARGV (5)' );
ok( $HELP && $FOO && !$BAR && $FILE eq 'foo' && $NO == 5, 'options set' );
