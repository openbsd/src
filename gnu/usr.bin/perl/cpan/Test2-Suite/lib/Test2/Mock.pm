package Test2::Mock;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak confess/;
our @CARP_NOT = (__PACKAGE__);

use Scalar::Util qw/weaken reftype blessed set_prototype/;
use Test2::Util qw/pkg_to_file/;
use Test2::Util::Stash qw/parse_symbol slot_to_sig get_symbol get_stash purge_symbol/;
use Test2::Util::Sub qw/gen_accessor gen_reader gen_writer/;

sub new; # Prevent hashbase from giving us 'new';
use Test2::Util::HashBase qw/class parent child _purge_on_destroy _blocked_load _symbols _track sub_tracking call_tracking/;

sub new {
    my $class = shift;

    croak "Called new() on a blessed instance, did you mean to call \$control->class->new()?"
        if blessed($class);

    my $self = bless({}, $class);

    $self->{+SUB_TRACKING}  ||= {};
    $self->{+CALL_TRACKING} ||= [];

    my @sets;
    while (my $arg = shift @_) {
        my $val = shift @_;

        if ($class->can(uc($arg))) {
            $self->{$arg} = $val;
            next;
        }

        push @sets => [$arg, $val];
    }

    croak "The 'class' field is required"
        unless $self->{+CLASS};

    for my $set (@sets) {
        my ($meth, $val) = @$set;
        my $type = reftype($val);

        confess "'$meth' is not a valid constructor argument for $class"
            unless $self->can($meth);

        if (!$type) {
            $self->$meth($val);
        }
        elsif($type eq 'HASH') {
            $self->$meth(%$val);
        }
        elsif($type eq 'ARRAY') {
            $self->$meth(@$val);
        }
        else {
            croak "'$val' is not a valid argument for '$meth'"
        }
    }

    return $self;
}

sub _check {
    return unless $_[0]->{+CHILD};
    croak "There is an active child controller, cannot proceed";
}

sub purge_on_destroy {
    my $self = shift;
    ($self->{+_PURGE_ON_DESTROY}) = @_ if @_;
    return $self->{+_PURGE_ON_DESTROY};
}

sub stash {
    my $self = shift;
    get_stash($self->{+CLASS});
}

sub file {
    my $self = shift;
    my $file = $self->class;
    return pkg_to_file($self->class);
}

sub block_load {
    my $self = shift;
    $self->_check();

    my $file = $self->file;

    croak "Cannot block the loading of module '" . $self->class . "', already loaded in file $INC{$file}"
        if $INC{$file};

    $INC{$file} = __FILE__;

    $self->{+_BLOCKED_LOAD} = 1;
}

my %NEW = (
    hash => sub {
        my ($class, %params) = @_;
        return bless \%params, $class;
    },
    array => sub {
        my ($class, @params) = @_;
        return bless \@params, $class;
    },
    ref => sub {
        my ($class, $params) = @_;
        return bless $params, $class;
    },
    ref_copy => sub {
        my ($class, $params) = @_;
        my $type = reftype($params);

        return bless {%$params}, $class
            if $type eq 'HASH';

        return bless [@$params], $class
            if $type eq 'ARRAY';

        croak "Not sure how to construct an '$class' from '$params'";
    },
);

sub override_constructor {
    my $self = shift;
    my ($name, $type) = @_;
    $self->_check();

    my $sub = $NEW{$type}
        || croak "'$type' is not a known constructor type";

    $self->override($name => $sub);
}

sub add_constructor {
    my $self = shift;
    my ($name, $type) = @_;
    $self->_check();

    my $sub = $NEW{$type}
        || croak "'$type' is not a known constructor type";

    $self->add($name => $sub);
}

