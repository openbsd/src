hugo : module
  
  synmode a = range(1:10);
  synmode p = powerset a;
  
  synmode s = set (sa, sb, sc);
  synmode s_ps = powerset s;
  
  x: proc (ps p);
    dcl i a;
    do for i in ps;
      writetext (stdout, "%C ", i);
    od;
    writetext(stdout, "%/");
  end x;
  
  y : proc (ps s_ps);
    dcl i s;
    do for i in ps;
      writetext (stdout, "%C ", i);
    od;
    writetext(stdout, "%/");
  end y;

  dummy: proc ();
  end dummy;
      
  x([1,2,3]);
  y([sa, sc]);
  dummy ();
  
end hugo;
