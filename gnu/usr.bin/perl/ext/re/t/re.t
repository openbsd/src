#!./perl

BEGIN {
	require Config;
	if (($Config::Config{'extensions'} !~ /\bre\b/) ){
        	print "1..0 # Skip -- Perl configured without re module\n";
		exit 0;
	}
}

use strict;

use Test::More tests => 13;
require_ok( 're' );

# setcolor
$INC{ 'Term/Cap.pm' } = 1;
local $ENV{PERL_RE_TC};
re::setcolor();
is( $ENV{PERL_RE_COLORS}, "md\tme\tso\tse\tus\tue", 
	'setcolor() should provide default colors' );
$ENV{PERL_RE_TC} = 'su,n,ny';
re::setcolor();
is( $ENV{PERL_RE_COLORS}, "su\tn\tny", '... or use $ENV{PERL_RE_COLORS}' );

# bits
# get on
my $warn;
local $SIG{__WARN__} = sub {
	$warn = shift;
};
#eval { re::bits(1) };
#like( $warn, qr/Useless use/, 'bits() should warn with no args' );

delete $ENV{PERL_RE_COLORS};
re::bits(0, 'debug');
is( $ENV{PERL_RE_COLORS}, undef,
	"... should not set regex colors given 'debug'" );
re::bits(0, 'debugcolor');
isnt( $ENV{PERL_RE_COLORS}, '', 
	"... should set regex colors given 'debugcolor'" );
re::bits(0, 'nosuchsubpragma');
like( $warn, qr/Unknown "re" subpragma/, 
	'... should warn about unknown subpragma' );
ok( re::bits(0, 'taint') & 0x00100000, '... should set taint bits' );
ok( re::bits(0, 'eval')  & 0x00200000, '... should set eval bits' );

local $^H;

# import
re->import('taint', 'eval');
ok( $^H & 0x00100000, 'import should set taint bits in $^H when requested' );
ok( $^H & 0x00200000, 'import should set eval bits in $^H when requested' );

re->unimport('taint');
ok( !( $^H & 0x00100000 ), 'unimport should clear bits in $^H when requested' );
re->unimport('eval');
ok( !( $^H & 0x00200000 ), '... and again' );
my $reg=qr/(foo|bar|baz|blah)/;
close STDERR;
eval"use re Debug=>'ALL'";
my $ok='foo'=~/$reg/;
eval"no re Debug=>'ALL'";
ok( $ok, 'No segv!' );

package Term::Cap;

sub Tgetent {
	bless({}, $_[0]);
}

sub Tputs {
	return $_[1];
}
