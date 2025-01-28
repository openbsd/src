package Test2::Require;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::API qw/context/;
use Carp qw/croak/;

sub skip {
    my $class = shift;
    croak "Class '$class' needs to implement 'skip()'";
}

sub import {
    my $class = shift;
    return if $class eq __PACKAGE__;

    my $skip = $class->skip(@_);
    return unless defined $skip;

    my $ctx = context();
    $ctx->plan(0, SKIP => $skip || "No reason given.");
    $ctx->release;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require - Base class and documentation for skip-unless type test
packages.

=head1 DESCRIPTION

Test2::Require::* packages are packages you load to ensure your test file is
skipped unless a specific requirement is met. Modules in this namespace may
subclass L<Test2::Require> if they wish, but it is not strictly necessary to do
so.

=head1 HOW DO I WRITE A 'REQUIRE' MODULE?

=head2 AS A SUBCLASS

    package Test2::Require::Widget;
    use strict;
    use warnings;

    use base 'Test2::Require';

    sub HAVE_WIDGETS { ... };

    sub skip {
        my $class = shift;
        my @import_args = @_;

        if (HAVE_WIDGETS()) {
            # We have widgets, do not skip
            return undef;
        }
        else {
            # No widgets, skip the test
            return "Skipped because there are no widgets" unless HAVE_WIDGETS();
        }
    }

    1;

A subclass of L<Test2::Require> simply needs to implement a C<skip()> method.
This method will receive all import arguments. This method should return undef
if the test should run, and should return a reason for skipping if the test
should be skipped.

=head2 STAND-ALONE

If you do not wish to subclass L<Test2::Require> then you should write an
C<import()> method:

    package Test2::Require::Widget;
    use strict;
    use warnings;

    use Test2::API qw/context/;

    sub HAVE_WIDGETS { ... };

    sub import {
        my $class = shift;

        # Have widgets, should run.
        return if HAVE_WIDGETS();

        # Use the context object to create the event
        my $ctx = context();
        $ctx->plan(0, SKIP => "Skipped because there are no widgets");
        $ctx->release;
    }

    1;

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
