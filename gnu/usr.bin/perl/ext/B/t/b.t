#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') {
	@INC = qw(: ::lib ::macos:lib);
    } else {
	@INC = '.';
	push @INC, '../lib';
    }
}

$|  = 1;
use warnings;
use strict;
use Test::More tests => 5;

BEGIN { use_ok( 'B' ); }


package Testing::Symtable;
use vars qw($This @That %wibble $moo %moo);
my $not_a_sym = 'moo';

sub moo { 42 }
sub car { 23 }


package Testing::Symtable::Foo;
sub yarrow { "Hock" }

package Testing::Symtable::Bar;
sub hock { "yarrow" }

package main;
use vars qw(%Subs);
local %Subs = ();
B::walksymtable(\%Testing::Symtable::, 'find_syms', sub { $_[0] =~ /Foo/ },
                'Testing::Symtable::');

sub B::GV::find_syms {
    my($symbol) = @_;

    $main::Subs{$symbol->STASH->NAME . '::' . $symbol->NAME}++;
}

my @syms = map { 'Testing::Symtable::'.$_ } qw(This That wibble moo car
                                               BEGIN);
push @syms, "Testing::Symtable::Foo::yarrow";

# Make sure we hit all the expected symbols.
ok( join('', sort @syms) eq join('', sort keys %Subs), 'all symbols found' );

# Make sure we only hit them each once.
ok( (!grep $_ != 1, values %Subs), '...and found once' );

# Tests for MAGIC / MOREMAGIC
ok( B::svref_2object(\$.)->MAGIC->TYPE eq "\0", '$. has \0 magic' );
{
    my $e = '';
    local $SIG{__DIE__} = sub { $e = $_[0] };
    # Used to dump core, bug #16828
    eval { B::svref_2object(\$.)->MAGIC->MOREMAGIC->TYPE; };
    like( $e, qr/Can't call method "TYPE" on an undefined value/, 
	'$. has no more magic' );
}
