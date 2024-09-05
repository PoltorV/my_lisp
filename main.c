#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mpc.h"

// #define LASSERT(args, cond, err) \
//     if (!(cond)) { lval_delete(args); return lval_make_error(err); }

#define LASSERT(args, cond, err_format, ...) \
    if (!(cond)) { lval *err = lval_make_error(err_format, ##__VA_ARGS__); lval_delete(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. " \
    "Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
    LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. " \
    "Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);


static char buffer[2048];

typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lenv {
    lenv *par;

    int count;
    char **syms;
    lval **vals;
};

struct lval {
    int type;

    long num;
    char *err;
    char *sym;
    char *str;

    lbuiltin builtin;
    lenv *env;
    lval *formals;
    lval *body;

    int count;
    struct lval** cell;    
};

mpc_parser_t *Number;
mpc_parser_t *Symbol;
mpc_parser_t *String;
mpc_parser_t *Comment;
mpc_parser_t *S_expression;
mpc_parser_t *Q_expression;
mpc_parser_t *Expression;
mpc_parser_t *Lispy;

enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR, LVAL_BOOL, LVAL_STR};

lval *lval_make_num(long x);
lval *lval_make_error(char *format, ...);
lval *lval_make_sym(char *sym);
lval *lval_make_str(char *str);
lval *lval_make_s_expr();
lval *lval_make_q_expr();
lval *lval_make_fun(lbuiltin func);
lval *lval_make_lambda(lval *formals, lval *body);
lenv *lenv_make();
void lval_delete(lval *cur);
lval *lval_copy(lval *cur);
void lenv_delete(lenv *cur);
lval *lval_add(lval *x, lval *add);
lval *lenv_get(lenv *env, lval *cur);
void lenv_put(lenv *env, lval *cur_name, lval *cur_fun);

lval *lval_list_builtin(lenv *env, lval *cur);
lval *lval_eval_builtin(lenv *env, lval *cur);

lenv *lenv_copy(lenv *cur);
void lenv_def(lenv *env, lval *name, lval *fun);
lval *lval_copy(lval *cur);
lval *lval_pop(lval *cur, int ind);
lval *lval_take(lval *cur, int ind);
lval *lval_fun_builtin(lenv *env, lval *cur);
lval *lval_eval_s_expression(lenv *env, lval *cur);
lval *lval_eval(lenv *env, lval *cur);

void lval_print(lval *cur);

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

lval *lval_make_error(char *format, ...) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_ERR;
    ans->err = malloc(512);
    va_list v;
    va_start(v, format);
    vsnprintf(ans->err, 511, format, v);
    ans->err = realloc(ans->err, strlen(ans->err) + 1);
    va_end(v);
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

lval *lval_make_fun(lbuiltin func) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_FUN;
    ans->builtin = func;
    return ans;
}

lval *lval_make_lambda(lval *formals, lval *body) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_FUN;
    ans->builtin = NULL;
    ans->env = lenv_make();
    ans->formals = formals;
    ans->body = body;
    return ans;
}

lval *lval_make_bool(int val) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_BOOL;
    ans->num = val;
    return ans;
}

lval *lval_make_str(char *str) {
    lval *ans = malloc(sizeof(lval));
    ans->type = LVAL_STR;
    ans->str = malloc(strlen(str) + 1);
    strcpy(ans->str, str);
    return ans;
}

lenv *lenv_make() {
    lenv *ans = malloc(sizeof(lenv));
    ans->par = NULL;
    ans->count = 0;
    ans->syms = NULL;
    ans->vals = NULL;
    return ans;
}

void lval_delete(lval *cur) {
    switch (cur->type) {
        case LVAL_BOOL: break;
        case LVAL_NUM: break;
        case LVAL_SYM:
            free(cur->sym);
            break;
        case LVAL_FUN: 
            if (cur->builtin == NULL) {
                lenv_delete(cur->env);
                lval_delete(cur->formals);
                lval_delete(cur->body);
            }
            break;
        case LVAL_ERR:
            free(cur->err);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            cur->count = 0;
            for (int i = 0;i < cur->count;i++) free(cur->cell[i]);
            free(cur->cell);
            break;
        case LVAL_STR:
            free(cur->str);
            break;
        default:
            printf("ERROR HELP");
            break;
    }
    free(cur);
}

