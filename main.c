#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mpc.h"

static char buffer[2048];

typedef struct lval {
    int type;
    long num;

    char *err;
    char *sym;

    int count;
    struct lval** cell;    
} lval;

enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR};
enum {LERR_DIV_ZERO, LERR_BAD_OPERATOR, LERR_BAD_NUM};

char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    size_t buf_len = strlen(buffer) + 1;
    char *cpy = malloc(buf_len * sizeof(char));
    strncpy(cpy, buffer, strlen(buffer));
    cpy[buf_len - 1] = '\0';
    return cpy;
}

void add_history(){}

lval *lval_make_num(long x) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_NUM;
    ans->num = x;
    return ans;
}

lval *lval_make_error(char *message) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_ERR;
    ans->err = malloc(strlen(message) + 1);
    strcpy(ans->err, message);
    return ans;
}

lval *lval_make_sym(char *sym) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_SYM;
    ans->sym = malloc(strlen(sym) + 1);
    strcpy(ans->sym, sym);
    return ans;
}

lval *lval_make_s_expr() {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_SEXPR;
    ans->count = 0;
    ans->cell = NULL;
    return ans;
}

lval *lval_make_q_expr() {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_QEXPR;
    ans->count = 0;
    ans->cell = NULL;
    return ans;
}

void lval_delete(lval *cur) {
    switch (cur->type) {
        case LVAL_NUM: break;
        case LVAL_SYM:
            free(cur->sym);
            break;
        case LVAL_ERR:
            free(cur->err);
            break;

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0;i < cur->count;i++) free(cur->cell[i]);
            free(cur->cell);
            break;
        default:
            break;
    }
    free(cur);
}

lval *lval_add(lval *x, lval *add) {
    //printf("hello");
    x->count++;
    x->cell = realloc(x->cell, sizeof(lval*) * x->count);
    x->cell[x->count - 1] = add;
    return x;
}

lval *lval_read(mpc_ast_t *cur) {
    if (strstr(cur->tag, "number")) {
        errno = 0;
        long cur_val = strtol(cur->contents, NULL, 10);
        return (errno == 0 ? lval_make_num(cur_val) : lval_make_error("Error: bad NUMBER"));
    }
    if (strstr(cur->tag, "symbol")) {
        return lval_make_sym(cur->contents);
    }
    lval *x = NULL;
    if (strcmp(cur->tag, ">") == 0) { x = lval_make_s_expr(); }
    if (strstr(cur->tag, "s_expression")) { x = lval_make_s_expr(); }
    if (strstr(cur->tag, "q_expression")) { x = lval_make_q_expr(); }

    for (int i = 0;i < cur->children_num;i++) {
        if (strcmp(cur->children[i]->contents, "(") == 0) continue;
        if (strcmp(cur->children[i]->contents, ")") == 0) continue;
        if (strcmp(cur->children[i]->contents, "{") == 0) continue;
        if (strcmp(cur->children[i]->contents, "}") == 0) continue;
        if (strcmp(cur->children[i]->tag, "regex") == 0) continue;
        x = lval_add(x, lval_read(cur->children[i]));
    }
    return x;
}

void lval_print(lval *cur);

void lval_print_expr(lval *cur, char *open, char *close) {
    printf("%s", open);
    for (int i = 0;i < cur->count;i++) {
        lval_print(cur->cell[i]);
        if (i < cur->count - 1) printf(" ");
    }
    printf("%s", close);
}

void lval_print(lval *cur) {
    switch (cur->type) {
        case LVAL_NUM:
            printf("%ld", cur->num);
            break;
        case LVAL_SYM:
            printf("%s", cur->sym);
            break;
        case LVAL_ERR:
            printf("%s", cur->err);
            break;
        case LVAL_SEXPR:
            lval_print_expr(cur, "(", ")");
            break;
        case LVAL_QEXPR:
            lval_print_expr(cur, "{", "}");
            break;
    }
}

