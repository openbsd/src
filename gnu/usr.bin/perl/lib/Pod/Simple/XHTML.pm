=pod

=head1 NAME

Pod::Simple::XHTML -- format Pod as validating XHTML

=head1 SYNOPSIS

  use Pod::Simple::XHTML;

  my $parser = Pod::Simple::XHTML->new();

  ...

  $parser->parse_file('path/to/file.pod');

=head1 DESCRIPTION

This class is a formatter that takes Pod and renders it as XHTML
validating HTML.

This is a subclass of L<Pod::Simple::Methody> and inherits all its
methods. The implementation is entirely different than
L<Pod::Simple::HTML>, but it largely preserves the same interface.

=cut

package Pod::Simple::XHTML;
use strict;
use vars qw( $VERSION @ISA $HAS_HTML_ENTITIES );
$VERSION = '3.04';
use Carp ();
use Pod::Simple::Methody ();
@ISA = ('Pod::Simple::Methody');

BEGIN {
  $HAS_HTML_ENTITIES = eval "require HTML::Entities; 1";
}

my %entities = (
  q{>} => 'gt',
  q{<} => 'lt',
  q{'} => '#39',
  q{"} => 'quot',
  q{&} => 'amp',
);

sub encode_entities {
  return HTML::Entities::encode_entities( $_[0] ) if $HAS_HTML_ENTITIES;
  my $str = $_[0];
  my $ents = join '', keys %entities;
  $str =~ s/([$ents])/'&' . $entities{$1} . ';'/ge;
  return $str;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

=head1 METHODS

Pod::Simple::XHTML offers a number of methods that modify the format of
the HTML output. Call these after creating the parser object, but before
the call to C<parse_file>:

  my $parser = Pod::PseudoPod::HTML->new();
  $parser->set_optional_param("value");
  $parser->parse_file($file);

=head2 perldoc_url_prefix

In turning L<Foo::Bar> into http://whatever/Foo%3a%3aBar, what
to put before the "Foo%3a%3aBar". The default value is
"http://search.cpan.org/perldoc?".

=head2 perldoc_url_postfix

What to put after "Foo%3a%3aBar" in the URL. This option is not set by
default.

=head2 title_prefix, title_postfix

What to put before and after the title in the head. The values should
already be &-escaped.

=head2 html_css

  $parser->html_css('path/to/style.css');

The URL or relative path of a CSS file to include. This option is not
set by default.

=head2 html_javascript

The URL or relative path of a JavaScript file to pull in. This option is
not set by default.

=head2 html_doctype

A document type tag for the file. This option is not set by default.

=head2 html_header_tags

Additional arbitrary HTML tags for the header of the document. The
default value is just a content type header tag:

  <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1">

Add additional meta tags here, or blocks of inline CSS or JavaScript
(wrapped in the appropriate tags).

=head2 default_title

Set a default title for the page if no title can be determined from the
content. The value of this string should already be &-escaped.

=head2 force_title

Force a title for the page (don't try to determine it from the content).
The value of this string should already be &-escaped.

=head2 html_header, html_footer

Set the HTML output at the beginning and end of each file. The default
header includes a title, a doctype tag (if C<html_doctype> is set), a
content tag (customized by C<html_header_tags>), a tag for a CSS file
(if C<html_css> is set), and a tag for a Javascript file (if
C<html_javascript> is set). The default footer simply closes the C<html>
and C<body> tags.

The options listed above customize parts of the default header, but
setting C<html_header> or C<html_footer> completely overrides the
built-in header or footer. These may be useful if you want to use
template tags instead of literal HTML headers and footers or are
integrating converted POD pages in a larger website.

If you want no headers or footers output in the HTML, set these options
to the empty string.

=head2 index

TODO -- Not implemented.

Whether to add a table-of-contents at the top of each page (called an
index for the sake of tradition).


=cut

__PACKAGE__->_accessorize(
 'perldoc_url_prefix',
 'perldoc_url_postfix',
 'title_prefix',  'title_postfix',
 'html_css', 
 'html_javascript',
 'html_doctype',
 'html_header_tags',
 'title', # Used internally for the title extracted from the content
 'default_title',
 'force_title',
 'html_header',
 'html_footer',
 'index',
 'batch_mode', # whether we're in batch mode
 'batch_mode_current_level',
    # When in batch mode, how deep the current module is: 1 for "LWP",
    #  2 for "LWP::Procotol", 3 for "LWP::Protocol::GHTTP", etc
);

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

=head1 SUBCLASSING

If the standard options aren't enough, you may want to subclass
Pod::Simple::XHMTL. These are the most likely candidates for methods
you'll want to override when subclassing.

=cut

sub new {
  my $self = shift;
  my $new = $self->SUPER::new(@_);
  $new->{'output_fh'} ||= *STDOUT{IO};
  $new->accept_targets( 'html', 'HTML' );
  $new->perldoc_url_prefix('http://search.cpan.org/perldoc?');
  $new->html_header_tags('<meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1">');
  $new->nix_X_codes(1);
  $new->codes_in_verbatim(1);
  $new->{'scratch'} = '';
  return $new;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

=head2 handle_text

This method handles the body of text within any element: it's the body
of a paragraph, or everything between a "=begin" tag and the
corresponding "=end" tag, or the text within an L entity, etc. You would
want to override this if you are adding a custom element type that does
more than just display formatted text. Perhaps adding a way to generate
HTML tables from an extended version of POD.

So, let's say you want add a custom element called 'foo'. In your
subclass's C<new> method, after calling C<SUPER::new> you'd call:

  $new->accept_targets_as_text( 'foo' );

Then override the C<start_for> method in the subclass to check for when
"$flags->{'target'}" is equal to 'foo' and set a flag that marks that
you're in a foo block (maybe "$self->{'in_foo'} = 1"). Then override the
C<handle_text> method to check for the flag, and pass $text to your
custom subroutine to construct the HTML output for 'foo' elements,
something like:

  sub handle_text {
      my ($self, $text) = @_;
      if ($self->{'in_foo'}) {
          $self->{'scratch'} .= build_foo_html($text); 
      } else {
          $self->{'scratch'} .= $text;
      }
  }

=cut

sub handle_text {
    # escape special characters in HTML (<, >, &, etc)
    $_[0]{'scratch'} .= $_[0]{'in_verbatim'} ? encode_entities( $_[1] ) : $_[1]
}

sub start_Para     { $_[0]{'scratch'} = '<p>' }
sub start_Verbatim { $_[0]{'scratch'} = '<pre><code>'; $_[0]{'in_verbatim'} = 1}

sub start_head1 {  $_[0]{'scratch'} = '<h1>' }
sub start_head2 {  $_[0]{'scratch'} = '<h2>' }
sub start_head3 {  $_[0]{'scratch'} = '<h3>' }
sub start_head4 {  $_[0]{'scratch'} = '<h4>' }

sub start_item_bullet { $_[0]{'scratch'} = '<li>' }
sub start_item_number { $_[0]{'scratch'} = "<li>$_[1]{'number'}. "  }
sub start_item_text   { $_[0]{'scratch'} = '<li>'   }

sub start_over_bullet { $_[0]{'scratch'} = '<ul>'; $_[0]->emit }
sub start_over_text   { $_[0]{'scratch'} = '<ul>'; $_[0]->emit }
sub start_over_block  { $_[0]{'scratch'} = '<ul>'; $_[0]->emit }
sub start_over_number { $_[0]{'scratch'} = '<ol>'; $_[0]->emit }

sub end_over_bullet { $_[0]{'scratch'} .= '</ul>'; $_[0]->emit }
sub end_over_text   { $_[0]{'scratch'} .= '</ul>'; $_[0]->emit }
sub end_over_block  { $_[0]{'scratch'} .= '</ul>'; $_[0]->emit }
sub end_over_number { $_[0]{'scratch'} .= '</ol>'; $_[0]->emit }

# . . . . . Now the actual formatters:

sub end_Para     { $_[0]{'scratch'} .= '</p>'; $_[0]->emit }
sub end_Verbatim {
    $_[0]{'scratch'}     .= '</code></pre>';
    $_[0]{'in_verbatim'}  = 0;
    $_[0]->emit;
}

sub end_head1       { $_[0]{'scratch'} .= '</h1>'; $_[0]->emit }
sub end_head2       { $_[0]{'scratch'} .= '</h2>'; $_[0]->emit }
sub end_head3       { $_[0]{'scratch'} .= '</h3>'; $_[0]->emit }
sub end_head4       { $_[0]{'scratch'} .= '</h4>'; $_[0]->emit }

sub end_item_bullet { $_[0]{'scratch'} .= '</li>'; $_[0]->emit }
sub end_item_number { $_[0]{'scratch'} .= '</li>'; $_[0]->emit }
sub end_item_text   { $_[0]->emit }

# This handles =begin and =for blocks of all kinds.
sub start_for { 
  my ($self, $flags) = @_;
  $self->{'scratch'} .= '<div';
  $self->{'scratch'} .= ' class="'.$flags->{'target'}.'"' if ($flags->{'target'});
  $self->{'scratch'} .= '>';
  $self->emit;

}
sub end_for { 
  my ($self) = @_;
  $self->{'scratch'} .= '</div>';
  $self->emit;
}

sub start_Document { 
  my ($self) = @_;
  if (defined $self->html_header) {
    $self->{'scratch'} .= $self->html_header;
    $self->emit unless $self->html_header eq "";
  } else {
    my ($doctype, $title, $metatags);
    $doctype = $self->html_doctype || '';
    $title = $self->force_title || $self->title || $self->default_title || '';
    $metatags = $self->html_header_tags || '';
    if ($self->html_css) {
      $metatags .= "\n<link rel='stylesheet' href='" .
             $self->html_css . "' type='text/css'>";
    }
    if ($self->html_javascript) {
      $metatags .= "\n<script type='text/javascript' src='" .
                    $self->html_javascript . "'></script>";
    }
    $self->{'scratch'} .= <<"HTML";
$doctype
<html>
<head>
<title>$title</title>
$metatags
</head>
<body>
HTML
    $self->emit;
  }
}

sub end_Document   { 
  my ($self) = @_;
  if (defined $self->html_footer) {
    $self->{'scratch'} .= $self->html_footer;
    $self->emit unless $self->html_footer eq "";
  } else {
    $self->{'scratch'} .= "</body>\n</html>";
    $self->emit;
  }
}

# Handling code tags
sub start_B { $_[0]{'scratch'} .= '<b>' }
sub end_B   { $_[0]{'scratch'} .= '</b>' }

sub start_C { $_[0]{'scratch'} .= '<code>' }
sub end_C   { $_[0]{'scratch'} .= '</code>' }

sub start_E { $_[0]{'scratch'} .= '&' }
sub end_E   { $_[0]{'scratch'} .= ';' }

sub start_F { $_[0]{'scratch'} .= '<i>' }
sub end_F   { $_[0]{'scratch'} .= '</i>' }

sub start_I { $_[0]{'scratch'} .= '<i>' }
sub end_I   { $_[0]{'scratch'} .= '</i>' }

sub start_L { 
  my ($self, $flags) = @_;
    my $url;
    if ($flags->{'type'} eq 'url') {
      $url = $flags->{'to'};
    } elsif ($flags->{'type'} eq 'pod') {
      $url .= $self->perldoc_url_prefix || '';
      $url .= $flags->{'to'} || '';
      $url .= '/' . $flags->{'section'} if ($flags->{'section'});
      $url .= $self->perldoc_url_postfix || '';
#    require Data::Dumper;
#    print STDERR Data::Dumper->Dump([$flags]);
    }

    $self->{'scratch'} .= '<a href="'. $url . '">';
}
sub end_L   { $_[0]{'scratch'} .= '</a>' }

sub start_S { $_[0]{'scratch'} .= '<nobr>' }
sub end_S   { $_[0]{'scratch'} .= '</nobr>' }

sub emit {
  my($self) = @_;
  my $out = $self->{'scratch'} . "\n";
  print {$self->{'output_fh'}} $out, "\n";
  $self->{'scratch'} = '';
  return;
}

# Bypass built-in E<> handling to preserve entity encoding
sub _treat_Es {} 

1;

__END__

=head1 SEE ALSO

L<Pod::Simple>, L<Pod::Simple::Methody>

=head1 COPYRIGHT

Copyright (c) 2003-2005 Allison Randal.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. The full text of the license
can be found in the LICENSE file included with this module.

This library is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Allison Randal <allison@perl.org>

=cut

