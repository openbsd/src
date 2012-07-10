package OpenBSD::MkTemp;

use 5.012002;
use strict;
use warnings;

use Exporter 'import';

our @EXPORT_OK = qw( mkstemps mkstemp mkdtemp );
our @EXPORT = qw( mkstemp mkdtemp );
our $VERSION = '0.03';

require XSLoader;
XSLoader::load('OpenBSD::MkTemp', $VERSION);

sub mkstemp($)
{
	return mkstemps($_[0]);
}


1;
__END__

=head1 NAME

OpenBSD::MkTemp - Perl access to mkstemps() and mkdtemp()

=head1 SYNOPSIS

  use OpenBSD::MkTemp;

  my($fh, $file) = mkstemp("/tmp/fooXXXXXXXXXX");

  use OpenBSD::MkTemp qw(mkdtemp mkstemps);

  my $dir_name = mkdtemp("/tmp/dirXXXXXXXXXX");
  my ($fh, $file) = mkstemps("/tmp/fileXXXXXXXXXX", ".tmp");


=head1 DESCRIPTION

This module provides routines for creating files and directories
with guaranteed unique names, using the C C<mkstemps()> and
C<mkdtemp()> routines.
On the perl-side, they are intended to behave the same as the
functions provided by L<File::Temp>.

For all these functions, the template provided follows the rules
of the system's C<mkstemps()> and C<mkdtemp()> functions.
The template may be any file name with some number of Xs appended
to it, for example C</tmp/temp.XXXXXXXX>.
The trailing Xs are replaced with a unique digit and letter combination.

C<mkstemp()> takes a template and creates a new, unique file.
In a list context, it returns a two items: a normal perl IO handle
open to the new file for both read and write, and the generated
filename.
In a scalar context it just returns the IO handle.

C<mkstemps()> takes the template and a suffix to append to the
filename.  For example, the call C<mkstemps("/tmp/temp.XXXXXXXXXX",
".c")> might create the file C</tmp/temp.SO4csi32GM.c>.
It returns the filename and/or filename just like C<mkstemp()>

C<mkdtemp()> simply takes the template and returns the path of the
newly created directory.

Note that the files and directories created by these functions are
I<not> automatically removed.

On failure, all of these functions call die.

=head2 EXPORT

  ($fh, $filename) = mkstemp($template)

=head2 Exportable functions

  ($fh, $filename) = mkstemps($template, $suffix)
  $dir = mkdtemp($template);

=head1 SEE ALSO

mkstemp(3)

=head1 AUTHOR

Philip Guenther, E<lt>guenther@openbsd.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2010,2012 by Philip Guenther

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.12.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