lval *lval_pop(lval *cur, int ind) {
    lval *ans = cur->cell[ind];
    memmove(&cur->cell[ind], &cur->cell[ind + 1], sizeof(lval*) * (cur->count - ind - 1));
    cur->count--;

    cur->cell = realloc(cur->cell, sizeof(lval*) * cur->count);
    return ans;
}

lval *lval_take(lval *cur, int ind) {
    lval *ans = lval_pop(cur, ind);
    lval_delete(cur);
    return ans;
}

lval *lval_eval_op(lval *f, lval *s, char *op) {
    if (strcmp(op, "+") == 0) return lval_make_num(f->num + s->num);
    if (strcmp(op, "-") == 0) return lval_make_num(f->num - s->num);
    if (strcmp(op, "*") == 0) return lval_make_num(f->num * s->num);
    if (strcmp(op, "/") == 0) 
        return (s->num != 0 ? lval_make_num(f->num / s->num) : lval_make_error("ERROR: DIVISION by ZERO"));
    return lval_make_error("ERROR: INVALID OPERATOR");
}

lval *lval_eval_builtin(lval *cur, char *sym) {
    for (int i = 0;i < cur->count;i++) {
        if (cur->cell[i]->type != LVAL_NUM) {
            lval_delete(cur);
            return lval_make_error("ERROR: INVALID NUMBER");
        }
    }

    lval *first = lval_pop(cur, 0);
    if (cur->count == 0 && strcmp(sym, "-") == 0) 
        first->num *= -1;

    if (cur->count == 0) {
        lval_delete(cur);
        return first;
    }
    
    while (cur->count > 0) {
        lval *current_el = lval_pop(cur, 0);
        first = lval_eval_op(first, current_el, sym);
        lval_delete(current_el);
    }
    lval_delete(cur);
    return first;
}


lval *lval_eval(lval *cur);

lval *lval_eval_s_expression(lval *cur) {
    for (int i = 0;i < cur->count;i++) cur->cell[i] = lval_eval(cur->cell[i]);

    for (int i = 0;i < cur->count;i++) 
        if (cur->cell[i]->type == LVAL_ERR) 
            return lval_take(cur, i);

    if (cur->count == 0) return cur;
    if (cur->count == 1) return lval_take(cur, 0);

    lval *f = lval_pop(cur, 0);
    if (f->type != LVAL_SYM) return lval_make_error("ERROR: S-expression doesnt start with a symbol");

    lval *ans = lval_eval_builtin(cur, f->sym);
    lval_delete(f);
    return ans;
}

lval *lval_eval(lval *cur) {
    if (cur->type != LVAL_SEXPR) return cur;
    else return lval_eval_s_expression(cur);
}

// int main(int argc, char *argv[]) {
int main(void) {
    // printf("%s", input);
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *S_expression = mpc_new("s_expression");
    mpc_parser_t *Q_expression = mpc_new("q_expression");
    mpc_parser_t *Expression = mpc_new("expression");
    mpc_parser_t *Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
    " number : /-?[0-9]+/; "
    " symbol : '+' | '-' | '*' | '/' | \"head\" | \"tail\"; "
    " s_expression : '(' <expression>* ')' ; "
    " q_expression : '{' <expression>* '}' ; "
    " expression : <number> | <symbol> | <s_expression> | <q_expression> ;"
    " lispy : /^/ <expression>* /$/; ",
    Number, Symbol, S_expression, Q_expression, Expression, Lispy, NULL
    );

    while (1) {
        char *input = readline("lisp >");
        //add_history(input);
        mpc_result_t r;
        
        if (mpc_parse("input", input, Lispy, &r)) {
            lval *ans = lval_read(r.output); 
            // lval_print(ans);
            lval_print(lval_eval(ans));
            printf("\n");
            // mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        }
        else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    
    mpc_cleanup(6, Number, Symbol, S_expression, Q_expression, Expression, Lispy);
    return 0;
}
