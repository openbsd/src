# Testing HTML paragraphs
use strict;
use warnings;
use Test::More tests => 15;

#use Pod::Simple::Debug (10);

use Pod::Simple::HTML;

sub x {
  my $code = $_[1];
  Pod::Simple::HTML->_out(
  sub{  $_[0]->bare_output(1); $code->($_[0]) if $code  },
  "=pod\n\n$_[0]",
) }

is( x(
q{
=pod

This is a paragraph

=cut
}),
  qq{\n<p>This is a paragraph</p>\n},
  "paragraph building"
);


is( x(qq{=pod\n\nThis is a paragraph}),
 qq{\n<p>This is a paragraph</p>\n},
 "paragraph building"
);


is( x(qq{This is a paragraph}),
 qq{\n<p>This is a paragraph</p>\n},
 "paragraph building"
);



like(x(
'=head1 This is a heading')
 => qr{\s*<h1><a[^<>]+>This\s+is\s+a\s+heading</a></h1>\s*$},
  "heading building"
);

like(x('=head1 This is a heading', sub { $_[0]->html_h_level(2) })
 => qr{\s*<h2><a[^<>]+>This\s+is\s+a\s+heading</a></h2>\s*$},
  "heading building"
);

like(x(
'=head2 This is a heading too')
 => qr{\s*<h2><a[^<>]+>This\s+is\s+a\s+heading\s+too</a></h2>\s*$},
  "heading building"
);

like(x(
'=head3 Also, this is a heading')
 => qr{\s*<h3><a[^<>]+>Also,\s+this\s+is\s+a\s+heading</a></h3>\s*$},
  "heading building"
);


like(x(
'=head4 This, too, is a heading')
 => qr{\s*<h4><a[^<>]+>This,\s+too,\s+is\s+a\s+heading</a></h4>\s*$},
  "heading building"
);

like(x(
'=head5 The number of the heading shall be five')
 => qr{\s*<h5><a[^<>]+>The\s+number\s+of\s+the\s+heading\s+shall\s+be\s+five</a></h5>\s*$},
  "heading building"
);

like(x(
'=head6 The sixth a heading is the perfect heading')
 => qr{\s*<h6><a[^<>]+>The\s+sixth\s+a\s+heading\s+is\s+the\s+perfect\s+heading</a></h6>\s*$},
  "heading building"
);

like(x(
'=head2 Yada Yada Operator
X<...> X<... operator> X<yada yada operator>')
 => qr{name="Yada_Yada_Operator"},
  "heading anchor name"
);

is(
    x("=over 4\n\n=item one\n\n=item two\n\nHello\n\n=back\n"),
    q{
<dl>
<dt><a name="one"
>one</a></dt>

<dd>
<dt><a name="two"
>two</a></dt>

<dd>
<p>Hello</p>
</dd>
</dl>
}
);

my $html = q{<tt>
<pre>
#include &lt;stdio.h&gt;

int main(int argc,char *argv[]) {

        printf("Hellow World\n");
        return 0;

}
</pre>
</tt>};
is(
    x("=begin html\n\n$html\n\n=end html\n"),
    "$html\n\n"
);

# Check subclass.
SUBCLASS: {
    package My::Pod::HTML;
    use vars '@ISA', '$VERSION';
    @ISA = ('Pod::Simple::HTML');
    $VERSION = '0.01';
    sub do_section { 'howdy' }
}

is(
    My::Pod::HTML->_out(
        sub{  $_[0]->bare_output(1)  },
        "=pod\n\n=over\n\n=item Foo\n\n=back\n",
    ),
    "\n<dl>\n<dt><a name=\"howdy\"\n>Foo</a></dt>\n</dl>\n",
);

{   # Test that strip_verbatim_indent() works.  github issue #i5
    my $output;

    my $obj = Pod::Simple::HTML->new;
    $obj->strip_verbatim_indent("  ");
    $obj->output_string(\$output);
    $obj->parse_string_document("=pod\n\n  First line\n  2nd line\n");
    like($output, qr!<pre>First line\n2nd line</pre>!s);
}
