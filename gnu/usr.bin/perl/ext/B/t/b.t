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
use Config;

print "1..2\n";

my $test = 1;

sub ok { print "ok $test\n"; $test++ }

use B;


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
print "not " unless join('', sort @syms) eq join('', sort keys %Subs);
ok;

# Make sure we only hit them each once.
print "not " unless !grep $_ != 1, values %Subs;
ok;
