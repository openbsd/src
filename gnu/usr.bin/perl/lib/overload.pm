package overload;

sub nil {}

sub OVERLOAD {
  $package = shift;
  my %arg = @_;
  my ($sub, $fb);
  $ {$package . "::OVERLOAD"}{dummy}++; # Register with magic by touching.
  *{$package . "::()"} = \&nil; # Make it findable via fetchmethod.
  for (keys %arg) {
    if ($_ eq 'fallback') {
      $fb = $arg{$_};
    } else {
      $sub = $arg{$_};
      if (not ref $sub and $sub !~ /::/) {
	$ {$package . "::(" . $_} = $sub;
	$sub = \&nil;
      }
      #print STDERR "Setting `$ {'package'}::\cO$_' to \\&`$sub'.\n";
      *{$package . "::(" . $_} = \&{ $sub };
    }
  }
  ${$package . "::()"} = $fb; # Make it findable too (fallback only).
}

sub import {
  $package = (caller())[0];
  # *{$package . "::OVERLOAD"} = \&OVERLOAD;
  shift;
  $package->overload::OVERLOAD(@_);
}

sub unimport {
  $package = (caller())[0];
  ${$package . "::OVERLOAD"}{dummy}++; # Upgrade the table
  shift;
  for (@_) {
    if ($_ eq 'fallback') {
      undef $ {$package . "::()"};
    } else {
      delete $ {$package . "::"}{"(" . $_};
    }
  }
}

sub Overloaded {
  my $package = shift;
  $package = ref $package if ref $package;
  $package->can('()');
}

sub ov_method {
  my $globref = shift;
  return undef unless $globref;
  my $sub = \&{*$globref};
  return $sub if $sub ne \&nil;
  return shift->can($ {*$globref});
}

sub OverloadedStringify {
  my $package = shift;
  $package = ref $package if ref $package;
  #$package->can('(""')
  ov_method mycan($package, '(""'), $package;
}

sub Method {
  my $package = shift;
  $package = ref $package if ref $package;
  #my $meth = $package->can('(' . shift);
  ov_method mycan($package, '(' . shift), $package;
  #return $meth if $meth ne \&nil;
  #return $ {*{$meth}};
}

sub AddrRef {
  my $package = ref $_[0];
  return "$_[0]" unless $package;
  bless $_[0], overload::Fake;	# Non-overloaded package
  my $str = "$_[0]";
  bless $_[0], $package;	# Back
  $package . substr $str, index $str, '=';
}

sub StrVal {
  (OverloadedStringify($_[0])) ?
    (AddrRef(shift)) :
    "$_[0]";
}

sub mycan {				# Real can would leave stubs.
  my ($package, $meth) = @_;
  return \*{$package . "::$meth"} if defined &{$package . "::$meth"};
  my $p;
  foreach $p (@{$package . "::ISA"}) {
    my $out = mycan($p, $meth);
    return $out if $out;
  }
  return undef;
}

1;

__END__

=head1 NAME 

overload - Package for overloading perl operations

=head1 SYNOPSIS

    package SomeThing;

    use overload 
	'+' => \&myadd,
	'-' => \&mysub;
	# etc
    ...

    package main;
    $a = new SomeThing 57;
    $b=5+$a;
    ...
    if (overload::Overloaded $b) {...}
    ...
    $strval = overload::StrVal $b;

=head1 CAVEAT SCRIPTOR

Overloading of operators is a subject not to be taken lightly.
Neither its precise implementation, syntax, nor semantics are
100% endorsed by Larry Wall.  So any of these may be changed 
at some point in the future.

=head1 DESCRIPTION

=head2 Declaration of overloaded functions

The compilation directive

    package Number;
    use overload
	"+" => \&add, 
	"*=" => "muas";

declares function Number::add() for addition, and method muas() in
the "class" C<Number> (or one of its base classes)
for the assignment form C<*=> of multiplication.  

Arguments of this directive come in (key, value) pairs.  Legal values
are values legal inside a C<&{ ... }> call, so the name of a
subroutine, a reference to a subroutine, or an anonymous subroutine
will all work.  Note that values specified as strings are
interpreted as methods, not subroutines.  Legal keys are listed below.

The subroutine C<add> will be called to execute C<$a+$b> if $a
is a reference to an object blessed into the package C<Number>, or if $a is
not an object from a package with defined mathemagic addition, but $b is a
reference to a C<Number>.  It can also be called in other situations, like
C<$a+=7>, or C<$a++>.  See L<MAGIC AUTOGENERATION>.  (Mathemagical
methods refer to methods triggered by an overloaded mathematical
operator.)

