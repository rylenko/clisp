#include <stdio.h>
#include "config.h"
#include "env.h"
#include "utils.h"
#include "value.h"

/*
Because we might use args in the construction of the error message we
need to make sure we don't delete it until we've created the error value.
*/
#define ERROR_SYMBOL_ARGS(args, fmt, ...) { \
	Value *error = value_error_alloc(fmt, ##__VA_ARGS__); \
	value_free(args); \
	return error; \
}
#define VALIDATE_SYMBOL_ARGS(args, condition, fmt, ...) { \
	if (!(condition)) { \
		ERROR_SYMBOL_ARGS(args, fmt, ##__VA_ARGS__); \
	} \
}
#define VALIDATE_SYMBOL_ARGS_COUNT(symbol, args, expected) { \
	VALIDATE_SYMBOL_ARGS( \
		args, \
		args->children_count == expected, \
		"%s: Too many arguments. Expected %zu. Got %zu.", \
		symbol, \
		expected, \
		args->children_count \
	); \
}
#define VALIDATE_SYMBOL_ARG_TYPE(symbol, args, index, expected) { \
	VALIDATE_SYMBOL_ARGS( \
		args, \
		args->children[index]->type == expected, \
		"%s: Invalid %zu argument type. Expected %s. Got %s.", \
		symbol, \
		index, \
		value_type_names[expected], \
		value_type_names[args->children[index]->type] \
	); \
}

/* Entire `Value` */
static void value_print(const Value *value);
static unsigned char value_eq(const Value *x, const Value *y);

/* `Value`'s childs. Usable for expressions */
static void value_extend_children(Value *to, Value *from);
static void value_extend_string(Value *to, Value *from);
static Value *value_pop_child(Value *value, size_t child_i);
static Value *value_free_without_child(Value *value, size_t child_i);

/* Expressions */
static void value_expression_print(const Value *value);

/* Sexpressions */
static Value *value_sexpression_eval(Value *value, Env *env);

/* Functions */
static Value *value_function_call(Value *f, Env *env, Value *args);
static void value_function_print(const Value *value);
static Value *value_lambda_alloc(Value *args, Value *body);

/* Strings */
static void value_string_print(const Value *value);
static Value *value_string_read(const mpc_ast_t *ast);

/* Symbol */
static Value *value_symbol_arithmetic_eval(
	const char *symbol,
	Value *value,
	const Env *env
);
static Value *value_symbol_cmp_eval(
	const char *symbol,
	Value *value,
	const Env *env
);
static Value *value_symbol_condition_chain_eval(
	const char *symbol,
	Value *value,
	const Env *env
);
static Value *value_symbol_ordering_eval(
	const char *symbol,
	Value *value,
	const Env *env
);
static Value *value_symbol_variable_eval(
	const char *symbol,
	Value *value,
	Env *env
);

/* Number */
static Value *value_number_alloc(ValueNumber number);
static Value *value_number_read(const mpc_ast_t *ast);

const char *value_type_names[] = {
	[ERROR_TYPE] = "Error",
	[FUNCTION_TYPE] = "Function",
	[NUMBER_TYPE] = "Number",
	[QEXPRESSION_TYPE] = "Qexpression",
	[SEXPRESSION_TYPE] = "Sexpression",
	[STRING_TYPE] = "String",
	[SYMBOL_TYPE] = "Symbol",
};

void
value_add_child(Value *value, Value *child)
{
	/* Reallocate memory with new size and add child to end */
	++value->children_count;
	value->children = realloc(
		value->children,
		sizeof(Value*) * value->children_count
	);
	value->children[value->children_count - 1] = child;
}

Value*
value_copy(const Value *value)
{
	size_t i;

	/* Alocate new value and set type to it */
	Value *new_value = malloc(sizeof(Value));
	new_value->type = value->type;

	switch (value->type) {
	case ERROR_TYPE:
		/* Copy error message */
		new_value->error = strdup(value->error);
		break;
	case FUNCTION_TYPE:
		if (value->builtin) {
			/* Copy builtin's pointer */
			new_value->builtin = value->builtin;
		} else {
			/* Copy lambda */
			new_value->env = env_copy(value->env);
			new_value->lambda_formals = value_copy(value->lambda_formals);
			new_value->lambda_body = value_copy(value->lambda_body);
			new_value->builtin = NULL;
		}
		break;
	case NUMBER_TYPE:
		new_value->number = value->number;
		break;
	case SEXPRESSION_TYPE: /* FALLTHROUGH */
	case QEXPRESSION_TYPE:
		/* Copy children */
		new_value->children_count = value->children_count;
		new_value->children = malloc(sizeof(Value *) * value->children_count);
		for (i = 0; i < value->children_count; ++i)
			new_value->children[i] = value_copy(value->children[i]);
		break;
	case STRING_TYPE:
		/* Copy string */
		new_value->string = strdup(value->string);
		break;
	case SYMBOL_TYPE:
		/* Copy symbol */
		new_value->symbol = strdup(value->symbol);
		break;
	}

	return new_value;
}

