#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use Scalar::Util ();
use Test::More  (grep { /set_prototype/ } @Scalar::Util::EXPORT_FAIL)
			? (skip_all => 'set_prototype requires XS version')
			: (tests => 13);

Scalar::Util->import('set_prototype');

sub f { }
is( prototype('f'),	undef,	'no prototype');

$r = set_prototype(\&f,'$');
is( prototype('f'),	'$',	'set prototype');
is( $r,			\&f,	'return value');

set_prototype(\&f,undef);
is( prototype('f'),	undef,	'remove prototype');

set_prototype(\&f,'');
is( prototype('f'),	'',	'empty prototype');

sub g (@) { }
is( prototype('g'),	'@',	'@ prototype');

set_prototype(\&g,undef);
is( prototype('g'),	undef,	'remove prototype');

sub stub;
is( prototype('stub'),	undef,	'non existing sub');

set_prototype(\&stub,'$$$');
is( prototype('stub'),	'$$$',	'change non existing sub');

sub f_decl ($$$$);
is( prototype('f_decl'),	'$$$$',	'forward declaration');

set_prototype(\&f_decl,'\%');
is( prototype('f_decl'),	'\%',	'change forward declaration');

eval { &set_prototype( 'f', '' ); };
print "not " unless 
ok($@ =~ /^set_prototype: not a reference/,	'not a reference');

eval { &set_prototype( \'f', '' ); };
ok($@ =~ /^set_prototype: not a subroutine reference/,	'not a sub reference');
