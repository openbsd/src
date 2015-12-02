echo pöüp | cut -b 2-5 > t2.out
echo pöüp | cut -c 2-3 >> t2.out
echo pöüp | cut -b 3-4 -n >> t2.out
diff t2.exp t2.out
