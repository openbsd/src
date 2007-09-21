/* From Ted Unangst */
int a() { return 1; }

int f()
{
	int b = 0;
	a() + ++b;
	return 0;
}

int main()
{
	int b = 0;
	a() + ++b;
	printf("%d\n", b);
	return 0;
}
