package OpenBSD::MkTemp;

use 5.012002;
use strict;
use warnings;

use Exporter 'import';

our @EXPORT_OK = qw( mkstemps mkstemp mkdtemp );
our @EXPORT = qw( mkstemp mkdtemp );
our $VERSION = '0.02';

require XSLoader;
XSLoader::load('OpenBSD::MkTemp', $VERSION);

sub mkstemp($)
{
	my $template = shift;
	my $fh = mkstemps_real($template, 0) || return;
	return wantarray() ? ($fh, $template) : $fh;
}

sub mkstemps($$)
{
	my($template, $suffix) = @_;
	$template .= $suffix;
	my $fh = mkstemps_real($template, length($suffix)) || return;
	return wantarray() ? ($fh, $template) : $fh;
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

This module provides routines for creating files and directories with
guaranteed unique names, using the C mkstemps() and mkdtemp() routines.

mkstemp() and mkstemps() must be called with a template argument
that is writable, so that they can update it with the path of the
generated file.
They return normal perl IO handles.

mkdtemp() simply takes the template and returns the path of the
newly created directory.

=head2 EXPORT

  $fh = mkstemp($template)

=head2 Exportable functions

  $fh = mkstemps($template, $suffix_len)
  $dir = mkdtemp($template);

=head1 SEE ALSO

mkstemp(3)

=head1 AUTHOR

Philip Guenther, E<lt>guenther@openbsd.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2010 by Philip Guenther

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.12.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