Value*
value_error_alloc(char *fmt, ...)
{
	Value *value = malloc(sizeof(Value));
	va_list va;
	va_start(va, fmt);

	/* Allocate error value */
	value->error = malloc(ERROR_BUFFER_SIZE);
	value->type = ERROR_TYPE;

	/* Format error message and fit it in memory */
	vsnprintf(value->error, ERROR_BUFFER_SIZE - 1, fmt, va);
	value->error = realloc(value->error, strlen(value->error) + 1);

	va_end(va);
	return value;
}

Value*
value_eval(Value *value, Env *env)
{
	Value *symbol_value;

	switch (value->type) {
	case SYMBOL_TYPE:
		/* Get value from env and return it */
		symbol_value = env_get(env, value);
		value_free(value);
		return symbol_value;
	case SEXPRESSION_TYPE:
		return value_sexpression_eval(value, env);
	default:
		return value;
	}
}

Value*
value_expression_alloc(ValueType type)
{
	Value *value = malloc(sizeof(Value));
	value->type = type;
	value->children_count = 0;
	value->children = NULL;
	return value;
}

void
value_free(Value *value)
{
	size_t i;

	if (value->type == SEXPRESSION_TYPE || value->type == QEXPRESSION_TYPE) {
		/* Free children */
		for (i = 0; i < value->children_count; ++i)
			value_free(value->children[i]);
	} else if (value->type == ERROR_TYPE) {
		/* Free allocated error message */
		free(value->error);
	} else if (value->type == FUNCTION_TYPE && !value->builtin) {
		/* Free lambda */
		env_free(value->env);
		value_free(value->lambda_formals);
		value_free(value->lambda_body);
	} else if (value->type == STRING_TYPE) {
		/* Free allocated string */
		free(value->string);
	} else if (value->type == SYMBOL_TYPE) {
		/* Free allocated symbol */
		free(value->symbol);
	}
	free(value);
}

Value*
value_builtin_alloc(ValueBuiltin builtin)
{
	Value *value = malloc(sizeof(Value));
	value->type = FUNCTION_TYPE;
	value->builtin = builtin;
	return value;
}

void
value_println(const Value *value)
{
	value_print(value);
	putchar('\n');
}

Value*
value_read(const mpc_ast_t *ast)
{
	size_t i;
	Value *value = NULL;

	if (strstr(ast->tag, "Number"))
		return value_number_read(ast);
	else if (strstr(ast->tag, "Symbol"))
		return value_symbol_alloc(ast->contents);
	else if (strstr(ast->tag, "String"))
		return value_string_read(ast);

	if (strcmp(ast->tag, ">") == 0 || strstr(ast->tag, "Sexpression"))
		value = value_expression_alloc(SEXPRESSION_TYPE);
	else if (strstr(ast->tag, "Qexpression"))
		value = value_expression_alloc(QEXPRESSION_TYPE);

	for (i = 0; i < (size_t)ast->children_num; ++i) {
		/* Escape brackets or "regex" mark */
		if (
			strcmp(ast->children[i]->contents, "(") == 0
			|| strcmp(ast->children[i]->contents, ")") == 0
			|| strcmp(ast->children[i]->contents, "{") == 0
			|| strcmp(ast->children[i]->contents, "}") == 0
			|| strcmp(ast->children[i]->contents, ";") == 0
			|| strstr(ast->children[i]->tag, "Comment")
			|| strcmp(ast->children[i]->tag, "regex") == 0
		)
			continue;

		/* Read and add child to expression */
		value_add_child(value, value_read(ast->children[i]));
	}

	return value;
}

Value*
value_string_alloc(const char *s)
{
	Value *rv = malloc(sizeof(Value));
	rv->type = STRING_TYPE;
	rv->string = strdup(s);
	return rv;
}

Value*
value_symbol_add_eval(Value *value, Env *env)
{
	return value_symbol_arithmetic_eval("+", value, env);
}

