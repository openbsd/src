package Test2::Tools;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools - Documentation for Tools.

=head1 DESCRIPTION

Tools are packages that export test functions, typically all related to a
specific aspect of testing. If you have a couple different categories of
exports then you may want to break them into separate modules.

Tools should export testing functions. Loading tools B<should not> have side
effects, or alter the behavior of other tools. If you want to alter behaviors
or create side effects then you probably want to write a L<Test2::Plugin>.

=head1 FAQ

=over 4

=item Why is it called Test2::Tools, and not Test2::Tool?

This question arises since Tools is the only namespace in the plural. This is
because each Plugin should be a distinct unit of functionality, but a Tools
dist can (and usually should) export several tools. A bundle is also typically
described as a single unit. Nobody would like Test2::Bundles::Foo.

=item Should my tools subclass Test2::Tools?

No. Currently this class is empty. Eventually we may want to add behavior, in
which case we do not want anyone to already be subclassing it.

=back

=head1 HOW DO I WRITE A 'TOOLS' MODULE?

It is very easy to write tools:

    package Test2::Tools::Mine
    use strict;
    use warnings;

    # All tools should use the context() function.
    use Test2::API qw/context/;

    our @EXPORTS = qw/ok plan/;
    use base 'Exporter';

    sub ok($;$) {
        my ($bool, $name) = @_;

        # All tool functions should start by grabbing a context
        my $ctx = context();

        # The context is the primary interface for generating events
        $ctx->ok($bool, $name);

        # When you are done you release the context
        $ctx->release;

        return $bool ? 1 : 0;
    }

    sub plan {
        my ($max) = @_;
        my $ctx = context();
        $ctx->plan($max);
        $ctx->release;
    }

    1;

See L<Test2::API::Context> for documentation on what the C<$ctx> object can do.

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
