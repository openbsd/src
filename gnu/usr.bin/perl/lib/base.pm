=head1 NAME

base - Establish IS-A relationship with base class at compile time

=head1 SYNOPSIS

    package Baz;

    use base qw(Foo Bar);

=head1 DESCRIPTION

Roughly similar in effect to

    BEGIN {
	require Foo;
	require Bar;
	push @ISA, qw(Foo Bar);
    }

This module was introduced with Perl 5.004_04.

=head1 BUGS

Needs proper documentation!

=cut

package base;

sub import {
    my $class = shift;

    foreach my $base (@_) {
	unless (defined %{"$base\::"}) {
	    eval "require $base";
	    unless (defined %{"$base\::"}) {
		require Carp;
		Carp::croak("Base class package \"$base\" is empty.\n",
			    "\t(Perhaps you need to 'use' the module ",
			    "which defines that package first.)");
	    }
	}
    }
    
    push @{caller(0) . '::ISA'}, @_;
}

1;