Since overloading respects inheritance via the @ISA hierarchy, the
above declaration would also trigger overloading of C<+> and C<*=> in
all the packages which inherit from C<Number>.

=head2 Calling Conventions for Binary Operations

The functions specified in the C<use overload ...> directive are called
with three (in one particular case with four, see L<Last Resort>)
arguments.  If the corresponding operation is binary, then the first
two arguments are the two arguments of the operation.  However, due to
general object calling conventions, the first argument should always be
an object in the package, so in the situation of C<7+$a>, the
order of the arguments is interchanged.  It probably does not matter
when implementing the addition method, but whether the arguments
are reversed is vital to the subtraction method.  The method can
query this information by examining the third argument, which can take
three different values:

=over 7

=item FALSE

the order of arguments is as in the current operation.

=item TRUE

the arguments are reversed.

=item C<undef>

the current operation is an assignment variant (as in
C<$a+=7>), but the usual function is called instead.  This additional
information can be used to generate some optimizations.

=back

=head2 Calling Conventions for Unary Operations

Unary operation are considered binary operations with the second
argument being C<undef>.  Thus the functions that overloads C<{"++"}>
is called with arguments C<($a,undef,'')> when $a++ is executed.

=head2 Overloadable Operations

The following symbols can be specified in C<use overload>:

=over 5

=item * I<Arithmetic operations>

    "+", "+=", "-", "-=", "*", "*=", "/", "/=", "%", "%=",
    "**", "**=", "<<", "<<=", ">>", ">>=", "x", "x=", ".", ".=",

For these operations a substituted non-assignment variant can be called if
the assignment variant is not available.  Methods for operations "C<+>",
"C<->", "C<+=>", and "C<-=>" can be called to automatically generate
increment and decrement methods.  The operation "C<->" can be used to
autogenerate missing methods for unary minus or C<abs>.

=item * I<Comparison operations>

    "<",  "<=", ">",  ">=", "==", "!=", "<=>",
    "lt", "le", "gt", "ge", "eq", "ne", "cmp",

If the corresponding "spaceship" variant is available, it can be
used to substitute for the missing operation.  During C<sort>ing
arrays, C<cmp> is used to compare values subject to C<use overload>.

=item * I<Bit operations>

    "&", "^", "|", "neg", "!", "~",

"C<neg>" stands for unary minus.  If the method for C<neg> is not
specified, it can be autogenerated using the method for
subtraction. If the method for "C<!>" is not specified, it can be
autogenerated using the methods for "C<bool>", or "C<\"\">", or "C<0+>".

=item * I<Increment and decrement>

    "++", "--",

If undefined, addition and subtraction methods can be
used instead.  These operations are called both in prefix and
postfix form.

=item * I<Transcendental functions>

    "atan2", "cos", "sin", "exp", "abs", "log", "sqrt",

If C<abs> is unavailable, it can be autogenerated using methods
for "E<lt>" or "E<lt>=E<gt>" combined with either unary minus or subtraction.

=item * I<Boolean, string and numeric conversion>

    "bool", "\"\"", "0+",

If one or two of these operations are unavailable, the remaining ones can
be used instead.  C<bool> is used in the flow control operators
(like C<while>) and for the ternary "C<?:>" operation.  These functions can
return any arbitrary Perl value.  If the corresponding operation for this value
is overloaded too, that operation will be called again with this value.

=item * I<Special>

    "nomethod", "fallback", "=",

see L<SPECIAL SYMBOLS FOR C<use overload>>.

=back

See L<"Fallback"> for an explanation of when a missing method can be autogenerated.

=head2 Inheritance and overloading

Inheritance interacts with overloading in two ways.

=over

=item Strings as values of C<use overload> directive

If C<value> in

  use overload key => value;

is a string, it is interpreted as a method name.

=item Overloading of an operation is inherited by derived classes

Any class derived from an overloaded class is also overloaded.  The
set of overloaded methods is the union of overloaded methods of all
the ancestors. If some method is overloaded in several ancestor, then
which description will be used is decided by the usual inheritance
rules:

If C<A> inherits from C<B> and C<C> (in this order), C<B> overloads
C<+> with C<\&D::plus_sub>, and C<C> overloads C<+> by C<"plus_meth">,
then the subroutine C<D::plus_sub> will be called to implement
operation C<+> for an object in package C<A>.

=back