sub autoload {
    my $self = shift;
    $self->_check();
    my $class = $self->class;
    my $stash = $self->stash;

    croak "Class '$class' already has an AUTOLOAD"
        if $stash->{AUTOLOAD} && *{$stash->{AUTOLOAD}}{CODE};
    croak "Class '$class' already has an can"
        if $stash->{can} && *{$stash->{can}}{CODE};

    # Weaken this reference so that AUTOLOAD does not prevent its own
    # destruction.
    weaken(my $c = $self);

    my ($file, $line) = (__FILE__, __LINE__ + 3);
    my $autoload = eval <<EOT || die "Failed generating AUTOLOAD sub: $@";
package $class;
#line $line "$file (Generated AUTOLOAD)"
our \$AUTOLOAD;
    sub {
        my (\$self) = \@_;
        my (\$pkg, \$name) = (\$AUTOLOAD =~ m/^(.*)::([^:]+)\$/g);
        \$AUTOLOAD = undef;

        return if \$name eq 'DESTROY';
        my \$sub = sub {
            my \$self = shift;
            (\$self->{\$name}) = \@_ if \@_;
            return \$self->{\$name};
        };

        \$c->add(\$name => \$sub);

        if (\$c->{_track}) {
            my \$call = {sub_name => \$name, sub_ref => \$sub, args => [\@_]};
            push \@{\$c->{sub_tracking}->{\$name}} => \$call;
            push \@{\$c->{call_tracking}} => \$call;
        }

        goto &\$sub;
    }
EOT

    $line = __LINE__ + 3;
    my $can = eval <<EOT || die "Failed generating can method: $@";
package $class;
#line $line "$file (Generated can)"
use Scalar::Util 'reftype';
    sub {
        my (\$self, \$meth) = \@_;
        if (\$self->SUPER::can(\$meth)) {
            return \$self->SUPER::can(\$meth);
        }
        elsif (ref \$self && reftype \$self eq 'HASH' && exists \$self->{\$meth}) {
            return sub { shift->\$meth(\@_) };
        }
        return undef;
    }
EOT

    {
        local $self->{+_TRACK} = 0;
        $self->add(AUTOLOAD => $autoload);
        $self->add(can => $can);
    }
}

sub before {
    my $self = shift;
    my ($name, $sub) = @_;
    $self->_check();
    my $orig = $self->current($name, required => 1);
    $self->_inject({}, $name => set_prototype(sub { $sub->(@_); $orig->(@_) }, prototype $sub));
}

sub after {
    my $self = shift;
    my ($name, $sub) = @_;
    $self->_check();
    my $orig = $self->current($name, required => 1);
    $self->_inject(
        {},
        $name => set_prototype(
            sub {
                my @out;

                my $want = wantarray;

                if ($want) {
                    @out = $orig->(@_);
                }
                elsif (defined $want) {
                    $out[0] = $orig->(@_);
                }
                else {
                    $orig->(@_);
                }

                $sub->(@_);

                return @out    if $want;
                return $out[0] if defined $want;
                return;
            },
            prototype $sub,
        )
    );
}

sub around {
    my $self = shift;
    my ($name, $sub) = @_;
    $self->_check();
    my $orig = $self->current($name, required => 1);
    $self->_inject({}, $name => set_prototype(sub { $sub->($orig, @_) }, prototype $sub));
}

sub add {
    my $self = shift;
    $self->_check();
    $self->_inject({add => 1}, @_);
}

sub override {
    my $self = shift;
    $self->_check();
    $self->_inject({}, @_);
}

sub set {
    my $self = shift;
    $self->_check();
    $self->_inject({set => 1}, @_);
}

sub current {
    my $self = shift;
    my ($sym, %params) = @_;

    my $out = get_symbol($sym, $self->{+CLASS});
    return $out unless $params{required};
    confess "Attempt to modify a sub that does not exist '$self->{+CLASS}\::$sym' (Mock operates on packages, not classes, are you looking for a symbol in a parent class?)"
        unless $out;
    return $out;
}

sub orig {
    my $self = shift;
    my ($sym) = @_;

    $sym = "&$sym" unless $sym =~ m/^[&\$\%\@]/;

    my $syms = $self->{+_SYMBOLS}
        or croak "No symbols have been mocked yet";

    my $ref = $syms->{$sym};

    croak "Symbol '$sym' is not mocked"
        unless $ref && @$ref;

    my ($orig) = @$ref;

    return $orig;
}

sub track {
    my $self = shift;

    ($self->{+_TRACK}) = @_ if @_;

    return $self->{+_TRACK};
}

sub clear_call_tracking { @{shift->{+CALL_TRACKING}} = () }

sub clear_sub_tracking {
    my $self = shift;

    unless (@_) {
        %{$self->{+SUB_TRACKING}} = ();
        return;
    }

    for my $item (@_) {
        delete $self->{+SUB_TRACKING}->{$item};
    }

    return;
}