void lenv_delete(lenv *cur) {
    for (int i = 0;i < cur->count;i++) {
        free(cur->syms[i]);
        lval_delete(cur->vals[i]);
    }
    free(cur->syms);
    free(cur->vals);
    free(cur);
}

lval *lval_add(lval *x, lval *add) {
    //printf("hello");
    x->count++;
    x->cell = realloc(x->cell, sizeof(lval*) * x->count);
    x->cell[x->count - 1] = add;
    return x;
}

lval *lenv_get(lenv *env, lval *cur) {
    for (int i = 0;i < env->count;i++) {
        if (strcmp(cur->sym, env->syms[i]) == 0) {
            return lval_copy(env->vals[i]);
        }
    }
    if (env->par == NULL)
        return lval_make_error("unbound symbol '%s'", cur->sym);
    else
        return lenv_get(env->par, cur);
}

void lenv_put(lenv *env, lval *cur_name, lval *cur_fun) {
    for (int i = 0;i < env->count;i++) {
        if (strcmp(env->syms[i], cur_name->sym) == 0) {
            lval_delete(env->vals[i]);
            env->vals[i] = lval_copy(cur_fun);
        }
    }

    env->count++;
    env->syms = realloc(env->syms, env->count * sizeof(char*));
    env->vals = realloc(env->vals, env->count * sizeof(lval*));

    env->syms[env->count - 1] = malloc(strlen(cur_name->sym) + 1);
    strcpy(env->syms[env->count - 1], cur_name->sym);
    env->vals[env->count - 1] = lval_copy(cur_fun);
    //return env;///!!!!!!!!!!!
}

lval *lval_read_str(mpc_ast_t *cur) {
    cur->contents[strlen(cur->contents) - 1] = '\0';
    char *cur_str = malloc(strlen(cur->contents + 1) + 1);
    strcpy(cur_str, cur->contents + 1);

    cur_str = mpcf_unescape(cur_str);
    lval *ans = lval_make_str(cur_str);
    free(cur_str);
    return ans;
}

lval *lval_read(mpc_ast_t *cur) {
    if (strstr(cur->tag, "number")) {
        errno = 0;
        long cur_val = strtol(cur->contents, NULL, 10);
        return (errno == 0 ? lval_make_num(cur_val) : lval_make_error("Error: bad NUMBER %s", cur->contents));
    }
    if (strstr(cur->tag, "symbol")) {
        return lval_make_sym(cur->contents);
    }
    if (strstr(cur->tag, "string")) {
        return lval_read_str(cur);
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
        if (strstr(cur->children[i]->tag, "comment")) continue;
        x = lval_add(x, lval_read(cur->children[i]));
    }
    return x;
}

