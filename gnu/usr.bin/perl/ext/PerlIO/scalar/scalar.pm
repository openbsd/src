package PerlIO::scalar;
our $VERSION = '0.01';
use XSLoader ();
XSLoader::load 'PerlIO::scalar';
1;
__END__

=head1 NAME

PerlIO::scalar - support module for in-memory IO.

=head1 SYNOPSIS

   open($fh,"<",\$scalar);
   open($fh,">",\$scalar);

or

   open($fh,"<:scalar",\$scalar);
   open($fh,">:scalar",\$scalar);

=head1 DESCRIPTION

C<PerlIO::scalar> only exists to use XSLoader to load C code that provides
support for treating a scalar as an "in memory" file.

All normal file operations can be performed on the handle. The scalar
is considered a stream of bytes. Currently fileno($fh) returns C<undef>.

=cut


