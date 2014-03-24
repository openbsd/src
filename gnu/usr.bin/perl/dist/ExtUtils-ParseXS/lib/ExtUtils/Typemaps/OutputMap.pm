package ExtUtils::Typemaps::OutputMap;
use 5.006001;
use strict;
use warnings;
our $VERSION = '3.18';

=head1 NAME

ExtUtils::Typemaps::OutputMap - Entry in the OUTPUT section of a typemap

=head1 SYNOPSIS

  use ExtUtils::Typemaps;
  ...
  my $output = $typemap->get_output_map('T_NV');
  my $code = $output->code();
  $output->code("...");

=head1 DESCRIPTION

Refer to L<ExtUtils::Typemaps> for details.

=head1 METHODS

=cut

=head2 new

Requires C<xstype> and C<code> parameters.

=cut

sub new {
  my $prot = shift;
  my $class = ref($prot)||$prot;
  my %args = @_;

  if (!ref($prot)) {
    if (not defined $args{xstype} or not defined $args{code}) {
      die("Need xstype and code parameters");
    }
  }

  my $self = bless(
    (ref($prot) ? {%$prot} : {})
    => $class
  );

  $self->{xstype} = $args{xstype} if defined $args{xstype};
  $self->{code} = $args{code} if defined $args{code};
  $self->{code} =~ s/^(?=\S)/\t/mg;

  return $self;
}

=head2 code

Returns or sets the OUTPUT mapping code for this entry.

=cut

sub code {
  $_[0]->{code} = $_[1] if @_ > 1;
  return $_[0]->{code};
}

=head2 xstype

Returns the name of the XS type of the OUTPUT map.

=cut

sub xstype {
  return $_[0]->{xstype};
}

=head2 cleaned_code

Returns a cleaned-up copy of the code to which certain transformations
have been applied to make it more ANSI compliant.

=cut

sub cleaned_code {
  my $self = shift;
  my $code = $self->code;

  # Move C pre-processor instructions to column 1 to be strictly ANSI
  # conformant. Some pre-processors are fussy about this.
  $code =~ s/^\s+#/#/mg;
  $code =~ s/\s*\z/\n/;

  return $code;
}

=head2 targetable

This is an obscure optimization that used to live in C<ExtUtils::ParseXS>
directly.

In a nutshell, this will check whether the output code
involves calling C<set_iv>, C<set_uv>, C<set_nv>, C<set_pv> or C<set_pvn>
to set the special C<$arg> placeholder to a new value
B<AT THE END OF THE OUTPUT CODE>. If that is the case, the code is
eligible for using the C<TARG>-related macros to optimize this.
Thus the name of the method: C<targetable>.

If the optimization can not be applied, this returns undef.
If it can be applied, this method returns a hash reference containing
the following information:

  type:      Any of the characters i, u, n, p
  with_size: Bool indicating whether this is the sv_setpvn variant
  what:      The code that actually evaluates to the output scalar
  what_size: If "with_size", this has the string length (as code,
             not constant)

=cut

sub targetable {
  my $self = shift;
  return $self->{targetable} if exists $self->{targetable};

  our $bal; # ()-balanced
  $bal = qr[
    (?:
      (?>[^()]+)
      |
      \( (??{ $bal }) \)
    )*
  ]x;

  # matches variations on (SV*)
  my $sv_cast = qr[
    (?:
      \( \s* SV \s* \* \s* \) \s*
    )?
  ]x;

  my $size = qr[ # Third arg (to setpvn)
    , \s* (??{ $bal })
  ]x;

  my $code = $self->code;

  # We can still bootstrap compile 're', because in code re.pm is
  # available to miniperl, and does not attempt to load the XS code.
  use re 'eval';

  my ($type, $with_size, $arg, $sarg) =
    ($code =~
      m[^
        \s+
        sv_set([iunp])v(n)?    # Type, is_setpvn
        \s*
        \( \s*
          $sv_cast \$arg \s* , \s*
          ( (??{ $bal }) )    # Set from
        ( (??{ $size }) )?    # Possible sizeof set-from
        \) \s* ; \s* $
      ]x
  );

  my $rv = undef;
  if ($type) {
    $rv = {
      type      => $type,
      with_size => $with_size,
      what      => $arg,
      what_size => $sarg,
    };
  }
  $self->{targetable} = $rv;
  return $rv;
}

=head1 SEE ALSO

L<ExtUtils::Typemaps>

=head1 AUTHOR

Steffen Mueller C<<smueller@cpan.org>>

=head1 COPYRIGHT & LICENSE

Copyright 2009, 2010, 2011, 2012 Steffen Mueller

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

1;

