# Modified from the original as a "mock" version for testing
use strict;
use warnings;
use 5.006; # warnings
package Software::License;
our $VERSION = 9999;

sub new {
  my ($class, $arg) = @_;

  # XXX changed from Carp::croak to die
  die "no copyright holder specified" unless $arg->{holder};

  bless $arg => $class;
}


sub year   { defined $_[0]->{year} ? $_[0]->{year} : (localtime)[5]+1900 }
sub holder { $_[0]->{holder} }

sub version  {
  my ($self) = @_;
  my $pkg = ref $self ? ref $self : $self;
  $pkg =~ s/.+:://;
  my (undef, @vparts) = split /_/, $pkg;

  return unless @vparts;
  return join '.', @vparts;
}


# sub meta1_name    { return undef; } # sort this out later, should be easy
sub meta_name     { return undef; }
sub meta_yml_name { $_[0]->meta_name }

sub meta2_name {
  my ($self) = @_;
  my $meta1 = $self->meta_name;

  return undef unless defined $meta1;

  return $meta1
    if $meta1 =~ /\A(?:open_source|restricted|unrestricted|unknown)\z/;

  return undef;
}

# XXX these are trivial mocks of the real thing
sub notice { 'NOTICE' }
sub license { 'LICENSE' }
sub fulltext { 'FULLTEXT' }

1;