Note that since the value of the C<fallback> key is not a subroutine,
its inheritance is not governed by the above rules.  In the current
implementation, the value of C<fallback> in the first overloaded
ancestor is used, but this is accidental and subject to change.

=head1 SPECIAL SYMBOLS FOR C<use overload>

Three keys are recognized by Perl that are not covered by the above
description.

=head2 Last Resort

C<"nomethod"> should be followed by a reference to a function of four
parameters.  If defined, it is called when the overloading mechanism
cannot find a method for some operation.  The first three arguments of
this function coincide with the arguments for the corresponding method if
it were found, the fourth argument is the symbol
corresponding to the missing method.  If several methods are tried,
the last one is used.  Say, C<1-$a> can be equivalent to

	&nomethodMethod($a,1,1,"-")

if the pair C<"nomethod" =E<gt> "nomethodMethod"> was specified in the
C<use overload> directive.

If some operation cannot be resolved, and there is no function
assigned to C<"nomethod">, then an exception will be raised via die()--
unless C<"fallback"> was specified as a key in C<use overload> directive.

=head2 Fallback 

The key C<"fallback"> governs what to do if a method for a particular
operation is not found.  Three different cases are possible depending on
the value of C<"fallback">:

=over 16

=item * C<undef>

Perl tries to use a
substituted method (see L<MAGIC AUTOGENERATION>).  If this fails, it
then tries to calls C<"nomethod"> value; if missing, an exception
will be raised.

=item * TRUE

The same as for the C<undef> value, but no exception is raised.  Instead,
it silently reverts to what it would have done were there no C<use overload>
present.

=item * defined, but FALSE

No autogeneration is tried.  Perl tries to call
C<"nomethod"> value, and if this is missing, raises an exception. 

=back

B<Note.> C<"fallback"> inheritance via @ISA is not carved in stone
yet, see L<"Inheritance and overloading">.

=head2 Copy Constructor

The value for C<"="> is a reference to a function with three
arguments, i.e., it looks like the other values in C<use
overload>. However, it does not overload the Perl assignment
operator. This would go against Camel hair.

This operation is called in the situations when a mutator is applied
to a reference that shares its object with some other reference, such
as

	$a=$b; 
	$a++;

To make this change $a and not change $b, a copy of C<$$a> is made,
and $a is assigned a reference to this new object.  This operation is
done during execution of the C<$a++>, and not during the assignment,
(so before the increment C<$$a> coincides with C<$$b>).  This is only
done if C<++> is expressed via a method for C<'++'> or C<'+='>.  Note
that if this operation is expressed via C<'+'> a nonmutator, i.e., as
in

	$a=$b; 
	$a=$a+1;

then C<$a> does not reference a new copy of C<$$a>, since $$a does not
appear as lvalue when the above code is executed.

If the copy constructor is required during the execution of some mutator,
but a method for C<'='> was not specified, it can be autogenerated as a
string copy if the object is a plain scalar.

=over 5

=item B<Example>

The actually executed code for 

	$a=$b; 
        Something else which does not modify $a or $b....
	++$a;

may be

	$a=$b; 
        Something else which does not modify $a or $b....
	$a = $a->clone(undef,"");
        $a->incr(undef,"");

if $b was mathemagical, and C<'++'> was overloaded with C<\&incr>,
C<'='> was overloaded with C<\&clone>.

=back

=head1 MAGIC AUTOGENERATION

If a method for an operation is not found, and the value for  C<"fallback"> is
TRUE or undefined, Perl tries to autogenerate a substitute method for
the missing operation based on the defined operations.  Autogenerated method
substitutions are possible for the following operations:

=over 16

=item I<Assignment forms of arithmetic operations>

C<$a+=$b> can use the method for C<"+"> if the method for C<"+=">
is not defined.

=item I<Conversion operations> 

String, numeric, and boolean conversion are calculated in terms of one
another if not all of them are defined.

=item I<Increment and decrement>

The C<++$a> operation can be expressed in terms of C<$a+=1> or C<$a+1>,
and C<$a--> in terms of C<$a-=1> and C<$a-1>.

=item C<abs($a)>

can be expressed in terms of C<$aE<lt>0> and C<-$a> (or C<0-$a>).

=item I<Unary minus>

can be expressed in terms of subtraction.

=item I<Negation>

C<!> and C<not> can be expressed in terms of boolean conversion, or
string or numerical conversion.

=item I<Concatenation>

can be expressed in terms of string conversion.

=item I<Comparison operations> 

