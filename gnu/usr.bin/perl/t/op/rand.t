#!./perl

# From: kgb@ast.cam.ac.uk (Karl Glazebrook)

print "1..4\n";

srand;

$m=0; 
for(1..1000){ 
   $n = rand(1);
   if ($n<0 || $n>=1) {
       print "not ok 1\n# The value of randbits is likely too low in config.sh\n";
       exit
   }
   $m += $n;

}
$m=$m/1000;
print "ok 1\n";

if ($m<0.4) {
    print "not ok 2\n# The value of randbits is likely too high in config.sh\n";
}
elsif ($m>0.6) {
    print "not ok 2\n# Something's really weird about rand()'s distribution.\n";
}else{
    print "ok 2\n";
}

srand;

$m=0; 
for(1..1000){ 
   $n = rand(100);
   if ($n<0 || $n>=100) {
       print "not ok 3\n";
       exit
   }
   $m += $n;

}
$m=$m/1000;
print "ok 3\n";

if ($m<40 || $m>60) {
    print "not ok 4\n";
}else{
    print "ok 4\n";
}


