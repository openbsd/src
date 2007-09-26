/* should not issue: init004.c, line 17: warning: illegal pointer combination */

struct ops;

typedef enum  { aap} Linetype;

typedef Linetype eval_fn(const struct ops *, int *, const char **);

static eval_fn eval_table, eval_unary;

static const struct ops {
        eval_fn *inner;
        struct op {
                const char *str;
                int (*fn)(int, int);
        } op[5];
} eval_ops[] = {
        { eval_table, { { "||", 0 } } },
};


