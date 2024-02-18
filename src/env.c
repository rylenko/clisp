#include <stdlib.h>
#include "env.h"
#include "utils.h"

static EnvEntry *env_entry_alloc(const char *symbol, const Value *value);
static EnvEntry *env_entry_copy(const EnvEntry *entry);
static void env_entry_free(EnvEntry *entry);
static size_t env_entry_hash_key(const Value *key);
static EnvEntry *env_lookup(const Env *env, const Value *key);
static void env_set_builtin(
	Env *env,
	const char *symbol,
	ValueBuiltin builtin
);

Env*
env_alloc(void)
{
	size_t i;
	Env *rv = malloc(sizeof(Env));
	rv->parent = NULL;
	for (i = 0; i < ENV_ENTRY_KEY_HASH_MODULO; ++i)
		rv->entries[i] = NULL;
	return rv;
}

Env*
env_copy(const Env *env)
{
	size_t i;
	Env *new_env = malloc(sizeof(Env));
	new_env->parent = env->parent;
	for (i = 0; i < ENV_ENTRY_KEY_HASH_MODULO; ++i) {
		new_env->entries[i] = env->entries[i]
			? env_entry_copy(env->entries[i])
			: NULL;
	}
	return new_env;
}

void
env_free(Env *env)
{
	size_t i;
	for (i = 0; i < ENV_ENTRY_KEY_HASH_MODULO; ++i)
		if (env->entries[i])
			env_entry_free(env->entries[i]);
	free(env);
}

Value*
env_get(const Env *env, const Value *key)
{
	EnvEntry *entry = env_lookup(env, key);
	if (entry)
		return value_copy(entry->value);
	else if (env->parent)
		return env_get(env->parent, key);
	return value_error_alloc("Invalid symbol: %s.", key->symbol);
}

void
env_set(Env *env, const Value *key, const Value *value)
{
	size_t hash;
	EnvEntry *entry = env_lookup(env, key);

	if (entry) {
		/* Free old value and set a new */
		value_free(entry->value);
		entry->value = value_copy(value);
	} else {
		/* Create new entry */
		hash = env_entry_hash_key(key);
		entry = env_entry_alloc(key->symbol, value);
		entry->next = env->entries[hash];
		env->entries[hash] = entry;
	}
}

void
env_set_builtins(Env *env)
{
	env_set_builtin(env, "=", value_symbol_set_eval);
	env_set_builtin(env, "+", value_symbol_add_eval);
	env_set_builtin(env, "-", value_symbol_substract_eval);
	env_set_builtin(env, "*", value_symbol_multiply_eval);
	env_set_builtin(env, "/", value_symbol_divide_eval);
	env_set_builtin(env, "==", value_symbol_eq_eval);
	env_set_builtin(env, "!=", value_symbol_ne_eval);
	env_set_builtin(env, ">", value_symbol_gt_eval);
	env_set_builtin(env, ">=", value_symbol_ge_eval);
	env_set_builtin(env, "<", value_symbol_lt_eval);
	env_set_builtin(env, "<=", value_symbol_le_eval);
	env_set_builtin(env, "!", value_symbol_not_eval);
	env_set_builtin(env, "||", value_symbol_or_eval);
	env_set_builtin(env, "&&", value_symbol_and_eval);
	env_set_builtin(env, "\\", value_symbol_lambda_eval);
	env_set_builtin(env, "def", value_symbol_def_eval);
	env_set_builtin(env, "error", value_symbol_error_eval);
	env_set_builtin(env, "eval", value_symbol_eval_eval);
	env_set_builtin(env, "head", value_symbol_head_eval);
	env_set_builtin(env, "if", value_symbol_if_eval);
	env_set_builtin(env, "while", value_symbol_while_eval);
	env_set_builtin(env, "input", value_symbol_input_eval);
	env_set_builtin(env, "join", value_symbol_join_eval);
	env_set_builtin(env, "list", value_symbol_list_eval);
	env_set_builtin(env, "load", value_symbol_load_eval);
	env_set_builtin(env, "print", value_symbol_print_eval);
	env_set_builtin(env, "tail", value_symbol_tail_eval);
}

void
env_set_for_ancestor(Env *env, const Value *key, const Value *value)
{
	while (env->parent)
		env = env->parent;
	env_set(env, key, value);
}

static EnvEntry*
env_entry_alloc(const char *symbol, const Value *value)
{
	EnvEntry *rv = malloc(sizeof(EnvEntry));
	rv->next = NULL;
	rv->symbol = strdup(symbol);
	rv->value = value_copy(value);
	return rv;
}

static EnvEntry*
env_entry_copy(const EnvEntry *entry)
{
	EnvEntry *rv = env_entry_alloc(entry->symbol, entry->value);
	if (entry->next)
		rv->next = env_entry_copy(entry->next);
	return rv;
}

static void
env_entry_free(EnvEntry *entry)
{
	if (entry->next)
		env_entry_free(entry->next);
	free(entry);
}

static size_t
env_entry_hash_key(const Value *key)
{
	char *ptr = key->symbol;
	size_t hash = 0;

	for (; *ptr != '\0'; ++ptr)
		hash = *ptr + 31 * hash;
	return hash % ENV_ENTRY_KEY_HASH_MODULO;
}

static EnvEntry*
env_lookup(const Env *env, const Value *key)
{
	EnvEntry *entry = env->entries[env_entry_hash_key(key)];
	for (; entry; entry = entry->next)
		if (strcmp(entry->symbol, key->symbol) == 0)
			return entry;
	return NULL;
}

static void
env_set_builtin(Env *env, const char *symbol, ValueBuiltin builtin)
{
	/* Allocate key and value */
	Value *key = value_symbol_alloc((char *)symbol),
		*value = value_builtin_alloc(builtin);

	env_set(env, key, value);

	/* Free key and value because of copies in `env_set` */
	value_free(key);
	value_free(value);
}
