#include <stdio.h>
#include <editline.h>
#include "env.h"
#include "grammar.h"
#include "mpc.h"
#include "value.h"

static void parsers_init(void);
static void parsers_free(void);
static void interpret(Env *env);
static void read(size_t argc, char **argv, Env *env);

static void
interpret(Env *env)
{
	char *input;
	mpc_result_t mpc_result;
	Value *value;

	while (1) {
		input = readline(">>> ");
		add_history(input);

		/* Parse an input */
		if (mpc_parse("<stdin>", input, Program, &mpc_result)) {
			/* Read, eval and print parsed tree */
			value = value_eval(value_read(mpc_result.output), env);
			value_println(value);

			/* Free eval result and parsed tree */
			value_free(value);
			mpc_ast_delete(mpc_result.output);
		} else {
			/* Print parsing error */
			mpc_err_print(mpc_result.error);
			mpc_err_delete(mpc_result.error);
		}

		free(input);
	}
}

static void
parsers_init(void)
{
	Program = mpc_new("Program");
	Expression = mpc_new("Expression");
	Sexpression = mpc_new("Sexpression");
	Qexpression = mpc_new("Qexpression");
	Symbol = mpc_new("Symbol");
	String = mpc_new("String");
	Number = mpc_new("Number");
	Comment = mpc_new("Comment");

	mpca_lang(
		MPCA_LANG_DEFAULT,
		GRAMMAR,
		Program,
		Expression,
		Sexpression,
		Qexpression,
		Symbol,
		String,
		Number,
		Comment
	);
}

static void
parsers_free(void)
{
	mpc_cleanup(
		8,
		Program,
		Expression,
		Sexpression,
		Qexpression,
		Symbol,
		Number,
		String,
		Comment
	);
}

static void
read(size_t argc, char **argv, Env *env)
{
	size_t i;
	Value *value;

	/* Read filenames */
	for (i = 1; i < argc; i++) {
		value = value_expression_alloc(SEXPRESSION_TYPE);
		value_add_child(value, value_string_alloc(argv[i]));
		value = value_symbol_load_eval(value, env);
		if (value->type == ERROR_TYPE)
			value_println(value);
		value_free(value);
	}
}

int
main(int argc, char **argv) {
	Env *env = env_alloc();
	env_set_builtins(env);

	parsers_init();
	if (argc == 1) {
		argv[argc++] = "std";
		read(argc, argv, env);
		interpret(env);
	} else if (argc == 2 && strstr(argv[1], "--no-std")) {
		interpret(env);
	} else {
		read(argc, argv, env);
	}
	parsers_free();

	env_free(env);
	return EXIT_SUCCESS;
}
