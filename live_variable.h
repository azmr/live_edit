#ifndef LIVE_VARIABLE_H
#include <stdio.h>
#include <stdlib.h>

#ifndef DEBUG_VARIABLE_TYPES
#error You have to #define DEBUG_VARIABLE_TYPES! Do so in this format: `DEBUG_VARIABLE_TYPE(type, printf_format_string)`
#endif
#ifndef DEBUG_VARIABLE_LISTINGS
#error You have to #define DEBUG_VARIABLE_LISTINGS! Do so in this format: `DEBUG_VARIABLE_LISTING(type, name)`
#endif

#define DEBUG_VARIABLE_STRUCT(type, format, ...) DEBUG_VARIABLE_TYPE(type, format)
typedef struct debug_variable
{
	enum {
#define DEBUG_VARIABLE_TYPE(type, format) \
		DebugVarType_## type,
		DEBUG_VARIABLE_TYPES
#undef DEBUG_VARIABLE_TYPE
	} Type;
	char *Name;
	union {
#define DEBUG_VARIABLE_TYPE(type, format) \
		type Value_## type;
		DEBUG_VARIABLE_TYPES
#undef DEBUG_VARIABLE_TYPE
	};
} debug_variable;
#undef DEBUG_VARIABLE_STRUCT

debug_variable DebugVariables[] = {
#define DEBUG_VARIABLE_LISTING(type, Name) \
	{ DebugVarType_## type, #Name, 0 },
	// NOTE: this only sets the first type listed (suggested integral type for toggling)
	DEBUG_VARIABLE_LISTINGS
#undef DEBUG_VARIABLE_LISTING
};

typedef struct debug_var_state
{
	enum compile_state {
		CompileState_Unacknowledged = 0,
		CompileState_Normal,
		CompileState_Compiling,
	} State; 
    int RecompileIsRequested; // on compile -> 0 -> no longer requesting recompile
} debug_var_state;
static debug_var_state DebugVarState;

static inline void RequestRecompile() { DebugVarState.RecompileIsRequested = 1; }
static inline int IsCompiling() { int Result = (DebugVarState.State == CompileState_Compiling); return Result; }

static void
UpdateCompiledVariables()
{
	unsigned int iVar = 0;
#define DEBUG_VARIABLE_LISTING(type, Name) \
	type TempDebugVar_## Name = DEBUGVAR_## Name; \
	DebugVariables[iVar++].Value_## type = TempDebugVar_## Name;
	DEBUG_VARIABLE_LISTINGS
#undef  DEBUG_VARIABLE_LISTING
}

static inline int
UpdateVariablesIfNeeded()
{
	int Result = (DebugVarState.State == CompileState_Unacknowledged);
	if(Result)
	{
		UpdateCompiledVariables();
		DebugVarState.State = CompileState_Normal;
	}
	return Result;
}

// returns the result of the system call (where 0 is success), or 0 if no recompile is requested
static int
RecompileOnRequest(char *BuildCall, char *Filename)
{
	int Result = 0;
	UpdateVariablesIfNeeded();
    if(DebugVarState.State == CompileState_Normal && DebugVarState.RecompileIsRequested)
    {
    	DebugVarState.State = CompileState_Compiling;
		FILE *File = fopen(Filename, "w");
		fprintf(File, "#ifndef LIVE_VAR_CONFIG_H\n");
		for(u32 iVar = 0; iVar < ArrayCount(DebugVariables); ++iVar)
		{
			debug_variable Var = DebugVariables[iVar];
			switch(Var.Type)
			{
#define DEBUG_STRUCT(type) Var.Value_## type.
#define DEBUG_VAL(type) Var.Value_## type
#define DEBUG_VARIABLE_TYPE(type, format) DEBUG_VARIABLE_STRUCT(type, format, Var.Value_## type)
#define DEBUG_VARIABLE_STRUCT(type, format, ...) case DebugVarType_## type: fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, __VA_ARGS__); break;
				DEBUG_VARIABLE_TYPES
#undef DEBUG_VARIABLE_TYPE
#undef DEBUG_VARIABLE_STRUCT
#undef DEBUG_STRUCT
#undef DEBUG_VAL
			}
		}
		fprintf(File, "#define LIVE_VAR_CONFIG_H\n#endif");
		fclose(File);

		Result = system(BuildCall);
    }
    return Result;
}

#define LIVE_VARIABLE_H
#endif//LIVE_VARIABLE_H
