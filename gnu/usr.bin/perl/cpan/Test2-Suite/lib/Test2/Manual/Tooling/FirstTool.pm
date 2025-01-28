package Test2::Manual::Tooling::FirstTool;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::FirstTool - Write your first tool with Test2.

=head1 DESCRIPTION

This tutorial will help you write your very first tool by cloning the C<ok()>
tool.

=head1 COMPLETE CODE UP FRONT

    package Test2::Tools::MyOk;
    use strict;
    use warnings;

    use Test2::API qw/context/;

    use base 'Exporter';
    our @EXPORT = qw/ok/;

    sub ok($;$@) {
        my ($bool, $name, @diag) = @_;

        my $ctx = context();

        return $ctx->pass_and_release($name) if $bool;
        return $ctx->fail_and_release($name, @diag);
    }

    1;

=head1 LINE BY LINE

=over 4

=item sub ok($;$@) {

In this case we are emulating the C<ok()> function exported by
L<Test2::Tools::Basic>.

C<ok()> and similar test tools use prototypes to enforce argument parsing. Your
test tools do not necessarily need prototypes, like any perl function you need
to make the decision based on how it is used.

The prototype requires at least 1 argument, which will
be forced into a scalar context. The second argument is optional, and is also
forced to be scalar, it is the name of the test. Any remaining arguments are
treated as diagnostics messages that will only be used if the test failed.

=item my ($bool, $name, @diag) = @_;

This line does not need much explanation, we are simply grabbing the args.

=item my $ctx = context();

This is a vital line in B<ALL> tools. The context object is the primary API for
test tools. You B<MUST> get a context if you want to issue any events, such as
making assertions. Further, the context is responsible for making sure failures
are attributed to the correct file and line number.

B<Note:> A test function B<MUST> always release the context when it is done,
you cannot simply let it fall out of scope and be garbage collected. Test2 does
a pretty good job of yelling at you if you make this mistake.

B<Note:> You B<MUST NOT> ever store or pass around a I<real> context object. If
you wish to hold on to a context for any reason you must use clone to make a
copy C<< my $copy = $ctx->clone >>. The copy may be passed around or stored,
but the original B<MUST> be released when you are done with it.

=item return $ctx->pass_and_release($name) if $bool;

When C<$bool> is true, this line uses the context object to issue a
L<Test2::Event::Pass> event. Along with issuing the event this will also
release the context object and return true.

This is short form for:

    if($bool) {
        $ctx->pass($name);
        $ctx->release;
        return 1;
    }

=item return $ctx->fail_and_release($name, @diag);

This line issues a L<Test2::Event::Fail> event, releases the context object,
and returns false. The fail event will include any diagnostics messages from
the C<@diag> array.

This is short form for:

    $ctx->fail($name, @diag);
    $ctx->release;
    return 0;

=back

=head1 CONTEXT OBJECT DOCUMENTATION

L<Test2::API::Context> is the place to read up on what methods the context
provides.

=head1 SEE ALSO

L<Test2::Manual> - Primary index of the manual.

=head1 SOURCE

The source code repository for Test2-Manual can be found at
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
