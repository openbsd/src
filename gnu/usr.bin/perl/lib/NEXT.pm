package NEXT;
$VERSION = '0.50';
use Carp;
use strict;

sub ancestors
{
	my @inlist = shift;
	my @outlist = ();
	while (my $next = shift @inlist) {
		push @outlist, $next;
		no strict 'refs';
		unshift @inlist, @{"$outlist[-1]::ISA"};
	}
	return @outlist;
}

sub AUTOLOAD
{
	my ($self) = @_;
	my $caller = (caller(1))[3]; 
	my $wanted = $NEXT::AUTOLOAD || 'NEXT::AUTOLOAD';
	undef $NEXT::AUTOLOAD;
	my ($caller_class, $caller_method) = $caller =~ m{(.*)::(.*)}g;
	my ($wanted_class, $wanted_method) = $wanted =~ m{(.*)::(.*)}g;
	croak "Can't call $wanted from $caller"
		unless $caller_method eq $wanted_method;

	local ($NEXT::NEXT{$self,$wanted_method}, $NEXT::SEEN) =
	      ($NEXT::NEXT{$self,$wanted_method}, $NEXT::SEEN);


	unless ($NEXT::NEXT{$self,$wanted_method}) {
		my @forebears =
			ancestors ref $self || $self, $wanted_class;
		while (@forebears) {
			last if shift @forebears eq $caller_class
		}
		no strict 'refs';
		@{$NEXT::NEXT{$self,$wanted_method}} = 
			map { *{"${_}::$caller_method"}{CODE}||() } @forebears
				unless $wanted_method eq 'AUTOLOAD';
		@{$NEXT::NEXT{$self,$wanted_method}} = 
			map { (*{"${_}::AUTOLOAD"}{CODE}) ? "${_}::AUTOLOAD" : ()} @forebears
				unless @{$NEXT::NEXT{$self,$wanted_method}||[]};
	}
	my $call_method = shift @{$NEXT::NEXT{$self,$wanted_method}};
	while ($wanted_class =~ /^NEXT:.*:UNSEEN/ && defined $call_method
	       && $NEXT::SEEN->{$self,$call_method}++) {
		$call_method = shift @{$NEXT::NEXT{$self,$wanted_method}};
	}
	unless (defined $call_method) {
		return unless $wanted_class =~ /^NEXT:.*:ACTUAL/;
		(local $Carp::CarpLevel)++;
		croak qq(Can't locate object method "$wanted_method" ),
		      qq(via package "$caller_class");
	};
	return shift()->$call_method(@_) if ref $call_method eq 'CODE';
	no strict 'refs';
	($wanted_method=${$caller_class."::AUTOLOAD"}) =~ s/.*:://
		if $wanted_method eq 'AUTOLOAD';
	$$call_method = $caller_class."::NEXT::".$wanted_method;
	return $call_method->(@_);
}

no strict 'vars';
package NEXT::UNSEEN;		@ISA = 'NEXT';
package NEXT::ACTUAL;		@ISA = 'NEXT';
package NEXT::ACTUAL::UNSEEN;	@ISA = 'NEXT';
package NEXT::UNSEEN::ACTUAL;	@ISA = 'NEXT';

1;

__END__

=head1 NAME

NEXT.pm - Provide a pseudo-class NEXT that allows method redispatch


=head1 SYNOPSIS

    use NEXT;

    package A;
    sub A::method   { print "$_[0]: A method\n";   $_[0]->NEXT::method() }
    sub A::DESTROY  { print "$_[0]: A dtor\n";     $_[0]->NEXT::DESTROY() }

    package B;
    use base qw( A );
    sub B::AUTOLOAD { print "$_[0]: B AUTOLOAD\n"; $_[0]->NEXT::AUTOLOAD() }
    sub B::DESTROY  { print "$_[0]: B dtor\n";     $_[0]->NEXT::DESTROY() }

    package C;
    sub C::method   { print "$_[0]: C method\n";   $_[0]->NEXT::method() }
    sub C::AUTOLOAD { print "$_[0]: C AUTOLOAD\n"; $_[0]->NEXT::AUTOLOAD() }
    sub C::DESTROY  { print "$_[0]: C dtor\n";     $_[0]->NEXT::DESTROY() }

    package D;
    use base qw( B C );
    sub D::method   { print "$_[0]: D method\n";   $_[0]->NEXT::method() }
    sub D::AUTOLOAD { print "$_[0]: D AUTOLOAD\n"; $_[0]->NEXT::AUTOLOAD() }
    sub D::DESTROY  { print "$_[0]: D dtor\n";     $_[0]->NEXT::DESTROY() }

    package main;

    my $obj = bless {}, "D";

    $obj->method();		# Calls D::method, A::method, C::method
    $obj->missing_method(); # Calls D::AUTOLOAD, B::AUTOLOAD, C::AUTOLOAD

    # Clean-up calls D::DESTROY, B::DESTROY, A::DESTROY, C::DESTROY


=head1 DESCRIPTION

NEXT.pm adds a pseudoclass named C<NEXT> to any program
that uses it. If a method C<m> calls C<$self->NEXT::m()>, the call to
C<m> is redispatched as if the calling method had not originally been found.

In other words, a call to C<$self->NEXT::m()> resumes the depth-first,
left-to-right search of C<$self>'s class hierarchy that resulted in the
original call to C<m>.

Note that this is not the same thing as C<$self->SUPER::m()>, which 
begins a new dispatch that is restricted to searching the ancestors
of the current class. C<$self->NEXT::m()> can backtrack
past the current class -- to look for a suitable method in other
ancestors of C<$self> -- whereas C<$self->SUPER::m()> cannot.

A typical use would be in the destructors of a class hierarchy,
as illustrated in the synopsis above. Each class in the hierarchy
has a DESTROY method that performs some class-specific action
and then redispatches the call up the hierarchy. As a result,
when an object of class D is destroyed, the destructors of I<all>
its parent classes are called (in depth-first, left-to-right order).

Another typical use of redispatch would be in C<AUTOLOAD>'ed methods.
If such a method determined that it was not able to handle a
particular call, it might choose to redispatch that call, in the
hope that some other C<AUTOLOAD> (above it, or to its left) might
do better.

By default, if a redispatch attempt fails to find another method
elsewhere in the objects class hierarchy, it quietly gives up and does
nothing (but see L<"Enforcing redispatch">). This gracious acquiesence
is also unlike the (generally annoying) behaviour of C<SUPER>, which
throws an exception if it cannot redispatch.

Note that it is a fatal error for any method (including C<AUTOLOAD>)
to attempt to redispatch any method that does not have the
same name. For example:

        sub D::oops { print "oops!\n"; $_[0]->NEXT::other_method() }


=head2 Enforcing redispatch

It is possible to make C<NEXT> redispatch more demandingly (i.e. like
C<SUPER> does), so that the redispatch throws an exception if it cannot
find a "next" method to call.

To do this, simple invoke the redispatch as:

	$self->NEXT::ACTUAL::method();

rather than:

	$self->NEXT::method();

The C<ACTUAL> tells C<NEXT> that there must actually be a next method to call,
or it should throw an exception.

C<NEXT::ACTUAL> is most commonly used in C<AUTOLOAD> methods, as a means to
decline an C<AUTOLOAD> request, but preserve the normal exception-on-failure 
semantics:

	sub AUTOLOAD {
		if ($AUTOLOAD =~ /foo|bar/) {
			# handle here
		}
		else {  # try elsewhere
			shift()->NEXT::ACTUAL::AUTOLOAD(@_);
		}
	}

By using C<NEXT::ACTUAL>, if there is no other C<AUTOLOAD> to handle the
method call, an exception will be thrown (as usually happens in the absence of
a suitable C<AUTOLOAD>).


=head2 Avoiding repetitions

If C<NEXT> redispatching is used in the methods of a "diamond" class hierarchy:

	#     A   B
	#    / \ /
	#   C   D
	#    \ /
	#     E

	use NEXT;

	package A;                 
	sub foo { print "called A::foo\n"; shift->NEXT::foo() }

	package B;                 
	sub foo { print "called B::foo\n"; shift->NEXT::foo() }

	package C; @ISA = qw( A );
	sub foo { print "called C::foo\n"; shift->NEXT::foo() }

	package D; @ISA = qw(A B);
	sub foo { print "called D::foo\n"; shift->NEXT::foo() }

	package E; @ISA = qw(C D);
	sub foo { print "called E::foo\n"; shift->NEXT::foo() }

	E->foo();

then derived classes may (re-)inherit base-class methods through two or
more distinct paths (e.g. in the way C<E> inherits C<A::foo> twice --
through C<C> and C<D>). In such cases, a sequence of C<NEXT> redispatches
will invoke the multiply inherited method as many times as it is
inherited. For example, the above code prints:

        called E::foo
        called C::foo
        called A::foo
        called D::foo
        called A::foo
        called B::foo

(i.e. C<A::foo> is called twice).

In some cases this I<may> be the desired effect within a diamond hierarchy,
but in others (e.g. for destructors) it may be more appropriate to 
call each method only once during a sequence of redispatches.

To cover such cases, you can redispatch methods via:

        $self->NEXT::UNSEEN::method();

rather than:

        $self->NEXT::method();

This causes the redispatcher to skip any classes in the hierarchy that it has
already visited in an earlier redispatch. So, for example, if the
previous example were rewritten:

        package A;                 
        sub foo { print "called A::foo\n"; shift->NEXT::UNSEEN::foo() }

        package B;                 
        sub foo { print "called B::foo\n"; shift->NEXT::UNSEEN::foo() }

        package C; @ISA = qw( A );
        sub foo { print "called C::foo\n"; shift->NEXT::UNSEEN::foo() }

        package D; @ISA = qw(A B);
        sub foo { print "called D::foo\n"; shift->NEXT::UNSEEN::foo() }

        package E; @ISA = qw(C D);
        sub foo { print "called E::foo\n"; shift->NEXT::UNSEEN::foo() }

        E->foo();

then it would print:
        
        called E::foo
        called C::foo
        called A::foo
        called D::foo
        called B::foo

and omit the second call to C<A::foo>.

Note that you can also use:

        $self->NEXT::UNSEEN::ACTUAL::method();

or:

        $self->NEXT::ACTUAL::UNSEEN::method();

to get both unique invocation I<and> exception-on-failure.


=head1 AUTHOR

Damian Conway (damian@conway.org)

=head1 BUGS AND IRRITATIONS

Because it's a module, not an integral part of the interpreter, NEXT.pm
has to guess where the surrounding call was found in the method
look-up sequence. In the presence of diamond inheritance patterns
it occasionally guesses wrong.

It's also too slow (despite caching).

Comment, suggestions, and patches welcome.

=head1 COPYRIGHT

 Copyright (c) 2000-2001, Damian Conway. All Rights Reserved.
 This module is free software. It may be used, redistributed
    and/or modified under the same terms as Perl itself.
