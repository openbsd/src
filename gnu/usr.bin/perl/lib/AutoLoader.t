#!./perl -w

BEGIN {
    chdir 't' if -d 't';
	@INC = '../lib';
}

use strict;
use File::Spec;
use File::Path;

my $dir;
BEGIN
{
	$dir = File::Spec->catdir( "auto-$$" );
	unshift @INC, $dir;
}

use Test::More tests => 17;

# First we must set up some autoloader files
my $fulldir = File::Spec->catdir( $dir, 'auto', 'Foo' );
mkpath( $fulldir ) or die "Can't mkdir '$fulldir': $!";

open(FOO, '>', File::Spec->catfile( $fulldir, 'foo.al' ))
	or die "Can't open foo file: $!";
print FOO <<'EOT';
package Foo;
sub foo { shift; shift || "foo" }
1;
EOT
close(FOO);

open(BAR, '>', File::Spec->catfile( $fulldir, 'bar.al' ))
	or die "Can't open bar file: $!";
print BAR <<'EOT';
package Foo;
sub bar { shift; shift || "bar" }
1;
EOT
close(BAR);

open(BAZ, '>', File::Spec->catfile( $fulldir, 'bazmarkhian.al' ))
	or die "Can't open bazmarkhian file: $!";
print BAZ <<'EOT';
package Foo;
sub bazmarkhianish { shift; shift || "baz" }
1;
EOT
close(BAZ);

open(BLECH, '>', File::Spec->catfile( $fulldir, 'blechanawilla.al' ))
       or die "Can't open blech file: $!";
print BLECH <<'EOT';
package Foo;
sub blechanawilla { compilation error (
EOT
close(BLECH);

# This is just to keep the old SVR3 systems happy; they may fail
# to find the above file so we duplicate it where they should find it.
open(BLECH, '>', File::Spec->catfile( $fulldir, 'blechanawil.al' ))
       or die "Can't open blech file: $!";
print BLECH <<'EOT';
package Foo;
sub blechanawilla { compilation error (
EOT
close(BLECH);

# Let's define the package
package Foo;
require AutoLoader;
AutoLoader->import( 'AUTOLOAD' );

sub new { bless {}, shift };
sub foo;
sub bar;
sub bazmarkhianish; 

package main;

my $foo = new Foo;

my $result = $foo->can( 'foo' );
ok( $result,               'can() first time' );
is( $foo->foo, 'foo', 'autoloaded first time' );
is( $foo->foo, 'foo', 'regular call' );
is( $result,   \&Foo::foo, 'can() returns ref to regular installed sub' );

eval {
    $foo->will_fail;
};
like( $@, qr/^Can't locate/, 'undefined method' );

$result = $foo->can( 'will_fail' );
ok( ! $result,               'can() should fail on undefined methods' );

# Used to be trouble with this
eval {
    my $foo = new Foo;
    die "oops";
};
like( $@, qr/oops/, 'indirect method call' );

# Pass regular expression variable to autoloaded function.  This used
# to go wrong because AutoLoader used regular expressions to generate
# autoloaded filename.
'foo' =~ /(\w+)/;

is( $foo->bar($1), 'foo', 'autoloaded method should not stomp match vars' );
is( $foo->bar($1), 'foo', '(again)' );
is( $foo->bazmarkhianish($1), 'foo', 'for any method call' );
is( $foo->bazmarkhianish($1), 'foo', '(again)' );

# Used to retry long subnames with shorter filenames on any old
# exception, including compilation error.  Now AutoLoader only
# tries shorter filenames if it can't find the long one.
eval {
  $foo->blechanawilla;
};
like( $@, qr/syntax error/, 'require error propagates' );

# test recursive autoloads
open(F, '>', File::Spec->catfile( $fulldir, 'a.al'))
	or die "Cannot make 'a' file: $!";
print F <<'EOT';
package Foo;
BEGIN { b() }
sub a { ::ok( 1, 'adding a new autoloaded method' ); }
1;
EOT
close(F);

open(F, '>', File::Spec->catfile( $fulldir, 'b.al'))
	or die "Cannot make 'b' file: $!";
print F <<'EOT';
package Foo;
sub b { ::ok( 1, 'adding a new autoloaded method' ) }
1;
EOT
close(F);
Foo::a();

package Bar;
AutoLoader->import();
::ok( ! defined &AUTOLOAD, 'AutoLoader should not export AUTOLOAD by default' );

package Foo;
AutoLoader->unimport();
eval { Foo->baz() };
::like( $@, qr/locate object method "baz"/,
	'unimport() should remove imported AUTOLOAD()' );

package Baz;

sub AUTOLOAD { 'i am here' }

AutoLoader->import();
AutoLoader->unimport();

::is( Baz->AUTOLOAD(), 'i am here', '... but not non-imported AUTOLOAD()' );

package main;

# cleanup
END {
	return unless $dir && -d $dir;
	rmtree $dir;
}