lval *lval_call(lenv *env, lval *fun, lval *a) {
    if (fun->builtin != NULL) return fun->builtin(env, a);

    // for (int i = 0;i < a->count;i++)
    //     lenv_put(fun->env, fun->formals->cell[i], a->cell[i]);
    
    // lval_delete(a);
    // fun->env->par = env;
    // return lval_eval_builtin(fun->env, lval_add(lval_make_s_expr(), fun->body));
    int arguments_count = a->count;
    int formals_count = fun->formals->count;
    while (a->count > 0) {
        if (fun->formals->count == 0) {
            lval_delete(a);
            return lval_make_error("Function passed too many arguments. Got %i, Expected %i.", arguments_count, formals_count);
        }
        
        lval *cur_formal = lval_pop(fun->formals, 0);
        if (strcmp(cur_formal->sym, "&") == 0) {
            if (fun->formals->count != 1) {
                lval_delete(a);
                return lval_make_error("Symbol '&' not followed by single symbol");
            }

            lval *list_name = lval_pop(fun->formals, 0);
            lenv_put(fun->env, list_name, lval_list_builtin(env, a));
            lval_delete(cur_formal);
            lval_delete(list_name);
            break;
        }
        lval *cur_val = lval_pop(a, 0);

        lenv_put(fun->env, cur_formal, cur_val);
        lval_delete(cur_formal);
        lval_delete(cur_val);
    }
    if (fun->formals->count > 0 && strcmp(fun->formals->cell[0]->sym, "&") == 0) {
        if (fun->formals->count != 2) {
            lval_delete(a);
            return lval_make_error("Symbol '&' not followed by single symbol");
        }
        lval_delete(lval_pop(fun->formals, 0));
        lval *list_name = lval_pop(fun->formals, 0);
        lval *val = lval_make_q_expr();
        lenv_put(fun->env, list_name, val);
        lval_delete(list_name);
        lval_delete(val);
    }
    lval_delete(a);
    if (fun->formals->count == 0) {
        fun->env->par = env;
        return lval_eval_builtin(fun->env, lval_add(lval_make_s_expr(), lval_copy(fun->body)));
    }
    else
        return lval_copy(fun);
}


void lval_print_expr(lval *cur, char *open, char *close) {
    // if (cur->count == 0) return;
    printf("%s", open);
    //printf("here");
    for (int i = 0;i < cur->count;i++) {
        lval_print(cur->cell[i]);
        if (i < cur->count - 1) printf(" ");
    }
    printf("%s", close);
}

void lval_print_str(lval *cur) {
    char *esc = malloc(strlen(cur->str) + 1);
    strcpy(esc, cur->str);
    esc = mpcf_escape(esc);
    printf("\"%s\"", esc);
    free(esc);
}

void lval_print(lval *cur) {
    // printf("print type %s\n", ltype_name(cur->type));
    switch (cur->type) {
        case LVAL_BOOL:
        case LVAL_NUM:
            printf("%ld", cur->num);
            break;
        case LVAL_SYM:
            printf("%s", cur->sym);
            break;
        case LVAL_FUN:
            if (cur->builtin != NULL)
                printf("<builtin>");
            else {
                printf("(\\ "); lval_print(cur->formals);
                printf(" ");   lval_print(cur->body);
                printf(")");
            }
            break;
        case LVAL_ERR:
            printf("ERROR:%s", cur->err);
            break;
        case LVAL_SEXPR:
            lval_print_expr(cur, "(", ")");
            break;
        case LVAL_QEXPR:
            lval_print_expr(cur, "{", "}");
            break;
        case LVAL_STR:
            lval_print_str(cur);
            break;
    }
}

lenv *lenv_copy(lenv *cur) {
    lenv *ans = malloc(sizeof(lenv));
    ans->par = cur->par;
    ans->count = cur->count;
    ans->syms = malloc(sizeof(char*) * ans->count);
    ans->vals = malloc(sizeof(lval*) * ans->count);
    for (int i = 0;i < cur->count;i++) {
        ans->syms[i] = malloc(strlen(cur->syms[i]) + 1);
        strcpy(ans->syms[i], cur->syms[i]);

        ans->vals[i] = lval_copy(cur->vals[i]);
    }
    return ans;
}

void lenv_def(lenv *env, lval *name, lval *fun) {
    while (env->par != NULL)
        env = env->par;
    //return lenv_put(env, name, fun);
    lenv_put(env, name, fun);
}

