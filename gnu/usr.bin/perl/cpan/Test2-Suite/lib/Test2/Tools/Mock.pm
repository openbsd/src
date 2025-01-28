package Test2::Tools::Mock;
use strict;
use warnings;

use Carp qw/croak/;
use Scalar::Util qw/blessed reftype weaken/;
use Test2::Util qw/try/;
use Test2::Util::Sub qw/gen_accessor gen_reader gen_writer/;

use Test2::Mock();

use base 'Exporter';

our $VERSION = '0.000162';

our @CARP_NOT = (__PACKAGE__, 'Test2::Mock');
our @EXPORT = qw/mock mocked/;
our @EXPORT_OK = qw{
    mock_obj mock_class
    mock_do  mock_build
    mock_accessor mock_accessors
    mock_getter   mock_getters
    mock_setter   mock_setters
    mock_building
};

my %HANDLERS;
my %MOCKS;
my @BUILD;

sub add_handler {
    my $class = shift;
    my ($for, $code) = @_;

    croak "Must specify a package for the mock handler"
        unless $for;

    croak "Handlers must be code references (got: $code)"
        unless $code && ref($code) eq 'CODE';

    push @{$HANDLERS{$for}} => $code;
}

sub mock_building {
    return unless @BUILD;
    return $BUILD[-1];
}

sub mocked {
    my $proto = shift;
    my $class = blessed($proto) || $proto;

    # Check if we have any mocks.
    my $set = $MOCKS{$class} || return;

    # Remove dead mocks (undef due to weaken)
    pop @$set while @$set && !defined($set->[-1]);

    # Remove the list if it is empty
    delete $MOCKS{$class} unless @$set;

    # Return the controls (may be empty list)
    return @$set;
}

