use strict;
use warnings;
use Test::More;

BEGIN {
    package MyXHTML;
    use base 'Pod::Simple::XHTML';

    sub new {
        my $class = shift;
        my $self = $class->SUPER::new(@_);
        $self->html_header('');
        $self->html_footer('');
        $self->index(1);
        $self->anchor_items(1);
        return $self;
    }

    sub parse_to_string {
        my $self = shift;
        my $pod = shift;
        my $output = '';
        $self->output_string( \$output );
        $self->parse_string_document($pod);
        return $output;
    }

    sub idify {
        my ($self, $t, $not_unique) = @_;
        for ($t) {
            $t =~ s/\A\s+//;
            $t =~ s/\s+\z//;
            $t =~ s/[\s-]+/-/g;
        }
        return $t if $not_unique;
        my $i = '';
        $i++ while $self->{ids}{"$t$i"}++;
        return "$t$i";
    }
}


my @tests = (
    # Pod                   id                        link (url encoded)
    [ 'Foo',                'Foo',                    'Foo'                       ],
    [ '$@',                 '$@',                     '%24%40'                    ],
    [ 'With C<Formatting>', 'With-Formatting',        'With-Formatting'           ],
    [ '$obj->method($foo)', '$obj->method($foo)',     '%24obj-%3Emethod(%24foo)'  ],
);

plan tests => 5 * scalar @tests;

my $parser = MyXHTML->new;

for my $names (@tests) {
    my ($heading, $id, $link) = @$names;

    is $link, $parser->encode_url($id),
        'assert correct encoding of url fragment';

    my $html_id = $parser->encode_entities($id);

    {
        my $result = MyXHTML->new->parse_to_string(<<"EOT");
=head1 $heading

L<< /$heading >>

EOT
        like $result, qr{<h1 id="\Q$html_id\E">},
            "heading id generated correctly for '$heading'";
        like $result, qr{<li><a href="\#\Q$link\E">},
            "index link generated correctly for '$heading'";
        like $result, qr{<p><a href="\#\Q$link\E">},
            "L<> link generated correctly for '$heading'";
    }
    {
        my $result = MyXHTML->new->parse_to_string(<<"EOT");
=over 4

=item $heading

=back

EOT
        like $result, qr{<dt id="\Q$html_id\E">},
            "item id generated correctly for '$heading'";
    }
}
