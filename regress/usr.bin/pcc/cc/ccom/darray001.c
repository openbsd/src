/* should not issue: darray001.c, line 5: compiler error: bad conversion */
/* From TAKAHASHI Tamotsu */
int main(void) {
        int n=1;
        int a[1][n];
        a[0][0]=1; /* this line */
        return 0;
}

