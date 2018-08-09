. ../bin/activate


for line in $(ls -1)
    do
    pycodestyle $line
done