sub _delegate {
    my ($args) = @_;

    my $do    = __PACKAGE__->can('mock_do');
    my $obj   = __PACKAGE__->can('mock_obj');
    my $class = __PACKAGE__->can('mock_class');
    my $build = __PACKAGE__->can('mock_build');

    return $obj unless @$args;

    my ($proto, $arg1) = @$args;

    return $obj if ref($proto) && !blessed($proto);

    if (blessed($proto)) {
        return $class unless $proto->isa('Test2::Mock');
        return $build if $arg1 && ref($arg1) && reftype($arg1) eq 'CODE';
    }

    return $class if $proto =~ m/(?:::|')/;
    return $class if $proto =~ m/^_*[A-Z]/;

    return $do if Test2::Mock->can($proto);

    if (my $sub = __PACKAGE__->can("mock_$proto")) {
        shift @$args;
        return $sub;
    }

    return undef;
}

sub mock {
    croak "undef is not a valid first argument to mock()"
        if @_ && !defined($_[0]);

    my $sub = _delegate(\@_);

    croak "'$_[0]' does not look like a package name, and is not a valid control method"
        unless $sub;

    $sub->(@_);
}

sub mock_build {
    my ($control, $sub) = @_;

    croak "mock_build requires a Test2::Mock object as its first argument"
        unless $control && blessed($control) && $control->isa('Test2::Mock');

    croak "mock_build requires a coderef as its second argument"
        unless $sub && ref($sub) && reftype($sub) eq 'CODE';

    push @BUILD => $control;
    my ($ok, $err) = &try($sub);
    pop @BUILD;
    die $err unless $ok;
}

sub mock_do {
    my ($meth, @args) = @_;

    croak "Not currently building a mock"
        unless @BUILD;

    my $build = $BUILD[-1];

    croak "'$meth' is not a valid action for mock_do()"
        if $meth =~ m/^_/ || !$build->can($meth);

    $build->$meth(@args);
}

sub mock_obj {
    my ($proto) = @_;

    if ($proto && ref($proto) && reftype($proto) ne 'CODE') {
        shift @_;
    }
    else {
        $proto = {};
    }

    my $class = _generate_class();
    my $control;

    if (@_ == 1 && reftype($_[0]) eq 'CODE') {
        my $orig = shift @_;
        $control = mock_class(
            $class,
            sub {
                my $c = mock_building;

                # We want to do these BEFORE anything that the sub may do.
                $c->block_load(1);
                $c->purge_on_destroy(1);
                $c->autoload(1);

                $orig->(@_);
            },
        );
    }
    else {
        $control = mock_class(
            $class,
            # Do these before anything the user specified.
            block_load       => 1,
            purge_on_destroy => 1,
            autoload         => 1,
            @_,
        );
    }

    my $new = bless($proto, $control->class);

    # We need to ensure there is a reference to the control object, and we want
    # it to go away with the object.
    $new->{'~~MOCK~CONTROL~~'} = $control;
    return $new;
}

sub _generate_class {
    my $prefix = __PACKAGE__;

    for (1 .. 100) {
        my $postfix = join '', map { chr(rand(26) + 65) } 1 .. 32;
        my $class = $prefix . '::__TEMP__::' . $postfix;
        my $file = $class;
        $file =~ s{::}{/}g;
        $file .= '.pm';
        next if $INC{$file};
        my $stash = do { no strict 'refs'; \%{"${class}\::"} };
        next if keys %$stash;
        return $class;
    }

    croak "Could not generate a unique class name after 100 attempts";
}

sub mock_class {
    my $proto = shift;
    my $class = blessed($proto) || $proto;
    my @args = @_;

    my $void   = !defined(wantarray);

    my $callback = sub {
        my ($parent) = reverse mocked($class);
        my $control;

        if (@args == 1 && ref($args[0]) && reftype($args[0]) eq 'CODE') {
            $control = Test2::Mock->new(class => $class);
            mock_build($control, @args);
        }
        else {
            $control = Test2::Mock->new(class => $class, @args);
        }

        if ($parent) {
            $control->{parent} = $parent;
            weaken($parent->{child} = $control);
        }

        $MOCKS{$class} ||= [];
        push @{$MOCKS{$class}} => $control;
        weaken($MOCKS{$class}->[-1]);

        return $control;
    };

    return $callback->() unless $void;

    my $level = 0;
    my $caller;
    while (my @call = caller($level++)) {
        next if $call[0] eq __PACKAGE__;
        $caller = \@call;
        last;
    }

    my $handled;
    for my $handler (@{$HANDLERS{$caller->[0]}}) {
        $handled++ if $handler->(
            class   => $class,
            caller  => $caller,
            builder => $callback,
            args    => \@args,
        );
    }

    croak "mock_class should not be called in a void context without a registered handler"
        unless $handled;
}

sub mock_accessors {
    return map {( $_ => gen_accessor($_) )} @_;
}

sub mock_accessor {
    my ($field) = @_;
    return gen_accessor($field);
}

sub mock_getters {
    my ($prefix, @list) = @_;
    return map {( "$prefix$_" => gen_reader($_) )} @list;
}

sub mock_getter {
    my ($field) = @_;
    return gen_reader($field);
}

sub mock_setters {
    my ($prefix, @list) = @_;
    return map {( "$prefix$_" => gen_writer($_) )} @list;
}

sub mock_setter {
    my ($field) = @_;
    return gen_writer($field);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Mock - Class/Instance mocking for Test2.

=head1 DESCRIPTION

Mocking is often an essential part of testing. This library covers some of the
most common mocking needs. This plugin is heavily influenced by L<Mock::Quick>,
but with an improved API. This plugin is also intended to play well with other
plugins in ways L<Mock::Quick> would be unable to.

=head1 SYNOPSIS

    my $mock = mock 'Some::Class' => (
        track => $BOOL, # Enable/Disable tracking on subs defined below

        add => [
            new_method => sub { ... },
        ],
        override => [
            replace_method => sub { ... },
        ],
        set => [
            replace_or_inject => sub { ... },
        ],

        track => $bool, # enable/disable tracking again to affect mocks made after this point
        ..., # Argument keys may be repeated
    );

    Some::Class->new_method();        # Calls the newly injected method
    Some::Class->replace_method();    # Calls our replacement method.

    $mock->override(...) # Override some more

    $mock = undef; # Undoes all the mocking, restoring all original methods.

    my $simple_mock = mock {} => (
        add => [
            is_active => sub { ... }
        ]
    );

    $simple_mock->is_active();        # Calls our newly mocked method.

=head1 EXPORTS

=head2 DEFAULT

=over 4

=item mock

This is a one-stop shop function that delegates to one of the other methods
depending on how it is used. If you are not comfortable with a function that
has a lot of potential behaviors, you can use one of the other functions
directly.

=item @mocks = mocked($object)

=item @mocks = mocked($class)

Check if an object or class is mocked. If it is mocked the C<$mock> object(s)
(L<Test2::Mock>) will be returned.

=item $mock = mock $class => ( ... );

=item $mock = mock $instance => ( ... )

=item $mock = mock 'class', $class => ( ... )

These forms delegate to C<mock_class()> to mock a package. The third form is to
be explicit about what type of mocking you want.

=item $obj = mock()

=item $obj = mock { ... }

=item $obj = mock 'obj', ...;

These forms delegate to C<mock_obj()> to create instances of anonymous packages
where methods are vivified into existence as needed.

=item mock $mock => sub { ... }

=item mock $method => ( ... )

These forms go together, the first form will set C<$mock> as the current mock
build, then run the sub. Within the sub you can declare mock specifications
using the second form. The first form delegates to C<mock_build()>.

The second form calls the specified method on the current build. This second
form delegates to C<mock_do()>.

=back

=head2 BY REQUEST

=head3 DEFINING MOCKS

=over 4

=item $obj = mock_obj( ... )

=item $obj = mock_obj { ... } => ( ... )

=item $obj = mock_obj sub { ... }

=item $obj = mock_obj { ... } => sub { ... }

This method lets you quickly generate a blessed object. The object will be an
instance of a randomly generated package name. Methods will vivify as
read/write accessors as needed.

Arguments can be any method available to L<Test2::Mock> followed by an
argument. If the very first argument is a hashref then it will be blessed as
your new object.

If you provide a coderef instead of key/value pairs, the coderef will be run to
build the mock. (See the L</"BUILDING MOCKS"> section).

=item $mock = mock_class $class => ( ... )

=item $mock = mock_class $instance => ( ... )

=item $mock = mock_class ... => sub { ... }

This will create a new instance of L<Test2::Mock> to control the package
specified. If you give it a blessed reference it will use the class of the
instance.

Arguments can be any method available to L<Test2::Mock> followed by an
argument. If the very first argument is a hashref then it will be blessed as
your new object.

If you provide a coderef instead of key/value pairs, the coderef will be run to
build the mock. (See the L</"BUILDING MOCKS"> section).

=back

=head3 BUILDING MOCKS

=over 4

=item mock_build $mock => sub { ... }

Set C<$mock> as the current build, then run the specified code. C<$mock> will
no longer be the current build when the sub is complete.

=item $mock = mock_building()

Get the current building C<$mock> object.

=item mock_do $method => $args

Run the specified method on the currently building object.

=back

=head3 METHOD GENERATORS

=over 4

=item $sub = mock_accessor $field

Generate a read/write accessor for the specified field. This will generate a sub like the following:

    $sub = sub {
        my $self = shift;
        ($self->{$field}) = @_ if @_;
        return $self->{$field};
    };

=item $sub = mock_getter $field

Generate a read only accessor for the specified field. This will generate a sub like the following:

    $sub = sub {
        my $self = shift;
        return $self->{$field};
    };

=item $sub = mock_setter $field

Generate a write accessor for the specified field. This will generate a sub like the following:

    $sub = sub {
        my $self = shift;
        ($self->{$field}) = @_;
    };

=item %pairs = mock_accessors(qw/name1 name2 name3/)

Generates several read/write accessors at once, returns key/value pairs where
the key is the field name, and the value is the coderef.

=item %pairs = mock_getters(qw/name1 name2 name3/)

Generates several read only accessors at once, returns key/value pairs where
the key is the field name, and the value is the coderef.

=item %pairs = mock_setters(qw/name1 name2 name3/)

Generates several write accessors at once, returns key/value pairs where the
key is the field name, and the value is the coderef.

=back

=head1 MOCK CONTROL OBJECTS

    my $mock = mock(...);

Mock objects are instances of L<Test2::Mock>. See it for their methods.

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
