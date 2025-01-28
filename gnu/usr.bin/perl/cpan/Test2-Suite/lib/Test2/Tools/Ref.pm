package Test2::Tools::Ref;
use strict;
use warnings;

our $VERSION = '0.000162';

use Scalar::Util qw/reftype refaddr/;
use Test2::API qw/context/;
use Test2::Util::Ref qw/render_ref/;

our @EXPORT = qw/ref_ok ref_is ref_is_not/;
use base 'Exporter';

sub ref_ok($;$$) {
    my ($thing, $wanttype, $name) = @_;
    my $ctx = context();

    my $gotname = render_ref($thing);
    my $gottype = reftype($thing);

    if (!$gottype) {
        $ctx->ok(0, $name, ["'$gotname' is not a reference"]);
        $ctx->release;
        return 0;
    }

    if ($wanttype && $gottype ne $wanttype) {
        $ctx->ok(0, $name, ["'$gotname' is not a '$wanttype' reference"]);
        $ctx->release;
        return 0;
    }

    $ctx->ok(1, $name);
    $ctx->release;
    return 1;
}

sub ref_is($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    $got = '<undef>' unless defined $got;
    $exp = '<undef>' unless defined $exp;

    my $bool = 0;
    if (!ref($got)) {
        $ctx->ok(0, $name, ["First argument '$got' is not a reference", @diag]);
    }
    elsif(!ref($exp)) {
        $ctx->ok(0, $name, ["Second argument '$exp' is not a reference", @diag]);
    }
    else {
        # Don't let overloading mess with us.
        $bool = refaddr($got) == refaddr($exp);
        $ctx->ok($bool, $name, ["'$got' is not the same reference as '$exp'", @diag]);
    }

    $ctx->release;
    return $bool ? 1 : 0;
}

sub ref_is_not($$;$) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    $got = '<undef>' unless defined $got;
    $exp = '<undef>' unless defined $exp;

    my $bool = 0;
    if (!ref($got)) {
        $ctx->ok(0, $name, ["First argument '$got' is not a reference", @diag]);
    }
    elsif(!ref($exp)) {
        $ctx->ok(0, $name, ["Second argument '$exp' is not a reference", @diag]);
    }
    else {
        # Don't let overloading mess with us.
        $bool = refaddr($got) != refaddr($exp);
        $ctx->ok($bool, $name, ["'$got' is the same reference as '$exp'", @diag]);
    }

    $ctx->release;
    return $bool ? 1 : 0;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Ref - Tools for validating references.

=head1 DESCRIPTION

This module contains tools that allow you to verify that something is a ref. It
also has tools to check if two refs are the same exact ref, or different. None of
the functions in this module do deep comparisons.

=head1 SYNOPSIS

    use Test2::Tools::Ref;

    # Ensure something is a ref.
    ref_ok($ref);

    # Check that $ref is a HASH reference
    ref_ok($ref, 'HASH', 'Must be a hash')

    ref_is($refa, $refb, "Same exact reference");

    ref_is_not($refa, $refb, "Not the same exact reference");

=head1 EXPORTS

All subs are exported by default.

=over 4

=item ref_ok($thing)

=item ref_ok($thing, $type)

=item ref_ok($thing, $type, $name)

This checks that C<$thing> is a reference. If C<$type> is specified then it
will check that C<$thing> is that type of reference.

=item ref_is($ref1, $ref2, $name)

Verify that two references are the exact same reference.

=item ref_is_not($ref1, $ref2, $name)

Verify that two references are not the exact same reference.

=back

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
