package File::Spec::Cygwin;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '1.0';

@ISA = qw(File::Spec::Unix);

sub canonpath {
    my($self,$path) = @_;
    $path =~ s|\\|/|g;
    return $self->SUPER::canonpath($path);
}

sub file_name_is_absolute {
    my ($self,$file) = @_;
    return 1 if $file =~ m{^([a-z]:)?[\\/]}is; # C:/test
    return $self->SUPER::file_name_is_absolute($file);
}

1;
__END__

=head1 NAME

File::Spec::Cygwin - methods for Cygwin file specs

=head1 SYNOPSIS

 require File::Spec::Cygwin; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

This module is still in beta.  Cygwin-knowledgeable folks are invited
to offer patches and suggestions.