Value*
value_symbol_alloc(const char *symbol)
{
	Value *value = malloc(sizeof(Value));
	value->type = SYMBOL_TYPE;
	value->symbol = strdup(symbol);
	return value;
}

Value*
value_symbol_and_eval(Value *value, Env *env)
{
	return value_symbol_condition_chain_eval("&&", value, env);
}

Value*
value_symbol_def_eval(Value *value, Env *env)
{
	return value_symbol_variable_eval("def", value, env);
}

Value*
value_symbol_divide_eval(Value *value, Env *env)
{
	return value_symbol_arithmetic_eval("/", value, env);
}

Value*
value_symbol_error_eval(Value *value, Env *env)
{
	(void)env;
	VALIDATE_SYMBOL_ARGS_COUNT("error", value, 1);
	VALIDATE_SYMBOL_ARG_TYPE("error", value, 0, STRING_TYPE);
	ERROR_SYMBOL_ARGS(value, value->children[0]->string);
}

Value*
value_symbol_eval_eval(Value *value, Env *env)
{
	Value *arg;

	VALIDATE_SYMBOL_ARGS_COUNT("eval", value, 1);
	VALIDATE_SYMBOL_ARG_TYPE("eval", value, 0, QEXPRESSION_TYPE);

	/* Set sexpression type to argument and eval it */
	arg = value_free_without_child(value, 0);
	arg->type = SEXPRESSION_TYPE;
	return value_eval(arg, env);
}

Value*
value_symbol_eq_eval(Value *value, Env *env)
{
	return value_symbol_cmp_eval("==", value, env);
}

Value*
value_symbol_ge_eval(Value *value, Env *env)
{
	return value_symbol_ordering_eval(">=", value, env);
}

Value*
value_symbol_gt_eval(Value *value, Env *env)
{
	return value_symbol_ordering_eval(">", value, env);
}

Value*
value_symbol_head_eval(Value *value, Env *env)
{
	(void)env;

	Value *arg,
		*new_value;

	VALIDATE_SYMBOL_ARGS_COUNT("head", value, 1);

	arg = value_free_without_child(value, 0);

	if (arg->type == QEXPRESSION_TYPE) {
		/* Validate a size */
		VALIDATE_SYMBOL_ARGS(
			value,
			arg->children_count != 0,
			"head: Argument is empty."
		);

		/* Put first argument's child to new allocated qexpression */
		new_value = value_expression_alloc(QEXPRESSION_TYPE);
		value_add_child(new_value, value_free_without_child(arg, 0));
	} else if (arg->type == STRING_TYPE) {
		/* Validate a size */
		VALIDATE_SYMBOL_ARGS(
			value,
			strlen(arg->string) != 0,
			"head: Argument is empty."
		);

		/* Extract first char */
		arg->string[1] = '\0';
		new_value = value_copy(arg);
		value_free(arg);
	} else {
		ERROR_SYMBOL_ARGS(
			arg,
			"head: Invalid arg type. Expected %s or %s. Got %s.",
			value_type_names[QEXPRESSION_TYPE],
			value_type_names[STRING_TYPE],
			value_type_names[arg->type]
		)
	}

	return new_value;
}

Value*
value_symbol_if_eval(Value *value, Env *env)
{
	Value *result;

	VALIDATE_SYMBOL_ARGS_COUNT("if", value, 3);
	VALIDATE_SYMBOL_ARG_TYPE("if", value, 0, NUMBER_TYPE);
	VALIDATE_SYMBOL_ARG_TYPE("if", value, 1, QEXPRESSION_TYPE);
	VALIDATE_SYMBOL_ARG_TYPE("if", value, 1, QEXPRESSION_TYPE);

	value->children[1]->type = SEXPRESSION_TYPE;
	value->children[2]->type = SEXPRESSION_TYPE;

	if (value->children[0]->number)
		result = value_eval(value_pop_child(value, 1), env);
	else
		result = value_eval(value_pop_child(value, 2), env);

	value_free(value);
	return result;
}

