#ifndef _ENV_H
#define _ENV_H

#include <stdlib.h>
#include "config.h"
#include "value.h"

typedef struct EnvEntry EnvEntry;
struct EnvEntry {
	EnvEntry *next;
	char *symbol;
	Value *value;
};

typedef struct Env {
	Env *parent;
	EnvEntry *entries[ENV_ENTRY_KEY_HASH_MODULO];
} Env;

Env *env_alloc(void);
Env *env_copy(const Env *);
Value *env_get(const Env *, const Value *);
void env_free(Env *);
void env_set(Env *, const Value *, const Value *);
void env_set_builtins(Env *);
void env_set_for_ancestor(Env *, const Value *, const Value *);

#endif /* _ENV_H */
