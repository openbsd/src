use strict;
use warnings;
use Test::More tests => 4;

{
  package Pod::Simple::ErrorFinder;
  use base 'Pod::Simple::DumpAsXML'; # arbitrary choice -- rjbs, 2013-04-16

  my @errors;
  sub whine {
    my ($self, @rest) = @_;
    push @errors, [ @rest ];
    $self->SUPER::whine(@rest);
  }

  sub scream {
    my ($self, @rest) = @_;
    push @errors, [ @rest ];
    $self->SUPER::scream(@rest);
  }

  sub errors_for_input {
    my ($class, $input, $mutor) = @_;
    @errors = ();

    my $parser = $class->new;
    my $output = '';
    $parser->output_string( \$output );
    $parser->parse_string_document( $input );

    @errors = sort { $a->[0] <=> $b->[0]
                  || $a->[1] cmp $b->[1] } @errors;

    return @errors;
  }
}

sub errors { Pod::Simple::ErrorFinder->errors_for_input(@_) }

{
  my @errors = errors("=over 4\n\n=item 1\n\nHey\n\n");
  is_deeply(
    \@errors,
    [ [ 1, "=over without closing =back" ] ],
    "no closing =back",
  );
}

{
  for my $l_code ('L< foo>', 'L< bar>') {
    my $input = "=pod\n\nAmbiguous space: $l_code\n";
    my @errors = errors("$input");
    is_deeply(
      \@errors,
      [ [ 3, "L<> starts or ends with whitespace" ] ],
      "warning for space in $l_code",
    );
  }
}

{
  my $input = "=pod\n\nAmbiguous slash: L<I/O Operators|op/io>\n";
  my @errors = errors("$input");
  is_deeply(
    \@errors,
    [ [ 3, "alternative text 'I/O Operators' contains non-escaped | or /" ] ],
    "warning for / in text part of L<>",
  );
}
