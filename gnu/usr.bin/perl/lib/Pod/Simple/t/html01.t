# t/html-para.t

BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN { plan tests => 8 };

#use Pod::Simple::Debug (10);

use Pod::Simple::HTML;

sub x ($) { Pod::Simple::HTML->_out(
  sub{  $_[0]->bare_output(1)  },
  "=pod\n\n$_[0]",
) }

ok( x(
q{
=pod
 
This is a paragraph
 
=cut
}),
  qq{\n<p>This is a paragraph</p>\n},
  "paragraph building"
);


ok( x(qq{=pod\n\nThis is a paragraph}),
 qq{\n<p>This is a paragraph</p>\n},
 "paragraph building"
);


ok( x(qq{This is a paragraph}),
 qq{\n<p>This is a paragraph</p>\n},
 "paragraph building"
);



ok(x(
'=head1 This is a heading')
 => q{/\s*<h1><a[^<>]+>This\s+is\s+a\s+heading</a></h1>\s*$/},
  "heading building"
);

ok(x(
'=head2 This is a heading too')
 => q{/\s*<h2><a[^<>]+>This\s+is\s+a\s+heading\s+too</a></h2>\s*$/},
  "heading building"
);

ok(x(
'=head3 Also, this is a heading')
 => q{/\s*<h3><a[^<>]+>Also,\s+this\s+is\s+a\s+heading</a></h3>\s*$/},
  "heading building"
);


ok(x(
'=head4 This, too, is a heading')
 => q{/\s*<h4><a[^<>]+>This,\s+too,\s+is\s+a\s+heading</a></h4>\s*$/},
  "heading building"
);


print "# And one for the road...\n";
ok 1;

