xx : module
  
dcl a chars(200) varying init := (70)'^(0)' // "Jason""^(0,5)""Hugo^(10)" // (70)'^(1)';
dcl b chars(20) varying init := "Jason""^(0,5)""Hugo^(10)";
dcl c chars(256) varying init := (70)'a' // "^(0,5)Jason" // (70)'b';
dcl d char init := '^(11)';

bulk: PROC ();
END bulk;

a := (50) '^(255,0,222,127)';
b := (1)'^(200)';
d := 'a';

c:= (256)" ";

DO FOR i:= 0 BY 1 TO 255;
   c (255-i) := char (i);
OD;
  
bulk ();

end xx;