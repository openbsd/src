package File::Spec::Epoc;

our $VERSION = '1.00';

use strict;
use Cwd;
use vars qw(@ISA);
require File::Spec::Unix;
@ISA = qw(File::Spec::Unix);

=head1 NAME

File::Spec::Epoc - methods for Epoc file specs

=head1 SYNOPSIS

 require File::Spec::Epoc; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

This package is still work in progress ;-)
o.flebbe@gmx.de


=over 4

sub case_tolerant {
    return 1;
}

=item canonpath()

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=cut

sub canonpath {
    my ($self,$path) = @_;

    $path =~ s|/+|/|g;                             # xx////xx  -> xx/xx
    $path =~ s|(/\.)+/|/|g;                        # xx/././xx -> xx/xx
    $path =~ s|^(\./)+||s unless $path eq "./";    # ./xx      -> xx
    $path =~ s|^/(\.\./)+|/|s;                     # /../../xx -> xx
    $path =~  s|/\Z(?!\n)|| unless $path eq "/";          # xx/       -> xx
    return $path;
}

=back

=head1 SEE ALSO

L<File::Spec>

=cut

1;