sub _parse_inject {
    my $self = shift;
    my ($param, $arg) = @_;

    if ($param =~ m/^-(.*)$/) {
        my $sym = $1;
        my $sig = slot_to_sig(reftype($arg));
        my $ref = $arg;
        return ($sig, $sym, $ref);
    }

    return ('&', $param, $arg)
        if ref($arg) && reftype($arg) eq 'CODE';

    my ($is, $field, $val);

    if(defined($arg) && !ref($arg) && $arg =~ m/^(rw|ro|wo)$/) {
        $is    = $arg;
        $field = $param;
    }
    elsif (!ref($arg)) {
        $val = $arg;
        $is  = 'val';
    }
    elsif (reftype($arg) eq 'HASH') {
        $field = delete $arg->{field} || $param;

        $val = delete $arg->{val};
        $is  = delete $arg->{is};

        croak "Cannot specify 'is' and 'val' together" if $val && $is;

        $is ||= $val ? 'val' : 'rw';

        croak "The following keys are not valid when defining a mocked sub with a hashref: " . join(", " => keys %$arg)
            if keys %$arg;
    }
    else {
        confess "'$arg' is not a valid argument when defining a mocked sub";
    }

    my $sub;
    if ($is eq 'rw') {
        $sub = gen_accessor($field);
    }
    elsif ($is eq 'ro') {
        $sub = gen_reader($field);
    }
    elsif ($is eq 'wo') {
        $sub = gen_writer($field);
    }
    else { # val
        $sub = sub { $val };
    }

    return ('&', $param, $sub);
}

sub _inject {
    my $self = shift;
    my ($params, @pairs) = @_;

    my $add = $params->{add};
    my $set = $params->{set};

    my $class = $self->{+CLASS};

    $self->{+_SYMBOLS} ||= {};
    my $syms = $self->{+_SYMBOLS};

    while (my $param = shift @pairs) {
        my $arg = shift @pairs;
        my ($sig, $sym, $ref) = $self->_parse_inject($param, $arg);
        my $orig = $self->current("$sig$sym");

        croak "Cannot override '$sig$class\::$sym', symbol is not already defined"
            unless $orig || $add || $set || ($sig eq '&' && $class->can($sym));

        # Cannot be too sure about scalars in globs
        croak "Cannot add '$sig$class\::$sym', symbol is already defined"
            if $add && $orig
            && (reftype($orig) ne 'SCALAR' || defined($$orig));

        $syms->{"$sig$sym"} ||= [];
        push @{$syms->{"$sig$sym"}} => $orig; # Might be undef, thats expected

        if ($self->{+_TRACK} && $sig eq '&') {
            my $sub_tracker  = $self->{+SUB_TRACKING};
            my $call_tracker = $self->{+CALL_TRACKING};
            my $sub = $ref;
            $ref = sub {
                my $call = {sub_name => $sym, sub_ref => $sub, args => [@_]};
                push @{$sub_tracker->{$param}} => $call;
                push @$call_tracker => $call;
                goto &$sub;
            };
        }

        no strict 'refs';
        no warnings 'redefine';
        *{"$class\::$sym"} = $ref;
    }

    return;
}

sub _set_or_unset {
    my $self = shift;
    my ($symbol, $set) = @_;

    my $class = $self->{+CLASS};

    return purge_symbol($symbol, $class)
        unless $set;

    my $sym = parse_symbol($symbol, $class);
    no strict 'refs';
    no warnings 'redefine';
    *{"$class\::$sym->{name}"} = $set;
}

sub restore {
    my $self = shift;
    my ($sym) = @_;
    $self->_check();

    $sym = "&$sym" unless $sym =~ m/^[&\$\%\@]/;

    my $syms = $self->{+_SYMBOLS}
        or croak "No symbols are mocked";

    my $ref = $syms->{$sym};

    croak "Symbol '$sym' is not mocked"
        unless $ref && @$ref;

    my $old = pop @$ref;
    delete $syms->{$sym} unless @$ref;

    return $self->_set_or_unset($sym, $old);
}

sub reset {
    my $self = shift;
    my ($sym) = @_;
    $self->_check();

    $sym = "&$sym" unless $sym =~ m/^[&\$\%\@]/;

    my $syms = $self->{+_SYMBOLS}
        or croak "No symbols are mocked";

    my $ref = delete $syms->{$sym};

    croak "Symbol '$sym' is not mocked"
        unless $ref && @$ref;

    my ($old) = @$ref;

    return $self->_set_or_unset($sym, $old);
}

sub reset_all {
    my $self = shift;
    $self->_check();

    my $syms = $self->{+_SYMBOLS} || return;

    $self->reset($_) for keys %$syms;

    delete $self->{+_SYMBOLS};
}

sub _purge {
    my $self = shift;
    my $stash = $self->stash;
    delete $stash->{$_} for keys %$stash;
}