lval *lval_copy(lval *cur) {
    lval *ans = malloc(sizeof(lval));
    ans->type = cur->type;
    switch (cur->type)
    {
        case LVAL_BOOL:
        case LVAL_NUM:
            ans->num = cur->num;
            break;
        case LVAL_SYM:
            ans->sym = malloc(strlen(cur->sym) + 1);
            strcpy(ans->sym, cur->sym);
            break;
        case LVAL_FUN:
            if (cur->builtin != NULL)
                ans->builtin = cur->builtin;
            else {
                ans->builtin = NULL;
                ans->env = lenv_copy(cur->env);
                ans->formals = lval_copy(cur->formals);
                ans->body = lval_copy(cur->body);
            }
            break;
        case LVAL_ERR:
            ans->err = malloc(strlen(cur->err) + 1);
            strcpy(ans->err, cur->err);
            break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            ans->count = cur->count;
            ans->cell = malloc(sizeof(lval*) * ans->count);
            for (int i = 0;i < ans->count;i++) ans->cell[i] = lval_copy(cur->cell[i]);
            break;
        case LVAL_STR:
            ans->str = malloc(strlen(cur->str) + 1);
            strcpy(ans->str, cur->str);
            break;
        default:
            break;
    }
    return ans;
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
    return lval_make_error("ERROR: INVALID OPERATOR %s", op);
}

lval *lval_op_builtin(lenv *env, lval *cur, char *sym) {
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

char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        case LVAL_STR: return "String";
        default: return "Unknown";
    }
}

lval *lval_head_builtin(lenv *env, lval *cur) {
    LASSERT(cur, cur->count == 1, "ERROR: cant take head of many q-expressions. Got %i, Expected %i.", cur->count, 1)
    LASSERT(cur, cur->cell[0]->type == LVAL_QEXPR, "ERROR: cant take head of not q-expression. Got %s, Expected %s.", ltype_name(cur->cell[0]->type), ltype_name(LVAL_QEXPR))
    LASSERT(cur, cur->cell[0]->count != 0, "ERROR: size of q-expression is zero")

    lval *child = lval_take(cur, 0);
    while (child->count > 1) {
        lval_delete(lval_pop(child, 1));
    }
    return child;
}

lval *lval_tail_builtin(lenv *env, lval *cur) {
    LASSERT(cur, cur->count == 1, "ERROR: cant take head of many q-expressions")
    LASSERT(cur, cur->cell[0]->type == LVAL_QEXPR, "ERROR: cant take head of not q-expression")
    LASSERT(cur, cur->cell[0]->count != 0, "ERROR: size of q-expression is zero")

    lval *child = lval_take(cur, 0);
    lval_delete(lval_pop(child, 0));
    return child;
}

lval *lval_list_builtin(lenv *env, lval *cur) {
    LASSERT(cur, cur->count > 0, "ERROR: list size is zero")
    
    lval *ans = lval_make_q_expr();
    while (cur->count > 0) {
        lval_add(ans, lval_pop(cur, 0));
    }
    lval_delete(cur);
    return ans;
}

void lval_join_child(lval *cur, lval *child) {
    while (child->count > 0) {
        lval_add(cur, lval_pop(child, 0));
    }
    lval_delete(child);
}

lval *lval_join_builtin(lenv *env, lval *cur) {
    LASSERT(cur, cur->count > 0, "ERROR: cant join nothing")
    for (int i = 0;i < cur->count;i++)
        LASSERT(cur, cur->cell[i]->type == LVAL_QEXPR, "ERROR: cant join not Q-expression")
    lval *ans = lval_make_q_expr();
    while (cur->count > 0) {
        lval_join_child(ans, lval_pop(cur, 0));
    }
    lval_delete(cur);
    return ans;
}

lval *lval_eval_builtin(lenv *env, lval *cur) {
    LASSERT(cur, cur->count == 1, "ERROR: can eval only 1 Q-expression")
    LASSERT(cur, cur->cell[0]->type == LVAL_QEXPR, "ERROR: eval not Q-expression")
    lval *child = lval_take(cur, 0);
    child->type = LVAL_SEXPR;
    // lval_print(cur);
    return lval_eval(env, child);
}

