package ExtUtils::MM_BeOS;

=head1 NAME

ExtUtils::MM_BeOS - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_BeOS;	# Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over 4

=cut 

use Config;
use File::Spec;
require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;

use vars qw(@ISA $VERSION);
@ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );
$VERSION = 1.03;


=item perl_archive

This is internal method that returns path to libperl.a equivalent
to be linked to dynamic extensions. UNIX does not have one, but at
least BeOS has one.

=cut

sub perl_archive
  {
  return File::Spec->catdir('$(PERL_INC)',$Config{libperl});
  }

=back

1;
__END__