Value*
value_symbol_input_eval(Value *value, Env *env)
{
	(void)env;

	size_t length;
	char *buffer;

	VALIDATE_SYMBOL_ARGS_COUNT("input", value, 2);
	VALIDATE_SYMBOL_ARG_TYPE("input", value, 0, STRING_TYPE);
	VALIDATE_SYMBOL_ARG_TYPE("input", value, 1, NUMBER_TYPE);
	VALIDATE_SYMBOL_ARGS(
		value,
		value->children[1]->number >= 1,
		"input: Length must be >= 1. Got %f.",
		value->children[1]->number
	);

	/* Allocate the buffer. +2 for '\n' and '\0' */
	length = (size_t)value->children[1]->number;
	buffer = malloc(length + 2);

	/* Print a prompt */
	printf("%s", value->children[0]->string);
	fflush(stdout);

	/* Read an input. +1 for '\n' */
	if (!fgets(buffer, length + 1, stdin)) {
		ERROR_SYMBOL_ARGS(value, "Failed to input.");
	}
	fflush(stdin);

	/* Free args */
	value_free(value);

	/* Remove '\n' and allocate new string */
	buffer[strcspn(buffer, "\n")] = '\0';
	return value_string_alloc(buffer);
}

Value*
value_symbol_join_eval(Value *value, Env *env)
{
	(void)env;

	size_t i;
	Value *left;

	VALIDATE_SYMBOL_ARGS(
		value,
		value->children_count >= 2,
		"join: Invalid args count. Expected at least 2. Got %zu.",
		value->children_count
	);
	if (value->children[0]->type == QEXPRESSION_TYPE) {
		for (i = 0; i < value->children_count; ++i)
			VALIDATE_SYMBOL_ARG_TYPE("join", value, i, QEXPRESSION_TYPE);

		/* Extend first child with other children */
		left = value_pop_child(value, 0);
		while (value->children_count > 0)
			value_extend_children(left, value_pop_child(value, 0));
	} else {
		for (i = 0; i < value->children_count; ++i)
			VALIDATE_SYMBOL_ARG_TYPE("join", value, i, STRING_TYPE);

		/* Extend first child with other strings */
		left = value_pop_child(value, 0);
		while (value->children_count > 0)
			value_extend_string(left, value_pop_child(value, 0));
	}

	value_free(value);
	return left;
}

Value*
value_symbol_lambda_eval(Value *value, Env *env)
{
	(void)env;

	size_t i;
	Value *formals,
		*body;

	VALIDATE_SYMBOL_ARGS_COUNT("\\", value, 2);
	VALIDATE_SYMBOL_ARG_TYPE("\\", value, 0, QEXPRESSION_TYPE);
	VALIDATE_SYMBOL_ARG_TYPE("\\", value, 1, QEXPRESSION_TYPE);

	for (i = 0; i < value->children[0]->children_count; ++i)
		VALIDATE_SYMBOL_ARGS(
			value,
			value->children[0]->children[i]->type == SYMBOL_TYPE,
			"\\: Invalid type for %zu arg. Expected %s. Got %s.",
			value_type_names[SYMBOL_TYPE],
			value_type_names[value->children[0]->children[i]->type]
		);

	/* Extract formals and body and allocate new lambda */
	formals = value_pop_child(value, 0);
	body = value_pop_child(value, 0);
	value_free(value);
	return value_lambda_alloc(formals, body);
}

Value*
value_symbol_le_eval(Value *value, Env *env)
{
	return value_symbol_ordering_eval("<=", value, env);
}

Value*
value_symbol_list_eval(Value *value, Env *env)
{
	(void)env;
	value->type = QEXPRESSION_TYPE;
	return value;
}

Value*
value_symbol_load_eval(Value *value, Env *env)
{
	mpc_result_t mpc_result;
	char *mpc_error;
	Value *eval_result,
		*expressions;

	VALIDATE_SYMBOL_ARGS_COUNT("load", value, 1);
	VALIDATE_SYMBOL_ARG_TYPE("load", value, 0, STRING_TYPE);

	if (mpc_parse_contents(value->children[0]->string, Program, &mpc_result)) {
		/* Read contents */
		expressions = value_read(mpc_result.output);
		mpc_ast_delete(mpc_result.output);

		/* Evaluate each expression */
		while (expressions->children_count > 0) {
			eval_result = value_eval(value_pop_child(expressions, 0), env);
			if (eval_result->type == ERROR_TYPE)
				value_println(eval_result);
			value_free(eval_result);
		}

		/* Free expressions and arguments */
		value_free(expressions);
		value_free(value);

		return value_expression_alloc(SEXPRESSION_TYPE);
	}

	/* Get parsing error string */
	mpc_error = mpc_err_string(mpc_result.error);
	mpc_err_delete(mpc_result.error);

	/* Return error as value */
	eval_result = value_error_alloc(
		"Error loading %s: %s",
		value->children[0]->string,
		mpc_error
	);
	free(mpc_error);
	value_free(value);
	return eval_result;
}

