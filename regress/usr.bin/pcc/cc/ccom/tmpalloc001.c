/* From Ted Unangst */
int a() { return 1; }

int main()
{
	int b = 0;
	a() + ++b;
	printf("b %d\n", b);
	return 0;
}