can be expressed in terms of its "spaceship" counterpart: either
C<E<lt>=E<gt>> or C<cmp>:

    <, >, <=, >=, ==, != 	in terms of <=>
    lt, gt, le, ge, eq, ne 	in terms of cmp

=item I<Copy operator>

can be expressed in terms of an assignment to the dereferenced value, if this
value is a scalar and not a reference.

=back

=head1 WARNING

The restriction for the comparison operation is that even if, for example,
`C<cmp>' should return a blessed reference, the autogenerated `C<lt>'
function will produce only a standard logical value based on the
numerical value of the result of `C<cmp>'.  In particular, a working
numeric conversion is needed in this case (possibly expressed in terms of
other conversions).

Similarly, C<.=>  and C<x=> operators lose their mathemagical properties
if the string conversion substitution is applied.

When you chop() a mathemagical object it is promoted to a string and its
mathemagical properties are lost.  The same can happen with other
operations as well.

=head1 Run-time Overloading

Since all C<use> directives are executed at compile-time, the only way to
change overloading during run-time is to

    eval 'use overload "+" => \&addmethod';

You can also use

    eval 'no overload "+", "--", "<="';

though the use of these constructs during run-time is questionable.

=head1 Public functions

Package C<overload.pm> provides the following public functions:

=over 5

=item overload::StrVal(arg)

Gives string value of C<arg> as in absence of stringify overloading.

=item overload::Overloaded(arg)

Returns true if C<arg> is subject to overloading of some operations.

=item overload::Method(obj,op)

Returns C<undef> or a reference to the method that implements C<op>.

=back

=head1 IMPLEMENTATION

What follows is subject to change RSN.

The table of methods for all operations is cached in magic for the
symbol table hash for the package.  The cache is invalidated during
processing of C<use overload>, C<no overload>, new function
definitions, and changes in @ISA. However, this invalidation remains
unprocessed until the next C<bless>ing into the package. Hence if you
want to change overloading structure dynamically, you'll need an
additional (fake) C<bless>ing to update the table.

(Every SVish thing has a magic queue, and magic is an entry in that
queue.  This is how a single variable may participate in multiple
forms of magic simultaneously.  For instance, environment variables
regularly have two forms at once: their %ENV magic and their taint
magic. However, the magic which implements overloading is applied to
the stashes, which are rarely used directly, thus should not slow down
Perl.)

If an object belongs to a package using overload, it carries a special
flag.  Thus the only speed penalty during arithmetic operations without
overloading is the checking of this flag.

In fact, if C<use overload> is not present, there is almost no overhead
for overloadable operations, so most programs should not suffer
measurable performance penalties.  A considerable effort was made to
minimize the overhead when overload is used in some package, but the
arguments in question do not belong to packages using overload.  When
in doubt, test your speed with C<use overload> and without it.  So far
there have been no reports of substantial speed degradation if Perl is
compiled with optimization turned on.

There is no size penalty for data if overload is not used. The only
size penalty if overload is used in some package is that I<all> the
packages acquire a magic during the next C<bless>ing into the
package. This magic is three-words-long for packages without
overloading, and carries the cache tabel if the package is overloaded.

Copying (C<$a=$b>) is shallow; however, a one-level-deep copying is 
carried out before any operation that can imply an assignment to the
object $a (or $b) refers to, like C<$a++>.  You can override this
behavior by defining your own copy constructor (see L<"Copy Constructor">).

It is expected that arguments to methods that are not explicitly supposed
to be changed are constant (but this is not enforced).

=head1 AUTHOR

Ilya Zakharevich E<lt>F<ilya@math.mps.ohio-state.edu>E<gt>.

=head1 DIAGNOSTICS

When Perl is run with the B<-Do> switch or its equivalent, overloading
induces diagnostic messages.

Using the C<m> command of Perl debugger (see L<perldebug>) one can
deduce which operations are overloaded (and which ancestor triggers
this overloading). Say, if C<eq> is overloaded, then the method C<(eq>
is shown by debugger. The method C<()> corresponds to the C<fallback>
key (in fact a presence of this method shows that this package has
overloading enabled, and it is what is used by the C<Overloaded>
function).

=head1 BUGS

Because it is used for overloading, the per-package hash %OVERLOAD now
has a special meaning in Perl. The symbol table is filled with names
looking like line-noise.

For the purpose of inheritance every overloaded package behaves as if
C<fallback> is present (possibly undefined). This may create
interesting effects if some package is not overloaded, but inherits
from two overloaded packages.

This document is confusing.

=cut

