package if;

our $VERSION = '0.03';

sub work {
  my $method = shift() ? 'import' : 'unimport';
  return unless shift;		# CONDITION

  my $p = $_[0];		# PACKAGE
  (my $file = "$p.pm") =~ s!::!/!g;
  require $file or die;

  my $m = $p->can($method);
  goto &$m if $m;
}

sub import   { shift; unshift @_, 1; goto &work }
sub unimport { shift; unshift @_, 0; goto &work }

1;
__END__

=head1 NAME

if - C<use> a Perl module if a condition holds

=head1 SYNOPSIS

  use if CONDITION, MODULE => ARGUMENTS;

=head1 DESCRIPTION

The construct

  use if CONDITION, MODULE => ARGUMENTS;

has no effect unless C<CONDITION> is true.  In this case the effect is
the same as of

  use MODULE ARGUMENTS;

=head1 BUGS

The current implementation does not allow specification of the
required version of the module.

=head1 AUTHOR

Ilya Zakharevich L<mailto:perl-module-if@ilyaz.org>.

=cut

