package Test2::Compare;
use strict;
use warnings;

our $VERSION = '0.000162';

use Scalar::Util qw/blessed/;
use Test2::Util qw/try/;
use Test2::Util::Ref qw/rtype/;

use Carp qw/croak/;

our @EXPORT_OK = qw{
    compare
    get_build push_build pop_build build
    strict_convert relaxed_convert convert
};
use base 'Exporter';

sub compare {
    my ($got, $check, $convert) = @_;

    $check = $convert->($check);

    return $check->run(
        id      => undef,
        got     => $got,
        exists  => 1,
        convert => $convert,
        seen    => {},
    );
}

my @BUILD;

sub get_build  { @BUILD ? $BUILD[-1] : undef }
sub push_build { push @BUILD => $_[0] }

sub pop_build {
    return pop @BUILD if @BUILD && $_[0] && $BUILD[-1] == $_[0];
    my $have = @BUILD ? "$BUILD[-1]" : 'undef';
    my $want = $_[0]  ? "$_[0]"      : 'undef';
    croak "INTERNAL ERROR: Attempted to pop incorrect build, have $have, tried to pop $want";
}

sub build {
    my ($class, $code) = @_;

    my @caller = caller(1);

    die "'$caller[3]\()' should not be called in void context in $caller[1] line $caller[2]\n"
        unless defined(wantarray);

    my $build = $class->new(builder => $code, called => \@caller);

    push @BUILD => $build;
    my ($ok, $err) = try { $code->($build); 1 };
    pop @BUILD;
    die $err unless $ok;

    return $build;
}

sub strict_convert  { convert($_[0], { implicit_end => 1, use_regex => 0, use_code => 0 }) }
sub relaxed_convert { convert($_[0], { implicit_end => 0, use_regex => 1, use_code => 1 }) }

my $CONVERT_LOADED = 0;
my %ALLOWED_KEYS = ( implicit_end => 1, use_regex => 1, use_code => 1 );
sub convert {
    my ($thing, $config) = @_;

    unless($CONVERT_LOADED) {
        require Test2::Compare::Array;
        require Test2::Compare::Base;
        require Test2::Compare::Custom;
        require Test2::Compare::DeepRef;
        require Test2::Compare::Hash;
        require Test2::Compare::Pattern;
        require Test2::Compare::Ref;
        require Test2::Compare::Regex;
        require Test2::Compare::Scalar;
        require Test2::Compare::String;
        require Test2::Compare::Undef;
        require Test2::Compare::Wildcard;
        $CONVERT_LOADED = 1;
    }

    if (ref($config)) {
        my $bad = join ', ' => grep { !$ALLOWED_KEYS{$_} } keys %$config;
        croak "The following config options are not understood by convert(): $bad" if $bad;
        $config->{implicit_end} = 1 unless defined $config->{implicit_end};
        $config->{use_regex}    = 1 unless defined $config->{use_regex};
        $config->{use_code}     = 0 unless defined $config->{use_code};
    }
    else { # Legacy...
        if ($config) {
            $config = {
                implicit_end => 1,
                use_regex  => 0,
                use_code   => 0,
            };
        }
        else {
            $config = {
                implicit_end => 0,
                use_regex  => 1,
                use_code   => 1,
            };
        }
    }

    return _convert($thing, $config);
}

