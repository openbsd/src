package Test2::Require::Module;
use strict;
use warnings;

use base 'Test2::Require';

our $VERSION = '0.000162';

use Test2::Util qw/pkg_to_file/;

sub skip {
    my $class = shift;
    my ($module, $ver) = @_;

    return "Module '$module' is not installed"
        unless check_installed($module);

    return undef unless defined $ver;

    return check_version($module, $ver);
}

sub check_installed {
    my ($mod) = @_;
    my $file = pkg_to_file($mod);

    return 1 if eval { require $file; 1 };
    my $error = $@;

    return 0 if $error =~ m/Can't locate \Q$file\E in \@INC/;

    # Some other error, rethrow it.
    die $error;
}

sub check_version {
    my ($mod, $ver) = @_;

    return undef if eval { $mod->VERSION($ver); 1 };
    my $have = $mod->VERSION;
    return "Need '$mod' version $ver, have $have.";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require::Module - Skip tests if certain packages are not installed, or
insufficient versions.

=head1 DESCRIPTION

Sometimes you have tests that are nice to run, but depend on tools that may not
be available. Instead of adding the tool as a dep, or making the test always
skip, it is common to make the test run conditionally. This package helps make
that possible.

This module is modeled after L<Test::Requires>. The difference is that this
module is based on L<Test2> directly, and does not go through L<Test::Builder>.
Another difference is that the packages you check for are not imported into
your namespace for you. This is intentional.

=head1 SYNOPSIS

    # The test will be skipped unless Some::Module is installed, any version.
    use Test2::Require::Module 'Some::Module';

    # The test will be skipped unless 'Other::Module' is installed and at
    # version '5.555' or greater.
    use Test2::Require::Module 'Other::Module' => '5.555';

    # We now need to use them directly, Test2::Require::Module does not import
    # them for us.
    use Some::Module;
    use Other::Module;

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
