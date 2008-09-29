use re 'debug';

$_ = 'foo bar baz bop fip fop';

/foo/ and $count++;

{
    no re 'debug';
    /bar/ and $count++;
    {
        use re 'debug';
        /baz/ and $count++;
    }
    /bop/ and $count++;
}

/fip/ and $count++;

no re 'debug';

/fop/ and $count++;

use re 'debug';
my $var='zoo|liz|zap';
/($var)/ or $count++;

print "Count=$count\n";


