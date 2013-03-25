package CPANPLUS::Dist::Build::Constants;

use strict;
use warnings;
use File::Spec;

BEGIN {

    require Exporter;
    use vars    qw[$VERSION @ISA @EXPORT];

    $VERSION    = '0.62';
    @ISA        = qw[Exporter];
    @EXPORT     = qw[ BUILD_DIR BUILD CPDB_PERL_WRAPPER];
}


use constant BUILD_DIR      => sub { return @_
                                        ? File::Spec->catdir($_[0], '_build')
                                        : '_build';
                            };
use constant BUILD          => sub { my $file = @_
                                        ? File::Spec->catfile($_[0], 'Build')
                                        : 'Build';

                                     ### on VMS, '.com' is appended when
                                     ### creating the Build file
                                     $file .= '.com' if $^O eq 'VMS';

                                     return $file;
                            };


use constant CPDB_PERL_WRAPPER   => 'use strict; BEGIN { my $old = select STDERR; $|++; select $old; $|++; $0 = shift(@ARGV); my $rv = do($0); die $@ if $@; }';

1;

=head1 NAME

CPANPLUS::Dist::Build::Constants - Constants for CPANPLUS::Dist::Build

=head1 SYNOPSIS

  use CPANPLUS::Dist::Build::Constants;

=head1 DESCRIPTION

CPANPLUS::Dist::Build::Constants provides some constants required by L<CPANPLUS::Dist::Build>.

=head1 AUTHOR

Originally by Jos Boumans E<lt>kane@cpan.orgE<gt>.  Brought to working
condition and currently maintained by Ken Williams E<lt>kwilliams@cpan.orgE<gt>.

=head1 LICENSE

The CPAN++ interface (of which this module is a part of) is
copyright (c) 2001, 2002, 2003, 2004, 2005 Jos Boumans E<lt>kane@cpan.orgE<gt>.
All rights reserved.

This library is free software;
you may redistribute and/or modify it under the same
terms as Perl itself.

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
