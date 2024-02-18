#ifndef _VALUE_H
#define _VALUE_H

#include <stdlib.h>
#include "mpc.h"

typedef double ValueNumber;

typedef enum {
	ERROR_TYPE,
	FUNCTION_TYPE,
	NUMBER_TYPE,
	SEXPRESSION_TYPE,
	STRING_TYPE,
	QEXPRESSION_TYPE,
	SYMBOL_TYPE,
} ValueType;

typedef struct Env Env;
typedef struct Value Value;
typedef Value *(*ValueBuiltin)(Value *, Env *);

struct Value {
	ValueType type;

	/* Basic */
	char *error;
	ValueNumber number;
	char *string;
	char *symbol;

	/* Functions */
	Env *env;
	Value *lambda_formals;
	Value *lambda_body;
	ValueBuiltin builtin;

	/* Expressions */
	size_t children_count;
	Value **children;
};

Value *value_copy(const Value *);
Value *value_eval(Value *, Env *);
void value_free(Value *);
void value_println(const Value *);
Value *value_read(const mpc_ast_t *);

void value_add_child(Value *, Value *);

Value *value_error_alloc(char *, ...);
Value *value_expression_alloc(ValueType);
Value *value_builtin_alloc(ValueBuiltin);
Value *value_string_alloc(const char *);
Value *value_symbol_alloc(const char *);

Value *value_symbol_add_eval(Value *, Env *);
Value *value_symbol_and_eval(Value *, Env *);
Value *value_symbol_def_eval(Value *, Env *);
Value *value_symbol_divide_eval(Value *, Env *);
Value *value_symbol_error_eval(Value *, Env *);
Value *value_symbol_eval_eval(Value *, Env *);
Value *value_symbol_eq_eval(Value *, Env *);
Value *value_symbol_ge_eval(Value *, Env *);
Value *value_symbol_gt_eval(Value *, Env *);
Value *value_symbol_head_eval(Value *, Env *);
Value *value_symbol_if_eval(Value *, Env *);
Value *value_symbol_input_eval(Value *, Env *);
Value *value_symbol_join_eval(Value *, Env *);
Value *value_symbol_lambda_eval(Value *, Env *);
Value *value_symbol_le_eval(Value *, Env *);
Value *value_symbol_list_eval(Value *, Env *);
Value *value_symbol_load_eval(Value *, Env *);
Value *value_symbol_lt_eval(Value *, Env *);
Value *value_symbol_multiply_eval(Value *, Env *);
Value *value_symbol_ne_eval(Value *, Env *);
Value *value_symbol_not_eval(Value *, Env *);
Value *value_symbol_or_eval(Value *, Env *);
Value *value_symbol_print_eval(Value *, Env *);
Value *value_symbol_set_eval(Value *, Env *);
Value *value_symbol_substract_eval(Value *, Env *);
Value *value_symbol_tail_eval(Value *, Env *);
Value *value_symbol_while_eval(Value *, Env *);

/* `grammar.h` globals */
extern mpc_parser_t *Program;

#endif /* _VALUE_H */