Value*
value_symbol_lt_eval(Value *value, Env *env)
{
	return value_symbol_ordering_eval("<", value, env);
}

Value*
value_symbol_multiply_eval(Value *value, Env *env)
{
	return value_symbol_arithmetic_eval("*", value, env);
}

Value*
value_symbol_ne_eval(Value *value, Env *env)
{
	return value_symbol_cmp_eval("!=", value, env);;
}

Value*
value_symbol_not_eval(Value *value, Env *env)
{
	(void)env;

	ValueNumber result;

	VALIDATE_SYMBOL_ARGS_COUNT("!", value, 1);
	VALIDATE_SYMBOL_ARG_TYPE("!", value, 0, NUMBER_TYPE);

	result = !value->children[0]->number;
	value_free(value);
	return value_number_alloc(result);
}

Value*
value_symbol_or_eval(Value *value, Env *env)
{
	return value_symbol_condition_chain_eval("||", value, env);
}

Value*
value_symbol_print_eval(Value *value, Env *env)
{
	(void)env;

	size_t i;

	/* Print args followed by a space */
	for (i = 0; i < value->children_count; ++i) {
		value_print(value->children[i]);
		putchar(' ');
	}

	/* Print newline, free args and return sexpression */
	putchar('\n');
	value_free(value);
	return value_expression_alloc(SEXPRESSION_TYPE);
}

Value*
value_symbol_set_eval(Value *value, Env *env)
{
	return value_symbol_variable_eval("=", value, env);
}

Value*
value_symbol_substract_eval(Value *value, Env *env)
{
	return value_symbol_arithmetic_eval("-", value, env);
}

Value*
value_symbol_tail_eval(Value *value, Env *env)
{
	(void)env;

	Value *arg;

	VALIDATE_SYMBOL_ARGS_COUNT("tail", value, 1);
	arg = value_free_without_child(value, 0);

	if (arg->type == QEXPRESSION_TYPE) {
		VALIDATE_SYMBOL_ARGS(
			value,
			arg->children_count != 0,
			"tail: Argument is empty."
		);
		value_free(value_pop_child(arg, 0));
	} else if (arg->type == STRING_TYPE) {
		VALIDATE_SYMBOL_ARGS(
			value,
			strlen(arg->string) != 0,
			"tail: Argument is empty."
		);
		++arg->string;
	} else {
		ERROR_SYMBOL_ARGS(
			arg,
			"tail: Invalid arg type. Expected: %s or %s. Got: %s.",
			value_type_names[QEXPRESSION_TYPE],
			value_type_names[STRING_TYPE],
			value_type_names[arg->type]
		);
	}

	return arg;
}

Value*
value_symbol_while_eval(Value *value, Env *env)
{
	Value *condition_result,
		*result;

	VALIDATE_SYMBOL_ARGS_COUNT("while", value, 2);
	VALIDATE_SYMBOL_ARG_TYPE("tail", value, 0, QEXPRESSION_TYPE);
	VALIDATE_SYMBOL_ARG_TYPE("tail", value, 1, QEXPRESSION_TYPE);

	/* Set a default return value */
	result = value_expression_alloc(SEXPRESSION_TYPE);

	/* Convert children to sexpressions */
	value->children[0]->type = SEXPRESSION_TYPE;
	value->children[1]->type = SEXPRESSION_TYPE;

	/* Loop */
	while (1) {
		condition_result = value_eval(value_copy(value->children[0]), env);
		VALIDATE_SYMBOL_ARGS(
			value,
			condition_result->type == NUMBER_TYPE,
			"while: Condition isn't a number, but %s.",
			value_type_names[condition_result->type]
		);

		if (condition_result->number)
			result = value_eval(value_copy(value->children[1]), env);
		else
			break;
	}

	value_free(value);
	return result;
}

