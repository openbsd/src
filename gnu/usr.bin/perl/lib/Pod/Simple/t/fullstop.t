BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN { plan tests => 11 };

#use Pod::Simple::Debug (6);

print "# Hi, I'm ", __FILE__, "\n";
ok 1;

use Pod::Simple;
use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";
sub e ($$) { Pod::Simple::DumpAsXML->_duo(@_) }

&ok( e "", "" );
&ok( e "\n", "", );

die unless ok !! Pod::Simple::XMLOutStream->can('fullstop_space_harden');
sub harden { $_[0]->fullstop_space_harden(1) }

print "# Test that \".  \" always compacts without the hardening on...\n";

ok( Pod::Simple::XMLOutStream->_out("\n=pod\n\nShe set me a message about the M.D.  I\ncalled back!\n"),
  qq{<Document><Para>She set me a message about the M.D. I called back!</Para></Document>}
);
ok( Pod::Simple::XMLOutStream->_out("\n=pod\n\nShe set me a message about the M.D. I called back!\n"),
  qq{<Document><Para>She set me a message about the M.D. I called back!</Para></Document>}
);
ok( Pod::Simple::XMLOutStream->_out("\n=pod\n\nShe set me a message about the M.D.\nI called back!\n"),
  qq{<Document><Para>She set me a message about the M.D. I called back!</Para></Document>}
);


print "# Now testing with the hardening on...\n";

ok( Pod::Simple::XMLOutStream->_out(\&harden, "\n=pod\n\nShe set me a message about the M.D.  I\ncalled back!\n"),
  qq{<Document><Para>She set me a message about the M.D.&#160; I called back!</Para></Document>}
);
ok( Pod::Simple::XMLOutStream->_out(\&harden, "\n=pod\n\nShe set me a message about the M.D. I called back!\n"),
  qq{<Document><Para>She set me a message about the M.D. I called back!</Para></Document>}
);
ok( Pod::Simple::XMLOutStream->_out(\&harden, "\n=pod\n\nShe set me a message about the M.D.\nI called back!\n"),
  qq{<Document><Para>She set me a message about the M.D. I called back!</Para></Document>}
);


print "# Byebye\n";
ok 1;

