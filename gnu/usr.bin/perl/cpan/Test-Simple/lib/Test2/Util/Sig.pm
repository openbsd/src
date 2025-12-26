package Test2::Util::Sig;
use strict;
use warnings;

our $VERSION = '1.302210';

use POSIX();
use Test2::Util qw/try IS_WIN32/;

our @EXPORT_OK = qw{
    try_sig_mask
};
BEGIN { require Exporter; our @ISA = qw(Exporter) }

sub try_sig_mask(&) {
    my $code = shift;

    my ($old, $blocked);
    unless(IS_WIN32) {
        my $to_block = POSIX::SigSet->new(
            POSIX::SIGINT(),
            POSIX::SIGALRM(),
            POSIX::SIGHUP(),
            POSIX::SIGTERM(),
            POSIX::SIGUSR1(),
            POSIX::SIGUSR2(),
        );
        $old = POSIX::SigSet->new;
        $blocked = POSIX::sigprocmask(POSIX::SIG_BLOCK(), $to_block, $old);
        # Silently go on if we failed to log signals, not much we can do.
    }

    my ($ok, $err) = &try($code);

    # If our block was successful we want to restore the old mask.
    POSIX::sigprocmask(POSIX::SIG_SETMASK(), $old, POSIX::SigSet->new()) if defined $blocked;

    return ($ok, $err);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Sig - Signal tools used by Test2 and friends.

=head1 DESCRIPTION

Collection of signal tools used by L<Test2> and friends.

=head1 EXPORTS

All exports are optional. You must specify subs to import.

=over 4

=item ($ok, $err) = try_sig_mask { ... }

Complete an action with several signals masked, they will be unmasked at the
end allowing any signals that were intercepted to get handled.

This is primarily used when you need to make several actions atomic (against
some signals anyway).

Signals that are intercepted:

=over 4

=item SIGINT

=item SIGALRM

=item SIGHUP

=item SIGTERM

=item SIGUSR1

=item SIGUSR2

=back

=back

=head1 SOURCE

The source code repository for Test2 can be found at
L<https://github.com/Test-More/test-more/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=item Kent Fredric E<lt>kentnl@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<https://dev.perl.org/licenses/>

=cut