static unsigned char
value_eq(const Value *x, const Value *y)
{
	size_t i;

	if (x->type != y->type)
		return 0;

	switch (x->type) {
	case ERROR_TYPE:
		return strcmp(x->error, y->error) == 0;
	case FUNCTION_TYPE:
		if (x->builtin || y->builtin)
			return x->builtin == y->builtin;
		else
			return value_eq(x->lambda_formals, y->lambda_formals)
				&& value_eq(x->lambda_body, y->lambda_body);
	case NUMBER_TYPE:
		return x->number == y->number;
	case QEXPRESSION_TYPE: /* FALLTHROUGH*/
	case SEXPRESSION_TYPE:
		if (x->children_count != y->children_count)\
			return 0;
		for (i = 0; i < x->children_count; ++i)
			if (!value_eq(x->children[i], y->children[i]))
				return 0;
		return 1;
	case STRING_TYPE:
		return strcmp(x->string, y->string) == 0;
	case SYMBOL_TYPE:
		return strcmp(x->symbol, y->symbol) == 0;
	}
	return 0;
}

static void
value_expression_print(const Value *value)
{
	size_t i;

	if (value->type == SEXPRESSION_TYPE)
		putchar('(');
	else
		putchar('{');

	/* Print children separated by space */
	for (i = 0; i < value->children_count; ++i) {
		value_print(value->children[i]);
		if (i != value->children_count - 1)
			putchar(' ');
	}

	if (value->type == SEXPRESSION_TYPE)
		putchar(')');
	else
		putchar('}');
}

/* Moves `from`'s children to `to` and frees `from`. */
static void
value_extend_children(Value *to, Value *from)
{
	while (from->children_count > 0)
		value_add_child(to, value_pop_child(from, 0));
	value_free(from);
}

/* Moves `from`'s string to `to` and frees `from`. */
static void
value_extend_string(Value *to, Value *from)
{
	size_t to_len = strlen(to->string);
	to->string = realloc(to->string, to_len + strlen(from->string) + 1);
	strcpy(to->string + to_len, from->string);

	value_free(from);
}

/*
Free entire value but not child `child_i`.

Returns child `child_i`.
*/
static Value*
value_free_without_child(Value *value, size_t child_i)
{
	Value *child = value_pop_child(value, child_i);
	value_free(value);
	return child;
}

static Value*
value_function_call(Value *f, Env *env, Value *args)
{
	size_t args_given,
		formals_expected;
	Value *key,
		*value;

	/* Call builtin, if value isn't lambda */
	if (f->builtin)
		return f->builtin(args, env);

	/* Arguments information */
	formals_expected = f->lambda_formals->children_count;
	args_given = args->children_count;

	while (args->children_count > 0) {
		/* Check that arguments count greater than formals count */
		if (f->lambda_formals->children_count == 0) {
			value_free(f);
			value_free(args);
			return value_error_alloc(
				"Too many args. Expected %zu. Got %zu.",
				formals_expected,
				args_given
			);
		}

		/* Pop formal */
		key = value_pop_child(f->lambda_formals, 0);

		/* Bind all other arguments list to single formal after formal `&` */
		if (strcmp(key->symbol, "&") == 0) {
			/* Free `&` */
			value_free(key);

			/* Check that `&` followed by single formal */
			if (f->lambda_formals->children_count != 1) {
				value_free(f);
				value_free(args);
				return value_error_alloc("`&` not followed by single formal");
			}

			/* Pop next formal and set remaining arguments list to it in env */
			key = value_pop_child(f->lambda_formals, 0);
			args->type = QEXPRESSION_TYPE;
			env_set(f->env, key, args);
			value_free(key);
			break;
		} else {
			/* Bind argument to formal */
			value = value_pop_child(args, 0);
			env_set(f->env, key, value);
			value_free(key);
			value_free(value);
		}
	}

	/*
	If no arguments remain, but formals remain and `&` is next in formals,
	then bind followed formal to empty list
	*/
	if (
		f->lambda_formals->children_count > 0
		&& strcmp(f->lambda_formals->children[0]->symbol, "&") == 0
	) {
		/* Check that `&` followed by single formal */
		if (f->lambda_formals->children_count != 2) {
			value_free(f);
			value_free(args);
			return value_error_alloc("`&` not followed by single formal");
		}

		/* Free `&` */
		value_free(value_pop_child(f->lambda_formals, 0));

		/* Followed formal and it's value */
		key = value_pop_child(f->lambda_formals, 0);
		value = value_expression_alloc(QEXPRESSION_TYPE);

		/* Set and free */
		env_set(f->env, key, value);
		value_free(key);
		value_free(value);
	}

	if (f->lambda_formals->children_count == 0) {
		/* Eval function body as sexpression with parent env */
		f->env->parent = env;
		f->lambda_body->type = SEXPRESSION_TYPE;
		value = value_eval(value_copy(f->lambda_body), f->env);

		/* Free arguments and lambda function and return a result */
		value_free(args);
		value_free(f);
		return value;
	}

	/* Copy partial called function with remaining formals */
	value_free(args);
	return f;
}

