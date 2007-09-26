
enum foo {aap};

enum foo eval_table(void);

static const struct ops {
        enum foo (*afrunc)(void);
} eval_ops[] = {
        { eval_table },
};