lval *lval_lambda_builtin(lenv *env, lval *cur) {
    LASSERT_NUM("\\", cur, 2);
    LASSERT_TYPE("\\", cur, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", cur, 1, LVAL_QEXPR);

    for (int i = 0;i < cur->cell[0]->count;i++) {
        LASSERT(cur, (cur->cell[0]->cell[i]->type == LVAL_SYM),
        "Cannot define non-symbol. Got %s, Expected %s.",
        ltype_name(cur->cell[0]->cell[i]->type),ltype_name(LVAL_SYM));
    }

    lval *formals = lval_pop(cur, 0);
    lval *body = lval_pop(cur, 0);
    lval_delete(cur);

    return lval_make_lambda(formals, body);
}

int lval_check_is_builtin_function(char *s) {
    if (strcmp(s, "+") == 0) return 1;
    if (strcmp(s, "-") == 0) return 1;
    if (strcmp(s, "*") == 0) return 1;
    if (strcmp(s, "/") == 0) return 1;
    if (strcmp(s, "head") == 0) return 1;
    if (strcmp(s, "tail") == 0) return 1;
    if (strcmp(s, "list") == 0) return 1;
    if (strcmp(s, "join") == 0) return 1;
    if (strcmp(s, "def") == 0) return 1;
    if (strcmp(s, "eval") == 0) return 1;
    return 0;
}

lval *lval_var_builtin(lenv *env, lval *cur, char *func) {
    /// synax is different
    // example def {a b} {1 2}
    //not work properly because of eval not register functions in q-ecpression
    //if use this syntax thwn in in eval_s_expression cell[i] = eval(cell[i]) dont work tight because of {}
    //only copy letters but not make functions

    // LASSERT(cur, cur->count == 2, "ERROR: cant make a right variables")
    // LASSERT(cur, cur->cell[0]->count == cur->cell[1]->count, "ERROR: count of variables and defenitions is not equal")
    // for (int i = 0;i < cur->cell[0]->count;i++)
    //     LASSERT(cur, cur->cell[0]->cell[i]->type == LVAL_SYM, "ERROR: variable does not have right name")
    
    // for (int i = 0;i < cur->cell[0]->count;i++) {
    //     // printf("%s %ld\n", cur->cell[0]->cell[i]->sym, cur->cell[1]->cell[i]->num);
    //     if (lval_check_is_builtin_function(cur->cell[0]->cell[i]->sym) == 1) 
    //         return lval_make_error("Cant make redefenition of builtin function");
    //     if (strcmp("=", func) == 0)
    //         lenv_put(env, cur->cell[0]->cell[i], cur->cell[1]->cell[i]);
    //     if (strcmp("def", func) == 0)
    //         lenv_def(env, cur->cell[0]->cell[i], cur->cell[1]->cell[i]);
    // }
    // lval_delete(cur);
    // return lval_make_s_expr();

    LASSERT_TYPE(func, cur, 0, LVAL_QEXPR);
    
    lval* syms = cur->cell[0];
    for (int i = 0; i < syms->count; i++)
        LASSERT(cur, (syms->cell[i]->type == LVAL_SYM), "Function '%s' cannot define non-symbol. Got %s, Expected %s.", func,  ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    
    
    LASSERT(cur, (syms->count == cur->count-1), "Function '%s' passed too many arguments for symbols. Got %i, Expected %i.", func, syms->count, cur->count-1);
        
    for (int i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0) 
            lenv_def(env, syms->cell[i], cur->cell[i+1]);
        
        
        if (strcmp(func, "=") == 0)
            lenv_put(env, syms->cell[i], cur->cell[i+1]);
    }
    
    lval_delete(cur);
    return lval_make_s_expr();
}

lval *lval_builtin_add(lenv* env, lval* a) {
    return lval_op_builtin(env, a, "+");
}

lval *lval_builtin_sub(lenv* env, lval* a) {
    return lval_op_builtin(env, a, "-");
}

lval *lval_builtin_mul(lenv* env, lval* a) {
    return lval_op_builtin(env, a, "*");
}

lval *lval_builtin_div(lenv* env, lval* a) {
    return lval_op_builtin(env, a, "/");
}

lval *lval_def_builtin(lenv *env, lval *cur) {
    return lval_var_builtin(env, cur, "def");
}

lval *lval_put_builtin(lenv *env, lval *cur) {
    return lval_var_builtin(env, cur, "=");
}

lval *lval_comp_builtin(lval *cur, char *comp_fun) {
    LASSERT_NUM(comp_fun, cur, 2);
    LASSERT_TYPE(comp_fun, cur, 0, LVAL_NUM);
    LASSERT_TYPE(comp_fun, cur, 1, LVAL_NUM);
    lval *f = lval_pop(cur, 0);
    lval *s = lval_pop(cur, 0);
    lval *ans = NULL;
    if (strcmp("<", comp_fun) == 0)
        ans = lval_make_bool(f->num < s->num);
    if (strcmp("<=", comp_fun) == 0)
        ans = lval_make_bool(f->num <= s->num);
    if (strcmp(">", comp_fun) == 0)
        ans = lval_make_bool(f->num > s->num);
    if (strcmp(">=", comp_fun) == 0)
        ans = lval_make_bool(f->num >= s->num);
    if (strcmp("==", comp_fun) == 0)
        ans = lval_make_bool(f->num == s->num);

    lval_delete(f);
    lval_delete(s);
    lval_delete(cur);

    if (ans == NULL)
        return lval_make_error("invalid compare function");
    else
        return ans;
}

lval *lval_smaller_builtin(lenv *env, lval *cur) {
    return lval_comp_builtin(cur, "<");
}

lval *lval_smaller_or_equal_builtin(lenv *env, lval *cur) {
    return lval_comp_builtin(cur, "<=");
}

lval *lval_bigger_builtin(lenv *env, lval *cur) {
    return lval_comp_builtin(cur, ">");
}

lval *lval_bigger_or_equal_builtin(lenv *env, lval *cur) {
    return lval_comp_builtin(cur, ">=");
}

lval *lval_equal(lenv *env, lval *f, lval *s) {
    switch (f->type) {
        case (LVAL_BOOL):
        case (LVAL_NUM):
            return lval_make_bool(f->num == s->num);
        case (LVAL_ERR):
            return lval_make_bool(strcmp(f->err, s->err) == 0);
        case (LVAL_SYM):
            return lval_make_bool(strcmp(f->sym, s->sym) == 0);
        case (LVAL_FUN):
            lval *cur_ans;
            if (f->builtin != 0)
                cur_ans = lval_make_bool(f->builtin == s->builtin);
            else {
                lval *is_eq_formals = lval_equal(env, f->formals, s->formals);
                lval *is_eq_body = lval_equal(env, f->body, s->body);
                cur_ans = lval_make_bool(is_eq_body->num == 1 && is_eq_formals->num == 1);
                lval_delete(is_eq_body);
                lval_delete(is_eq_formals);
            }
            return cur_ans;
        case (LVAL_SEXPR):
        case (LVAL_QEXPR):
            if (f->count != s->count) 
                return lval_make_bool(0);
            for (int i = 0;i < f->count;i++) {
                lval *cur_eq = lval_equal(env, f->cell[i], s->cell[i]);
                if (cur_eq->type != LVAL_BOOL || cur_eq->num == 0)
                    return cur_eq;
                lval_delete(cur_eq);
            }
            return lval_make_bool(1);
        case (LVAL_STR):
            return lval_make_bool(strcmp(f->str, s->str) == 0);
    }
    return lval_make_error("error in function ==");
}

lval *lval_equal_builtin(lenv *env, lval *cur) {
    LASSERT_NUM("==", cur, 2);
    // LASSERT(cur, cur->cell[0]->type == cur->cell[1]->type,
    // "In function == values with different types: %s %s",
    // ltype_name(cur->cell[0]->type), ltype_name(cur->cell[1]->type));
    if (cur->cell[0]->type != cur->cell[1]->type)
        return lval_make_bool(0);
    lval *f = lval_pop(cur, 0);
    lval *s = lval_pop(cur, 0);
    lval *ans = lval_equal(env, f, s);
    lval_delete(f);
    lval_delete(s);
    lval_delete(cur);
    return ans;
}

lval *lval_not_equal_builtin(lenv *env, lval *cur) {
    lval *cur_ans = lval_equal_builtin(env, cur);
    if (cur_ans->type == LVAL_ERR)
        return cur_ans;
    else {
        lval *ans = lval_make_bool((cur_ans->num + 1) % 2);
        lval_delete(cur);
        lval_delete(cur_ans);
        return ans;
    }
}

void lenv_add_builtin_functions(lenv *env, char *name, lbuiltin func) {
    lval *cur_lval_name = lval_make_sym(name);
    lval *cur_lval_func = lval_make_fun(func);
    lenv_put(env, cur_lval_name, cur_lval_func);
    lval_delete(cur_lval_func);
    lval_delete(cur_lval_name);
}

lval *lval_fun_builtin(lenv *env, lval *cur) {
    LASSERT_NUM("fun", cur, 2);
    lval *formals = lval_pop(cur, 0);
    // if (formals->count == 0)
    lval *name = lval_pop(formals, 0);
    lval *body = lval_pop(cur, 0);

    lval *func = lval_make_lambda(formals, body);
    lenv_put(env, name, func);
    return lval_make_s_expr();
}

lval *lval_eval_comparison(lenv *env, lval *cur) {
    lval *cur_fun = lval_pop(cur, 0);
    return lval_call(env, cur_fun, cur);
}

lval *lval_if_builtin(lenv *env, lval *cur) {
    LASSERT_NUM("if", cur, 3)
    LASSERT_TYPE("if", cur, 0, LVAL_BOOL)
    LASSERT_TYPE("if", cur, 1, LVAL_QEXPR)
    LASSERT_TYPE("if", cur, 2, LVAL_QEXPR)

    lval *expr_res = lval_pop(cur, 0);
    lval *ans;

    cur->cell[0]->type = LVAL_SEXPR;
    cur->cell[1]->type = LVAL_SEXPR;
    if (expr_res->num == 1)
        ans = lval_eval(env, lval_pop(cur, 0));
    else 
        ans = lval_eval(env, lval_pop(cur, 1));
    
    lval_delete(expr_res);
    lval_delete(cur);
    return ans;
}

lval *lval_load_builtin(lenv *env, lval *cur) {
    LASSERT_NUM("load", cur, 1)
    LASSERT_TYPE("load", cur, 0, LVAL_STR);

    mpc_result_t res;
    if (mpc_parse_contents(cur->cell[0]->str, Lispy, &res)) {
        lval *expr = lval_read(res.output);
        while (expr->count) {
            lval *cur_expr = lval_pop(expr, 0);
            // lval_print(cur_expr);
            // printf("\n");
            lval *cur_ans = lval_eval(env, cur_expr);
            if (cur_ans->type == LVAL_ERR)
                lval_print(cur_ans);
            lval_delete(cur_ans);
        }

        lval_delete(cur);
        lval_delete(expr);
        return lval_make_s_expr();
    }
    else {
        mpc_err_print(res.error);
        mpc_err_delete(res.error);
        return lval_make_error("Error while loading file");
    }
}

lval *lval_print_builtin(lenv *env, lval *cur) {
    for (int i = 0;i < cur->count;i++) {
        lval_print(cur->cell[i]);
        printf(" ");
    }
    printf("\n");
    lval_delete(cur);
    return lval_make_s_expr();
}

lval *lval_error_builtin(lenv *env, lval *cur) {
    LASSERT_NUM("error", cur, 1)
    LASSERT_TYPE("error", cur, 0, LVAL_STR)
    lval *error = lval_make_error(cur->cell[0]->str);
    lval_delete(cur);
    return error;
}

void lenv_add_functions(lenv *env) {
    lenv_add_builtin_functions(env, "+", lval_builtin_add);
    lenv_add_builtin_functions(env, "-", lval_builtin_sub);
    lenv_add_builtin_functions(env, "*", lval_builtin_mul);
    lenv_add_builtin_functions(env, "/", lval_builtin_div);
    lenv_add_builtin_functions(env, "head", lval_head_builtin);
    lenv_add_builtin_functions(env, "tail", lval_tail_builtin);
    lenv_add_builtin_functions(env, "join", lval_join_builtin);
    lenv_add_builtin_functions(env, "list", lval_list_builtin);
    lenv_add_builtin_functions(env, "eval", lval_eval_builtin);
    lenv_add_builtin_functions(env, "\\", lval_lambda_builtin);
    lenv_add_builtin_functions(env, "def", lval_def_builtin);
    lenv_add_builtin_functions(env, "=", lval_put_builtin);
    lenv_add_builtin_functions(env, "fun", lval_fun_builtin);
    lenv_add_builtin_functions(env, "<", lval_smaller_builtin);
    lenv_add_builtin_functions(env, "<=", lval_smaller_or_equal_builtin);
    lenv_add_builtin_functions(env, ">", lval_bigger_builtin);
    lenv_add_builtin_functions(env, ">=", lval_bigger_or_equal_builtin);
    lenv_add_builtin_functions(env, "==", lval_equal_builtin);
    lenv_add_builtin_functions(env, "!=", lval_not_equal_builtin);
    lenv_add_builtin_functions(env, "if", lval_if_builtin);
    lenv_add_builtin_functions(env, "load", lval_load_builtin);
    lenv_add_builtin_functions(env, "print", lval_print_builtin);
    lenv_add_builtin_functions(env, "error", lval_error_builtin);
}

lval *lval_eval_s_expression(lenv *env, lval *cur) {
    for (int i = 0;i < cur->count;i++) cur->cell[i] = lval_eval(env, cur->cell[i]);

    for (int i = 0;i < cur->count;i++) 
        if (cur->cell[i]->type == LVAL_ERR) 
            return lval_take(cur, i);

    if (cur->count == 0) return cur;
    if (cur->count == 1) return lval_eval(env, lval_take(cur, 0));

    lval *f = lval_pop(cur, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_make_error(
        "S-Expression starts with incorrect type. "
        "Got %s, Expected %s.\n",
        ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_print(f);
        lval_delete(f); lval_delete(cur);
        return err;
    }
    lval *ans = lval_call(env, f, cur);
    lval_delete(f);
    return ans;
}

lval *lval_eval(lenv *env, lval *cur) {
    if (cur->type == LVAL_SYM) {
        lval *x = lenv_get(env, cur);
        lval_delete(cur);
        // lval_print(x);
        return x;
    }
    if (cur->type == LVAL_SEXPR) return lval_eval_s_expression(env, cur);
    return cur;
}

int main(int argc, char *argv[]) {
    // printf("\\\n");
    Number = mpc_new("number");
    Symbol = mpc_new("symbol");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    S_expression = mpc_new("s_expression");
    Q_expression = mpc_new("q_expression");
    Expression = mpc_new("expression");
    Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
    " number : /-?[0-9]+/; "
    " symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; "
    " string : /\"(\\\\.|[^\"])*\"/ ; "
    " comment : /;[^\\r\\n]*/ ;"
    " s_expression : '(' <expression>* ')' ; "
    " q_expression : '{' <expression>* '}' ; "
    " expression : <number> | <symbol> | <string> | <comment> | <s_expression> | <q_expression> ;"
    " lispy : /^/ <expression>* /$/; ",
    Number, Symbol, String, Comment, S_expression, Q_expression, Expression, Lispy, NULL
    );

    lenv *env = lenv_make();
    lenv_add_functions(env);

    if (argc >= 2) {
        for (int i = 1;i < argc;i++) {
            lval *cur = lval_add(lval_make_s_expr(), lval_make_str(argv[i]));
            lval *cur_ans = lval_load_builtin(env, cur);
            lval_print(cur_ans);
            
            lval_delete(cur_ans);
        }
    }

    while (1) {
        char *input = readline("lisp >");
        //add_history(input);
        mpc_result_t r;
        
        if (mpc_parse("input", input, Lispy, &r)) {
            lval *ans = lval_read(r.output); 
            // lval_print(ans);
            lval_print(lval_eval(env, ans));
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
    
    mpc_cleanup(8, Number, Symbol, String, Comment, S_expression, Q_expression, Expression, Lispy);
    return 0;
}