static void
value_function_print(const Value *value)
{
	if (value->builtin) {
		printf("<builtin>");
	} else {
		printf("(\\ ");
		value_print(value->lambda_formals);
		putchar(' ');
		value_print(value->lambda_body);
		putchar(')');
	}
}

static Value*
value_lambda_alloc(Value *args, Value *body)
{
	Value *value = malloc(sizeof(Value));
	value->type = FUNCTION_TYPE;
	value->env = env_alloc();
	value->lambda_formals = args;
	value->lambda_body = body;
	value->builtin = NULL;
	return value;
}

static Value*
value_number_alloc(ValueNumber number)
{
	Value *value = malloc(sizeof(Value));
	value->type = NUMBER_TYPE;
	value->number = number;
	return value;
}

static Value*
value_number_read(const mpc_ast_t *ast)
{
	double number;
	errno = 0;
	number = strtod(ast->contents, NULL);
	if (errno == ERANGE)
		return value_error_alloc("Invalid number: %s.", ast->contents);
	return value_number_alloc(number);
}

static Value*
value_pop_child(Value *value, size_t child_i)
{
	Value *child = value->children[child_i];

	/* Shift memory */
	memcpy(
		value->children + child_i,
		value->children + child_i + 1,
		sizeof(Value *) * (value->children_count - child_i - 1)
	);

	value->children_count--;

	/* Fit memory */
	value->children = realloc(
		value->children,
		sizeof(Value *) * value->children_count
	);
	return child;
}

static void
value_print(const Value *value)
{
	switch (value->type) {
	case ERROR_TYPE:
		printf("Error: %s", value->error);
		break;
	case FUNCTION_TYPE:
		value_function_print(value);
		break;
	case NUMBER_TYPE:
		printf("%f", value->number);
		break;
	case QEXPRESSION_TYPE: /* FALLTHROUGH*/
	case SEXPRESSION_TYPE:
		value_expression_print(value);
		break;
	case STRING_TYPE:
		value_string_print(value);
		break;
	case SYMBOL_TYPE:
		printf("%s", value->symbol);
		break;
	}
}

static Value*
value_sexpression_eval(Value *value, Env *env)
{
	size_t i;
	Value *first_child,
		*result;

	/* Eval children and check errors of evaluation */
	for (i = 0; i < value->children_count; ++i) {
		value->children[i] = value_eval(value->children[i], env);
		if (value->children[i]->type == ERROR_TYPE)
			return value_free_without_child(value, i);
	}

	/* Check if there is no arguments */
	if (value->children_count == 0)
		return value;
	else if (value->children_count == 1)
		return value_free_without_child(value, 0);

	/* Pop function */
	first_child = value_pop_child(value, 0);
	if (first_child->type != FUNCTION_TYPE) {
		result = value_error_alloc(
			"()'s first child is not a function, but %s.",
			value_type_names[first_child->type]
		);
		value_free(first_child);
		value_free(value);
		return result;
	}

	/* Call function: fill env with arguments, etc. */
	return value_function_call(first_child, env, value);
}

static void
value_string_print(const Value *value)
{
	char *escaped = strdup(value->string);
	escaped = mpcf_escape(escaped);
	printf("\"%s\"", escaped);
	free(escaped);
}

static Value*
value_string_read(const mpc_ast_t *ast)
{
	Value *rv;
	/* Duplicate a content and cut off first and last quotes */
	char *unescaped = strdup(ast->contents + 1);
	unescaped[strlen(unescaped) - 1] = '\0';

	/* Unescape a string and alloc new value */
	unescaped = mpcf_unescape(unescaped);
	rv = value_string_alloc(unescaped);
	free(unescaped);
	return rv;
}

static Value*
value_symbol_arithmetic_eval(const char *symbol, Value *value, const Env *env)
{
	(void)env;

	size_t i;
	Value *left,
		*right;
	unsigned char add = strcmp(symbol, "+") == 0,
		substract = strcmp(symbol, "-") == 0,
		multiply = strcmp(symbol, "*") == 0,
		divide = strcmp(symbol, "/") == 0;

	/* Check that children are numbers */
	for (i = 0; i < value->children_count; ++i)
		VALIDATE_SYMBOL_ARG_TYPE(symbol, value, i, NUMBER_TYPE);

	left = value_pop_child(value, 0);

	/* Negative number */
	if (substract && value->children_count == 0)
		left->number = -left->number;

	while (value->children_count > 0) {
		right = value_pop_child(value, 0);
		if (add) {
			left->number += right->number;
		} else if (substract) {
			left->number -= right->number;
		} else if (multiply) {
			left->number *= right->number;
		} else if (divide) {
			/* Check division by zero */
			if (right->number == 0) {
				value_free(right);
				value_free(left);
				left = value_error_alloc("Division by zero.");
				break;
			}

			left->number /= right->number;
		}

		value_free(right);
	}

	value_free(value);
	return left;
}

