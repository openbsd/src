use Test2::Bundle::Extended;

use Test2::Util::Sub qw{
    sub_name
};

imported_ok qw{
    sub_name
};

sub named { 'named' }
*unnamed = sub { 'unnamed' };
like(sub_name(\&named), qr/named$/, "got sub name (named)");
like(sub_name(\&unnamed), qr/__ANON__$/, "got sub name (anon)");

like(
    dies { sub_name() },
    qr/sub_name requires a coderef as its only argument/,
    "Need an arg"
);

like(
    dies { sub_name('xxx') },
    qr/sub_name requires a coderef as its only argument/,
    "Need a ref"
);

like(
    dies { sub_name({}) },
    qr/sub_name requires a coderef as its only argument/,
    "Need a code ref"
);

done_testing;
