BEGIN {
        chdir 't' if -d 't';
        @INC = '../lib';
}

use MIME::QuotedPrint;

$x70 = "x" x 70;

@tests =
  (
   # plain ascii should not be encoded
   ["quoted printable"  =>
    "quoted printable"],

   # 8-bit chars should be encoded
   ["v\xe5re kj\xe6re norske tegn b\xf8r \xe6res" =>
    "v=E5re kj=E6re norske tegn b=F8r =E6res"],

   # trailing space should be encoded
   ["  " => "=20=20"],
   ["\tt\t" => "\tt=09"],
   ["test  \ntest\n\t \t \n" => "test=20=20\ntest\n=09=20=09=20\n"],

   # "=" is special an should be decoded
   ["=\n" => "=3D\n"],
   ["\0\xff" => "=00=FF"],

   # Very long lines should be broken (not more than 76 chars
   ["The Quoted-Printable encoding is intended to represent data that largly consists of octets that correspond to printable characters in the ASCII character set." =>
    "The Quoted-Printable encoding is intended to represent data that largly con=
sists of octets that correspond to printable characters in the ASCII charac=
ter set."
    ],

   # Long lines after short lines were broken through 2.01.
   ["short line
In America, any boy may become president and I suppose that's just one of the risks he takes. -- Adlai Stevenson" =>
    "short line
In America, any boy may become president and I suppose that's just one of t=
he risks he takes. -- Adlai Stevenson"],

   # My (roderick@argon.org) first crack at fixing that bug failed for
   # multiple long lines.
   ["College football is a game which would be much more interesting if the faculty played instead of the students, and even more interesting if the
trustees played.  There would be a great increase in broken arms, legs, and necks, and simultaneously an appreciable diminution in the loss to humanity. -- H. L. Mencken" =>
    "College football is a game which would be much more interesting if the facu=
lty played instead of the students, and even more interesting if the
trustees played.  There would be a great increase in broken arms, legs, and=
 necks, and simultaneously an appreciable diminution in the loss to humanit=
y. -- H. L. Mencken"],

   # Don't break a line that's near but not over 76 chars.
   ["$x70!23"		=> "$x70!23"],
   ["$x70!234"		=> "$x70!234"],
   ["$x70!2345"		=> "$x70!2345"],
   ["$x70!23456"	=> "$x70!23456"],
   ["$x70!23\n"		=> "$x70!23\n"],
   ["$x70!234\n"	=> "$x70!234\n"],
   ["$x70!2345\n"	=> "$x70!2345\n"],
   ["$x70!23456\n"	=> "$x70!23456\n"],

   # Not allowed to break =XX escapes using soft line break
   ["$x70===xxxx" => "$x70=3D=\n=3D=3Dxxxx"],
   ["$x70!===xxx" => "$x70!=3D=\n=3D=3Dxxx"],
   ["$x70!!===xx" => "$x70!!=3D=\n=3D=3Dxx"],
   ["$x70!!!===x" => "$x70!!!=\n=3D=3D=3Dx"],
   #                            ^
   #                    70123456|
   #                           max
   #                        line width
);

$notests = @tests + 3;
print "1..$notests\n";

$testno = 0;
for (@tests) {
    $testno++;
    ($plain, $encoded) = @$_;
    if (ord('A') == 193) {  # EBCDIC 8 bit chars are different
        if ($testno == 2) { $plain =~ s/\xe5/\x47/; $plain =~ s/\xe6/\x9c/g; $plain =~ s/\xf8/\x70/; }
        if ($testno == 7) { $plain =~ s/\xff/\xdf/; }
    }
    $x = encode_qp($plain);
    if ($x ne $encoded) {
	print "Encode test failed\n";
	print "Got:      '$x'\n";
	print "Expected: '$encoded'\n";
	print "not ok $testno\n";
	next;
    }
    $x = decode_qp($encoded);
    if ($x ne $plain) {
	print "Decode test failed\n";
	print "Got:      '$x'\n";
	print "Expected: '$plain'\n";
	print "not ok $testno\n";
	next;
    }
    print "ok $testno\n";
}

# Some extra testing for a case that was wrong until libwww-perl-5.09
print "not " unless decode_qp("foo  \n\nfoo =\n\nfoo=20\n\n") eq
                                "foo\n\nfoo \nfoo \n\n";
$testno++; print "ok $testno\n";

# Same test but with "\r\n" terminated lines
print "not " unless decode_qp("foo  \r\n\r\nfoo =\r\n\r\nfoo=20\r\n\r\n") eq
                                "foo\r\n\r\nfoo \r\nfoo \r\n\r\n";
$testno++; print "ok $testno\n";

print "not " if eval { encode_qp("XXX \x{100}") } || $@ !~ /^The Quoted-Printable encoding is only defined for bytes/;
$testno++; print "ok $testno\n";