static Value*
value_symbol_cmp_eval(const char *symbol, Value *value, const Env *env)
{
	(void)env;
	unsigned char eq;

	VALIDATE_SYMBOL_ARGS_COUNT(symbol, value, 2);

	eq = value_eq(value->children[0], value->children[1]);
	value_free(value);
	return value_number_alloc(strcmp(symbol, "==") == 0 ? eq : !eq);
}


static Value*
value_symbol_condition_chain_eval(
	const char *symbol,
	Value *value,
	const Env *env
)
{
	(void)env;

	size_t i;
	unsigned char or = 0,
		and = 0;
	ValueNumber result;

	VALIDATE_SYMBOL_ARGS(
		value,
		value->children_count >= 2,
		"%s: Too few arguments. Expected greater or equal to 2. Got %zu.",
		symbol,
		value->children_count
	);

	if (strcmp(symbol, "||") == 0)
		or = 1;
	else if (strcmp(symbol, "&&") == 0)
		and = 1;

	for (i = 0; i < value->children_count; ++i) {
		VALIDATE_SYMBOL_ARG_TYPE(symbol, value, i, NUMBER_TYPE);

		if (i == 0)
			result = value->children[i]->number;
		else if (or)
			result = result || value->children[i]->number;
		else if (and)
			result = result && value->children[i]->number;

		if ((!result && and) || (result && or))
			break;
	}

	value_free(value);
	return value_number_alloc(result);
}

static Value*
value_symbol_ordering_eval(const char *symbol, Value *value, const Env *env)
{
	(void)env;
	ValueNumber result = 0;

	VALIDATE_SYMBOL_ARGS_COUNT(symbol, value, 2);
	VALIDATE_SYMBOL_ARG_TYPE(symbol, value, 0, NUMBER_TYPE);
	VALIDATE_SYMBOL_ARG_TYPE(symbol, value, 1, NUMBER_TYPE);

	if (strcmp(symbol, ">") == 0)
		result = value->children[0]->number > value->children[1]->number;
	else if (strcmp(symbol, "<") == 0)
		result = value->children[0]->number < value->children[1]->number;
	else if (strcmp(symbol, ">=") == 0)
		result = value->children[0]->number >= value->children[1]->number;
	else if (strcmp(symbol, "<=") == 0)
		result = value->children[0]->number <= value->children[1]->number;

	value_free(value);
	return value_number_alloc(result);
}

/*
To set variable with `symbol` keyword to `env` with first child of `value`
as formals list and other children of `value` as arguments.
*/
static Value*
value_symbol_variable_eval(const char *symbol, Value *value, Env *env)
{
	size_t i;
	Value *symbols;

	VALIDATE_SYMBOL_ARGS(
		value,
		value->children_count >= 2,
		"%s: Required at least one value.",
		symbol
	);

	symbols = value->children[0];

	VALIDATE_SYMBOL_ARGS(
		value,
		symbols->type == QEXPRESSION_TYPE,
		"%s: Arguments not in {}.",
		symbol
	);
	for (i = 0; i < symbols->children_count; ++i)
		VALIDATE_SYMBOL_ARGS(
			value,
			symbols->children[i]->type == SYMBOL_TYPE,
			"%s: Argument not a symbol.",
			symbol
		);
	VALIDATE_SYMBOL_ARGS(
		value,
		symbols->children_count == value->children_count - 1,
		"%s: Arguments count not equals to values count.",
		symbol
	);

	/* Set value to key locally or globally */
	for (i = 0; i < symbols->children_count; ++i) {
		if (strcmp(symbol, "=") == 0)
			env_set(env, symbols->children[i], value->children[i + 1]);
		else if (strcmp(symbol, "def") == 0)
			env_set_for_ancestor(env, symbols->children[i], value->children[i + 1]);
	}

	value_free(value);
	return value_expression_alloc(SEXPRESSION_TYPE);
}
