void	funmap_init(void);
PF	name_function(char *);
char	*function_name(PF);
LIST	*complete_function_list(char *, int);
int	funmap_add(PF, char *);