sub DESTROY {
    my $self = shift;

    delete $self->{+CHILD};
    $self->reset_all if $self->{+_SYMBOLS};

    delete $INC{$self->file} if $self->{+_BLOCKED_LOAD};

    $self->_purge if $self->{+_PURGE_ON_DESTROY};
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Mock - Module for managing mocked classes and instances.

=head1 DESCRIPTION

This module lets you add and override methods for any package temporarily. When
the instance is destroyed it will restore the package to its original state.

=head1 SYNOPSIS

    use Test2::Mock;
    use MyClass;

    my $mock = Test2::Mock->new(
        track => $BOOL, # enable call tracking if desired
        class => 'MyClass',
        override => [
            name => sub { 'fred' },
            ...
        ],
        add => [
            is_mocked => sub { 1 }
            ...
        ],
        ...
    );

    # Unmock the 'name' sub
    $mock->restore('name');

    ...

    $mock = undef; # Will remove all the mocking

=head1 CONSTRUCTION

=head1 METHODS

=over 4

=item $mock = Test2::Mock->new(class => $CLASS, ...)

This will create a new instance of L<Test2::Mock> that manages mocking
for the specified C<$CLASS>.

Any C<Test2::Mock> method can be used as a constructor argument, each
should be followed by an arrayref of arguments to be used within the method. For
instance the C<add()> method:

    my $mock = Test2::Mock->new(
        class => 'AClass',
        add => [foo => sub { 'foo' }],
    );

is identical to this:

    my $mock = Test2::Mock->new(
        class => 'AClass',
    );
    $mock->add(foo => sub { 'foo' });

=item $mock->track($bool)

Turn tracking on or off. Any sub added/overridden/set when tracking is on will
log every call in a hash retrievable via C<< $mock->tracking >>. Changing the
tracking toggle will not affect subs already altered, but will affect any
additional alterations.

=item $hashref = $mock->sub_tracking

The tracking data looks like this:

    {
        sub_name => [
            {sub_name => $sub_name, sub_ref => $mock_subref, args => [... copy of @_ from the call ... ]},
            ...,
            ...,
        ],
    }

Unlike call_tracking, this lists all calls by sub, so you can choose to only
look at the sub specific calls.

B<Please note:> The hashref items with the subname and args are shared with
call_tracking, modifying one modifies the other, so copy first!

=item $arrayref = $mock->call_tracking

The tracking data looks like this:

    [
        {sub_name => $sub_name, sub_ref => $mock_subref, args => [... copy of @_ from the call ... ]},
        ...,
        ...,
    ]

Unlike sub_tracking this lists all calls to any mocked sub, in the order they
were called. To filter by sub use sub_tracking.

B<Please note:> The hashref items with the subname and args are shared with
sub_tracking, modifying one modifies the other, so copy first!

=item $mock->clear_sub_tracking()

=item $mock->clear_sub_tracking(\@subnames)

Clear tracking data. With no arguments ALL tracking data is cleared. When
arguments are provided then only those specific keys will be cleared.

=item $mock->clear_call_tracking()

Clear all items from call_tracking.

=item $mock->add('symbol' => ..., 'symbol2' => ...)

=item $mock->override('symbol1' => ..., 'symbol2' => ...)

=item $mock->set('symbol1' => ..., 'symbol2' => ...)

C<add()> and C<override()> are the primary ways to add/modify methods for a
class. Both accept the exact same type of arguments. The difference is that
C<override> will fail unless the symbol you are overriding already exists,
C<add> on the other hand will fail if the symbol does already exist.

C<set()> was more recently added for cases where you may not know if the sub
already exists. These cases are rare, and set should be avoided (think of it
like 'no strict'). However there are valid use cases, so it was added.

B<Note:> Think of override as a push operation. If you call override on the
same symbol multiple times it will track that. You can use C<restore()> as a
pop operation to go back to the previous mock. C<reset> can be used to remove
all the mocking for a symbol.

Arguments must be a symbol name, with optional sigil, followed by a new
specification of the symbol. If no sigil is specified then '&' (sub) is
assumed. A simple example of overriding a sub:

    $mock->override(foo => sub { 'overridden foo' });
    my $val = $class->foo; # Runs our override
    # $val is now set to 'overridden foo'

You can also simply provide a value and it will be wrapped in a sub for you:

    $mock->override( foo => 'foo' );

The example above will generate a sub that always returns the string 'foo'.

There are three *special* values that can be used to generate accessors:

    $mock->add(
        name => 'rw',   # Generates a read/write accessor
        age  => 'ro',   # Generates a read only accessor
        size => 'wo',   # Generates a write only accessor
    );

If you want to have a sub that actually returns one of the three special strings, or
that returns a coderef, you can use a hashref as the spec:

    my $ref = sub { 'my sub' };
    $mock->add(
        rw_string => { val => 'rw' },
        ro_string => { val => 'ro' },
        wo_string => { val => 'wo' },
        coderef   => { val => $ref }, # the coderef method returns $ref each time
    );

You can also override/add other symbol types, such as hash:

    package Foo;
    ...

    $mock->add('%foo' => {a => 1});

    print $Foo::foo{a}; # prints '1'

You can also tell mock to deduce the symbol type for the add/override from the
reference, rules are similar to glob assignments:

    $mock->add(
        -foo => sub { 'foo' },     # Adds the &foo sub to the package
        -foo => { foo => 1 },      # Adds the %foo hash to the package
        -foo => [ 'f', 'o', 'o' ], # Adds the @foo array to the package
        -foo => \"foo",            # Adds the $foo scalar to the package
    );

=item $mock->restore($SYMBOL)

Restore the symbol to what it was before the last override. If the symbol was
recently added this will remove it. If the symbol has been overridden multiple
times this will ONLY restore it to the previous state. Think of C<override> as a
push operation, and C<restore> as the pop operation.

=item $mock->reset($SYMBOL)

Remove all mocking of the symbol and restore the original symbol. If the symbol
was initially added then it will be completely removed.

=item $mock->orig($SYMBOL)

This will return the original symbol, before any mocking. For symbols that were
added this will return undef.

=item $mock->current($SYMBOL)

This will return the current symbol.

=item $mock->reset_all

Remove all added symbols, and restore all overridden symbols to their originals.

=item $mock->add_constructor($NAME => $TYPE)

=item $mock->override_constructor($NAME => $TYPE)

This can be used to inject constructors. The first argument should be the name
of the constructor. The second argument specifies the constructor type.

The C<hash> type is the most common, all arguments are used to create a new
hash that is blessed.

    hash => sub  {
        my ($class, %params) = @_;
        return bless \%params, $class;
    };

The C<array> type is similar to the hash type, but accepts a list instead of
key/value pairs:

    array => sub {
        my ($class, @params) = @_;
        return bless \@params, $class;
    };

The C<ref> type takes a reference and blesses it. This will modify your
original input argument.

    ref => sub {
        my ($class, $params) = @_;
        return bless $params, $class;
    };

The C<ref_copy> type will copy your reference and bless the copy:

    ref_copy => sub {
        my ($class, $params) = @_;
        my $type = reftype($params);

        return bless {%$params}, $class
            if $type eq 'HASH';

        return bless [@$params], $class
            if $type eq 'ARRAY';

        croak "Not sure how to construct a '$class' from '$params'";
    };

=item $mock->before($NAME, sub { ... })

This will replace the original sub C<$NAME> with a new sub that calls your
custom code just before calling the original method. The return from your
custom sub is ignored. Your sub and the original both get the unmodified
arguments.

=item $mock->after($NAME, sub { ... })

This is similar to before, except your callback runs after the original code.
The return from your callback is ignored.

=item $mock->around($NAME, sub { ... })

This gives you the chance to wrap the original sub:

    $mock->around(foo => sub {
        my $orig = shift;
        my $self = shift;
        my (@args) = @_;

        ...
        $self->$orig(@args);
        ...

        return ...;
    });

The original sub is passed in as the first argument, even before C<$self>. You
are responsible for making sure your wrapper sub returns the correct thing.

=item $mock->autoload

This will inject an C<AUTOLOAD> sub into the class. This autoload will
automatically generate read-write accessors for any sub called that does not
already exist.

=item $mock->block_load

This will prevent the real class from loading until the mock is destroyed. This
will fail if the class is already loaded. This will let you mock a class
completely without loading the original module.

=item $pm_file = $mock->file

This returns the relative path to the file for the module. This corresponds to
the C<%INC> entry.

=item $bool = $mock->purge_on_destroy($bool)

When true, this will cause the package stash to be completely obliterated when
the mock object falls out of scope or is otherwise destroyed. You do not
normally want this.

=item $stash = $mock->stash

This returns the stash for the class being mocked. This is the equivalent of:

    my $stash = \%{"${class}\::"};

This saves you from needing to turn off strict.

=item $class = $mock->class

The class being mocked by this instance.

=item $p = $mock->parent

If you mock a class twice the first instance is the parent, the second is the
child. This prevents the parent from being destroyed before the child, which
would lead to a very unpleasant situation.

=item $c = $mock->child

Returns the child mock, if any.

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
L<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<https://dev.perl.org/licenses/>

=cut
