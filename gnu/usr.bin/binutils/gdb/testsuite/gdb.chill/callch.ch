hack : module

dcl i int;
newmode otto = array (bool, bool) byte;
newmode str1 = struct (f1 int, f2 bool);
newmode str2 = struct (f1 otto);

dcl a otto := [[1,1],[1,1]];
dcl b str1 := [10, false];
dcl c str2;

fred : proc (a int in, b int loc);
  writetext(stdout, "a is '%C'; b is '%C'.%/", a, b);
end fred;

klaus : proc ();
  writetext(stdout, "here's klaus calling.%/");
end klaus;

king : proc (p otto loc, x otto in);
  dcl i, j bool;
  p := [[h'ff,h'ff],[h'ff,h'ff]];
  do for i:= lower(bool) to upper(bool);
    do for j:= lower(bool) to upper(bool);
      writetext(stdout, "x(%C, %C) = %C%..%/", i, j, x(i, j));
      writetext(stdout, "p(%C, %C) = %C%..%/", i, j, p(i, j));
    od;
  od;
end king;

ralph : proc (x str1 in);
  writetext(stdout, "x.f1 = %C, x.f2 = %C%..%/", x.f1, x.f2);
end ralph;

whitney : proc (x str2 in);
  dcl i, j bool;

  do for i:= lower(bool) to upper(bool);
    do for j:= lower(bool) to upper(bool);
      writetext(stdout, "x.f1(%C, %C) = %C%..%/", i, j, x.f1(i, j));
     od;
  od;

end whitney;

c := [a];
i:=12;
writetext(stdout, "done.%/");

end hack;