sub _convert {
    my ($thing, $config) = @_;

    return Test2::Compare::Undef->new()
        unless defined $thing;

    if (blessed($thing) && $thing->isa('Test2::Compare::Base')) {
        if ($config->{implicit_end} && $thing->can('set_ending') && !defined $thing->ending) {
            my $clone = $thing->clone;
            $clone->set_ending('implicit');
            return $clone;
        }

        return $thing unless $thing->isa('Test2::Compare::Wildcard');
        my $newthing = _convert($thing->expect, $config);
        $newthing->set_builder($thing->builder) unless $newthing->builder;
        $newthing->set_file($thing->_file)      unless $newthing->_file;
        $newthing->set_lines($thing->_lines)    unless $newthing->_lines;
        return $newthing;
    }

    my $type = rtype($thing);

    return Test2::Compare::Array->new(inref => $thing, $config->{implicit_end} ? (ending => 1) : ())
        if $type eq 'ARRAY';

    return Test2::Compare::Hash->new(inref => $thing, $config->{implicit_end} ? (ending => 1) : ())
        if $type eq 'HASH';

    return Test2::Compare::Pattern->new(
        pattern       => $thing,
        stringify_got => 1,
    ) if $config->{use_regex} && $type eq 'REGEXP';

    return Test2::Compare::Custom->new(code => $thing)
        if $config->{use_code} && $type eq 'CODE';

    return Test2::Compare::Regex->new(input => $thing)
        if $type eq 'REGEXP';

    if ($type eq 'SCALAR' || $type eq 'VSTRING') {
        my $nested = _convert($$thing, $config);
        return Test2::Compare::Scalar->new(item => $nested);
    }

    return Test2::Compare::DeepRef->new(input => $thing)
        if $type eq 'REF';

    return Test2::Compare::Ref->new(input => $thing)
        if $type;

    # is() will assume string and use 'eq'
    return Test2::Compare::String->new(input => $thing);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare - Test2 extension for writing deep comparison tools.

=head1 DESCRIPTION

This library is the driving force behind deep comparison tools such as
C<Test2::Tools::Compare::is()> and
C<Test2::Tools::ClassicCompare::is_deeply()>.

=head1 SYNOPSIS

    package Test2::Tools::MyCheck;

    use Test2::Compare::MyCheck;
    use Test2::Compare qw/compare/;

    sub MyCheck {
        my ($got, $exp, $name, @diag) = @_;
        my $ctx = context();

        my $delta = compare($got, $exp, \&convert);

        if ($delta) {
            $ctx->fail($name, $delta->diag, @diag);
        }
        else {
            $ctx->ok(1, $name);
        }

        $ctx->release;
        return !$delta;
    }

    sub convert {
        my $thing = shift;
        return $thing if blessed($thing) && $thing->isa('Test2::Compare::MyCheck');

        return Test2::Compare::MyCheck->new(stuff => $thing);
    }

See L<Test2::Compare::Base> for details about writing a custom check.

=head1 EXPORTS

=over 4

=item $delta = compare($got, $expect, \&convert)

This will compare the structures in C<$got> with those in C<$expect>, The
convert sub should convert vanilla structures inside C<$expect> into checks.
If there are differences in the structures they will be reported back as an
L<Test2::Compare::Delta> tree.

=item $build = get_build()

Get the current global build, if any.

=item push_build($build)

Set the current global build.

=item $build = pop_build($build)

Unset the current global build. This will throw an exception if the build
passed in is different from the current global.

=item build($class, sub { ... })

Run the provided codeblock with a new instance of C<$class> as the current
build. Returns the new build.

=item $check = convert($thing)

=item $check = convert($thing, $config)

This convert function is used by C<strict_convert()> and C<relaxed_convert()>
under the hood. It can also be used as the basis for other convert functions.

If you want to use it with a custom configuration you should wrap it in another
sub like so:

    sub my_convert {
        my $thing_to_convert = shift;
        return convert(
            $thing_to_convert,
            { ... }
        );
    }

Or the short variant:

    sub my_convert { convert($_[0], { ... }) }

There are several configuration options, here they are with the default setting
listed first:

=over 4

=item implicit_end => 1

This option toggles array/hash boundaries. If this is true then no extra hash
keys or array indexes will be allowed. This setting effects generated compare
objects as well as any passed in.

=item use_regex => 1

This option toggles regex matching. When true (default) regexes are converted
to checks such that values must match the regex. When false regexes will be
compared to see if they are identical regexes.

=item use_code => 0

This option toggles code matching. When false (default) coderefs in structures
must be the same coderef as specified. When true coderefs will be run to verify
the value being checked.

=back

=item $check = strict_convert($thing)

Convert C<$thing> to an L<Test2::Compare::*> object. This will behave strictly
which means it uses these settings:

=over 4

=item implicit_end => 1

Array bounds will be checked when this object is used in a comparison. No
unexpected hash keys can be present.

=item use_code => 0

Sub references will be compared as refs (IE are these sub refs the same ref?)

=item use_regex => 0

Regexes will be compared directly (IE are the regexes the same?)

=back

=item $compare = relaxed_convert($thing)

Convert C<$thing> to an L<Test2::Compare::*> object. This will be relaxed which
means it uses these settings:

=over 4

=item implicit_end => 0

Array bounds will not be checked when this object is used in a comparison.
Unexpected hash keys can be present.

=item use_code => 1

Sub references will be run to verify a value.

=item use_regex => 1

Values will be checked against any regexes provided.

=back

=back

=head1 WRITING A VARIANT OF IS/LIKE

    use Test2::Compare qw/compare convert/;

    sub my_like($$;$@) {
        my ($got, $exp, $name, @diag) = @_;
        my $ctx = context();

        # A custom converter that does the same thing as the one used by like()
        my $convert = sub {
            my $thing = shift;
            return convert(
                $thing,
                {
                    implicit_end => 0,
                    use_code     => 1,
                    use_regex    => 1,
                }
            );
        };

        my $delta = compare($got, $exp, $convert);

        if ($delta) {
            $ctx->fail($name, $delta->diag, @diag);
        }
        else {
            $ctx->ok(1, $name);
        }

        $ctx->release;
        return !$delta;
    }

The work of a comparison tool is done by 3 entities:

=over 4

=item compare()

The C<compare()> function takes the structure you got, the specification you
want to check against, and a C<\&convert> sub that will convert anything that
is not an instance of an L<Test2::Compare::Base> subclass into one.

This tool will use the C<\&convert> function on the specification, and then
produce an L<Test2::Compare::Delta> structure that outlines all the ways the
structure you got deviates from the specification.

=item \&convert

Converts anything that is not an instance of an L<Test2::Compare::Base>
subclass, and turns it into one. The objects this produces are able to check
that a structure matches a specification.

=item $delta

An instance of L<Test2::Compare::Delta> is ultimately returned. This object
represents all the ways in with the structure you got deviated from the
specification. The delta is a tree and may contain child deltas for nested
structures.

The delta is capable of rendering itself as a table, use C<< @lines =
$delta->diag >> to get the table (lines in C<@lines> will not be terminated
with C<"\n">).

=back

The C<convert()> function provided by this package contains all the
specification behavior of C<like()> and C<is()>. It is intended to be wrapped
in a sub that passes in a configuration hash, which allows you to control the
behavior.

You are free to write your own C<$check = compare($thing)> function, it just
needs to accept a single argument, and produce a single instance of an
L<Test2::Compare::Base> subclass.

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

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

See F<http://dev.perl.org/licenses/>

=cut
