package ExtUtils::Typemaps::OutputMap;
use 5.006001;
use strict;
use warnings;
our $VERSION = '3.57';

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

=head2 targetable_legacy

Do not use for new code.

This is the original version of the targetable() method, whose behaviour
has been frozen for backwards compatibility. It is used to determine
whether to emit an early C<dXSTARG>, which will be in scope for most of
the XSUB. More recent XSUB code generation emits a C<dXSTARG> in a tighter
scope if one has not already been emitted. Some XS code assumes that
C<TARG> has been declared, so continue to declare it under the same
conditions as before. The newer C<targetable> method may be true under
additional circumstances.

If the optimization can not be applied, this returns undef.  If it can be
applied, this method returns a hash reference containing the following
information:

  type:      Any of the characters i, u, n, p
  with_size: Bool indicating whether this is the sv_setpvn variant
  what:      The code that actually evaluates to the output scalar
  what_size: If "with_size", this has the string length (as code,
             not constant, including leading comma)


=cut

sub targetable_legacy {
  my $self = shift;
  return $self->{targetable_legacy} if exists $self->{targetable_legacy};

  our $bal; # ()-balanced
  $bal = qr[
    (?:
      (?>[^()]+)
      |
      \( (??{ $bal }) \)
    )*
  ]x;
  my $bal_no_comma = qr[
    (?:
      (?>[^(),]+)
      |
      \( (??{ $bal }) \)
    )+
  ]x;

  # matches variations on (SV*)
  my $sv_cast = qr[
    (?:
      \( \s* SV \s* \* \s* \) \s*
    )?
  ]x;

  my $size = qr[ # Third arg (to setpvn)
    , \s* (??{ $bal })
  ]xo;

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
          ( $bal_no_comma )    # Set from
          ( $size )?           # Possible sizeof set-from
        \s* \) \s* ; \s* $
      ]xo
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
  $self->{targetable_legacy} = $rv;
  return $rv;
}

=head2 targetable

Class method.

Return a boolean indicating whether the supplied code snippet is suitable
for using TARG as the destination SV rather than an new mortal.

In principle most things are, except expressions which would set the SV
to a ref value. That can cause the referred value to never be freed, as
targs aren't freed (at least for the lifetime of their CV). So in
practice, we restrict it to an approved list of sv_setfoo() forms, and
only where there is no extra code following the sv_setfoo() (so we have to
match the closing bracket, allowing for nested brackets etc within).

=cut

my %targetable_cache;

sub targetable {
  my ($class, $code) = @_;

  return $targetable_cache{$code} if exists $targetable_cache{$code};

  # Match a string with zero or more balanced/nested parentheses
  # within it, e.g.
  #
  #   "aa,bb(cc,dd)ee(ff,gg(hh,ii)jj,kk)ll"

  our $bal;
  $bal = qr[
    (?:
      (?>[^()]+)
      |
      " ([^"] | \\")* "
      |
      \( (??{ $bal }) \)
    )*
  ]x;

  # Like $bal, but doesn't allow commas at the *top* level; e.g.
  #
  #       "aabb(cc,dd)ee(ff,gg(hh,ii)jj,kk)ll"
  #
  # Something like "aa,bb(cc,dd)" will just match/consume the "aa"
  # part of the string.

  my $bal_no_comma = qr[
    (?:
      (?>[^(),]+)
      |
      " ([^"] | \\")* "
      |
      \( (??{ $bal }) \)
    )+
  ]x;

  # the SV whose value is to be set (typically arg 1)
  # Note that currently ParseXS will always call with $arg expanded
  # to 'RETVALSV', but also match other possibilities too for future
  # use.

  my $target = qr[
    (?:
      \( \s* SV \s* \* \s* \) \s*   # optional SV* cast
    )?
    (?:
        \$arg
    |
        RETVAL
    |
        RETVALSV
    |
        ST\(\d+\)
    )
    \s*
  ]x;

  # We can still bootstrap compile 're', because in code re.pm is
  # available to miniperl, and does not attempt to load the XS code.
  use re 'eval';

  my $match =
    ($code =~
      m[^
        \s*
        (?:
            # 1-arg functions
            sv_set_(?:undef|true|false)
            \s*
            \( \s*
              $target                # arg 1: SV to set
        |
            # 2-arg functions
            sv_set(?:iv|iv_mg|uv|uv_mg|nv|nv_mg|pv|pv_mg|_bool)
            \s*
            \( \s*
              $target                # arg 1: SV to set
              , \s*
              $bal_no_comma          # arg 2: value to use
        |
            # 3-arg functions
            sv_set(?:pvn|pvn_mg)
            \s*
            \( \s*
              $target                # arg 1: SV to set
              , \s*
              $bal_no_comma          # arg 2: value to use
              , \s*
              $bal_no_comma          # arg 3: length
        )
        \s* \)
        \s* ; \s*
        $
      ]xo
  );

  $targetable_cache{$code} = $match;
  return $match;
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

