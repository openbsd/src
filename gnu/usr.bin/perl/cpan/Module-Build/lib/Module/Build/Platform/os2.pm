package Module::Build::Platform::os2;

use strict;
use vars qw($VERSION);
$VERSION = '0.4003';
$VERSION = eval $VERSION;
use Module::Build::Platform::Unix;

use vars qw(@ISA);
@ISA = qw(Module::Build::Platform::Unix);

sub manpage_separator { '.' }

sub have_forkpipe { 0 }

# Copied from ExtUtils::MM_OS2::maybe_command
sub _maybe_command {
    my($self,$file) = @_;
    $file =~ s,[/\\]+,/,g;
    return $file if -x $file && ! -d _;
    return "$file.exe" if -x "$file.exe" && ! -d _;
    return "$file.cmd" if -x "$file.cmd" && ! -d _;
    return;
}

1;
__END__


=head1 NAME

Module::Build::Platform::os2 - Builder class for OS/2 platform

=head1 DESCRIPTION

This module provides some routines very specific to the OS/2
platform.

Please see the L<Module::Build> for the general docs.

=head1 AUTHOR

Ken Williams <kwilliams@cpan.org>

=head1 SEE ALSO

perl(1), Module::Build(3), ExtUtils::MakeMaker(3)

=cut
