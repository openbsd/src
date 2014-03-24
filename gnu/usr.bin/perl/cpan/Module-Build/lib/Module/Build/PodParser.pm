package Module::Build::PodParser;

use strict;
use vars qw($VERSION);
$VERSION = '0.4003';
$VERSION = eval $VERSION;
use vars qw(@ISA);

sub new {
  # Perl is so fun.
  my $package = shift;

  my $self;
  @ISA = ();
  $self = bless {have_pod_parser => 0, @_}, $package;

  unless ($self->{fh}) {
    die "No 'file' or 'fh' parameter given" unless $self->{file};
    $self->{fh} = IO::File->new($self->{file}) or die "Couldn't open $self->{file}: $!";
  }

  return $self;
}

sub parse_from_filehandle {
  my ($self, $fh) = @_;

  local $_;
  while (<$fh>) {
    next unless /^=(?!cut)/ .. /^=cut/;  # in POD
    # Accept Name - abstract or C<Name> - abstract
    last if ($self->{abstract}) = /^ (?: [a-z0-9:]+ | [BCIF] < [a-z0-9:]+ > ) \s+ - \s+ (.*\S) /ix;
  }

  my @author;
  while (<$fh>) {
    next unless /^=head1\s+AUTHORS?/i ... /^=/;
    next if /^=/;
    push @author, $_ if /\@/;
  }
  return unless @author;
  s/^\s+|\s+$//g foreach @author;

  $self->{author} = \@author;

  return;
}

sub get_abstract {
  my $self = shift;
  return $self->{abstract} if defined $self->{abstract};

  $self->parse_from_filehandle($self->{fh});

  return $self->{abstract};
}

sub get_author {
  my $self = shift;
  return $self->{author} if defined $self->{author};

  $self->parse_from_filehandle($self->{fh});

  return $self->{author} || [];
}
