#ifndef SIMPLE_EXEC_H
#define SIMPLE_EXEC_H

int simple_execvp(const char *file, char *const args[]);
int simple_execlp(const char *file, ...);

#endif
