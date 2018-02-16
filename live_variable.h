/* NOTE: `#if NO_VA_ARGS == 0` is for e.g. C89, where __VA_ARGS__ is not available -> more work needed to make structs live
TODO:
 - better accommodate C89, e.g. with STRUCT2, STRUCT3 etc
 - add known types first time (like DEBUG_LIVE_IF)
 - add "Leave Empty" comment under macro lists
 - allow types to be specified elsewhere?
 - observe arrays
*/

#ifndef LIVE_VARIABLE_H
#include <stdio.h>

typedef struct debug_variable debug_variable;

typedef int debug_if;
#define DEBUG_VAR_DECLARE(type, Name) static type DEBUG_LIVE_VAR_## Name
#define DEBUG_IF_DECLARE(Name) DEBUG_VAR_DECLARE(debug_if, Name)
#define DEBUG_LIVE_if(Name) if(DEBUG_LIVE_VAR_## Name)

// Declare the if statement variables
#define DEBUG_LIVE_IF(Name, init) DEBUG_IF_DECLARE(Name);
#define DEBUG_LIVE_VAR(type, Name, init)
#if !NO_VA_ARGS
#define DEBUG_LIVE_STRUCT(type, Name, ...)
#endif//!NO_VA_ARGS
	DEBUG_LIVE_VARS
#if !NO_VA_ARGS
#undef DEBUG_LIVE_STRUCT
#endif//!NO_VA_ARGS
#undef DEBUG_LIVE_VAR
#undef DEBUG_LIVE_IF
// from now on, treat ifs like other vars
#define DEBUG_LIVE_IF(Name, init) DEBUG_LIVE_VAR(debug_if, Name, init)

#define LIVE_VARIABLE_H
#endif//LIVE_VARIABLE_H

/* OBSERVED ******************************************************************/
#ifndef DEBUG_OBSERVED_H
#define DEBUG_OBSERVED_COUNTER __COUNTER__
debug_variable DebugObservedVariables[];
static unsigned int DebugObservedCount = 0;
unsigned int DebugObservedArrayLen;

/* Usage:    int X = 6;    =>    DEBUG_OBSERVE(int, X) = 6;    */
/* TODO: do `if` and swap as atomic operation */
#define DEBUG_OBSERVE(type, name) \
static type name; \
{ \
	static int DebugObserved_## name ##_NeedsInit = 1; \
	if(DebugObserved_## name ##_NeedsInit) { DEBUG_OBSERVED_COUNTER; \
		DebugObserved_## name ##_NeedsInit = 0; \
		debug_variable DebugObserved = { DebugVarType_## type, #name, &name }; \
		DebugObservedVariables[++DebugObservedCount] = DebugObserved; \
	} \
} \
name
/* ^^^ this has the feature that it doesn't trigger a warning if unused elsewhere */

#define DEBUG_OBSERVED_DECLARATION  \
	enum { DebugObservedArrayLenEnum = DEBUG_OBSERVED_COUNTER + 1 }; \
	debug_variable DebugObservedVariables[DebugObservedArrayLenEnum]; \
	unsigned int DebugObservedArrayLen = DebugObservedArrayLenEnum;
/* NOTE: 0 is free for invalid queries */
#define DEBUG_OBSERVED_H
#endif/*DEBUG_OBSERVED_H*/
/* END OBSERVED **************************************************************/

/* LIVE **********************************************************************/
/* TODO: should I include the function definitions above? */
#if defined(DEBUG_LIVE_IMPLEMENTATION) && !defined(DEBUG_LIVE_IMPLEMENTATION_H)
#ifndef DEBUG_TYPES
#error You have to #define DEBUG_TYPES! Do so in this format: `DEBUG_TYPE(type, printf_format_string)`
#endif
/* #ifndef DEBUG_LIVE_VARS */
/* #error You have to #define DEBUG_LIVE_VARS! Do so in this format: `DEBUG_LIVE_VAR(type, name)` */
/* #endif */

	/* DEBUG_TYPE(Unknown, "%d") \ */
#define DEBUG_LIVE_INTERNAL_TYPES \
	DEBUG_TYPE(debug_if, "%u") \

#define DEBUG_LIVE_TYPES \
	DEBUG_LIVE_INTERNAL_TYPES \
	DEBUG_TYPES \

/* //// Debug variable type, name and pointer to value */
#if !NO_VA_ARGS
#define DEBUG_TYPE_STRUCT(type, format, ...) DEBUG_TYPE(type, format)
#endif/*!NO_VA_ARGS */
typedef struct debug_variable
{
	enum {
#define DEBUG_TYPE(type, format) \
		DebugVarType_## type,
		DEBUG_LIVE_TYPES
#undef DEBUG_TYPE
	} Type;
	char *Name;
	union {
		void *Data;
#define DEBUG_TYPE(type, format) \
		type *Value_## type;
		DEBUG_LIVE_TYPES
#undef DEBUG_TYPE
	};
} debug_variable;
#if !NO_VA_ARGS
#undef DEBUG_TYPE_STRUCT
#endif/*!NO_VA_ARGS */

/* //// Reference debug array instances by name */
enum {
#if !NO_VA_ARGS
#define DEBUG_LIVE_STRUCT(type, Name, ...) DebugVarIndex_## Name,
#endif/*!NO_VA_ARGS */
#define DEBUG_LIVE_VAR(type, Name, init) DebugVarIndex_## Name,
	DEBUG_LIVE_VARS
#undef DEBUG_LIVE_VAR
#undef DEBUG_LIVE_STRUCT
};


/* //// Global variables of the right type */
#if !NO_VA_ARGS
#define DEBUG_LIVE_STRUCT(type, Name, ...) DEBUG_VAR_DECLARE(type, Name) = __VA_ARGS__;
#endif/*!NO_VA_ARGS */
#define DEBUG_LIVE_VAR(type, Name, init)   DEBUG_VAR_DECLARE(type, Name) = init;
	DEBUG_LIVE_VARS
#undef DEBUG_LIVE_VAR
#undef DEBUG_LIVE_STRUCT

/* //// Array of annotated typed pointers to global vars */
debug_variable DebugLiveVariables[] = {
#if !NO_VA_ARGS
#define DEBUG_LIVE_STRUCT(type, Name, ...) { DebugVarType_## type, #Name, (void *)&DEBUG_LIVE_VAR_## Name },
#endif/*!NO_VA_ARGS */
#define DEBUG_LIVE_VAR(type, Name, init) { DebugVarType_## type, #Name, (void *)&DEBUG_LIVE_VAR_## Name },
	/* NOTE: ^^^ only sets the first type listed (suggested integral type for toggling) */
		DEBUG_LIVE_VARS
#undef DEBUG_LIVE_VAR
#if !NO_VA_ARGS
#undef DEBUG_LIVE_STRUCT
#endif/*!NO_VA_ARGS */
};

/* //// Compilation info */
static int DebugLiveVar_IsCompiling;

/* returns the result of the system call (where 0 is success), or 0 if no action taken */
static int
DebugLiveVar_Recompile(char *BuildCall)
{
	int Result = 0;
	/* TODO: CAS */
	if(! DebugLiveVar_IsCompiling)
	{
		DebugLiveVar_IsCompiling = 1;
		Result = system(BuildCall);
	}
	return Result;
}

/* returns 1 if all file prints were successful */
static int
DebugLiveVar_RewriteDefines(char *Filename)
{
	int Result = 1;
	if(! DebugLiveVar_IsCompiling)
	{
		FILE *File = fopen(Filename, "w");
		Result &= fprintf(File, "#ifndef LIVE_VAR_DEFINES_H\n") > 0;
		for(u32 iVar = 0; iVar < ArrayCount(DebugLiveVariables); ++iVar)
		{
			debug_variable Var = DebugLiveVariables[iVar];
			switch(Var.Type)
			{
#if ! NO_VA_ARGS
#define DEBUG_LIVE_MEMBER(struct, member) Var.Value_## struct->member
#define DEBUG_TYPE_STRUCT(type, format, ...) case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, __VA_ARGS__) > 0; break;
#endif/*! NO_VA_ARGS */
#define DEBUG_TYPE(type, format)		case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, *Var.Value_## type) > 0; break;
				DEBUG_TYPES
#undef DEBUG_TYPE
#if ! NO_VA_ARGS
#undef DEBUG_TYPE_STRUCT
#undef DEBUG_LIVE_MEMBER
#endif/*NO_VA_ARGS == 0 */
			}
		}
		Result &= fprintf(File, "#define LIVE_VAR_DEFINES_H\n#endif") > 0;
		fclose(File);
	}
	return Result;
}

/* returns 1 if all file prints were successful */
/* TODO: make more obvious that nothing will happen if currently compiling */
static int
DebugLiveVar_RewriteConfig(char *Filename)
{
	int Result = 1;
	if(! DebugLiveVar_IsCompiling)
	{
		FILE *File = fopen(Filename, "w");
		Result &= fprintf(File, "#ifndef LIVE_VAR_CONFIG_H\n\n#define DEBUG_TYPES \\\n") > 0;
#if ! NO_VA_ARGS
#define DEBUG_LIVE_MEMBER(struct, member) Var.Value_## struct.member
#define DEBUG_TYPE_STRUCT(type, format, ...) \
		Result &= fputs("\tDEBUG_TYPE_STRUCT("#type", "#format", "#__VA_ARGS__") \\\n", File) >= 0;
#endif/*! NO_VA_ARGS */
#define DEBUG_TYPE(type, format) \
		Result &= fputs("\tDEBUG_TYPE("#type", "#format") \\\n", File) >= 0;
		DEBUG_TYPES
#undef DEBUG_TYPE
#if ! NO_VA_ARGS
#undef DEBUG_TYPE_STRUCT
#undef DEBUG_LIVE_MEMBER
#endif/*! NO_VA_ARGS */

			Result &= fprintf(File, "\n\n#define DEBUG_LIVE_VARS \\\n") > 0;
		for(u32 iVar = 0; iVar < ArrayCount(DebugLiveVariables); ++iVar)
		{
			debug_variable Var = DebugLiveVariables[iVar];
			switch(Var.Type)
			{
				case DebugVarType_debug_if: Result &= fprintf(File, "\tDEBUG_LIVE_IF(%s, %u) \\\n", Var.Name, *Var.Value_debug_if) > 0; break;
#if ! NO_VA_ARGS
#define DEBUG_LIVE_MEMBER(struct, member) Var.Value_## struct->member
#define DEBUG_TYPE_STRUCT(type, format, ...) \
				case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_LIVE_STRUCT("#type", %s, "format") \\\n", Var.Name, __VA_ARGS__) > 0; break;
#endif/*! NO_VA_ARGS */
#define DEBUG_TYPE(type, format) \
				case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_LIVE_VAR("#type", %s, "format") \\\n", Var.Name, *Var.Value_## type) > 0; break;
			DEBUG_TYPES
#undef DEBUG_TYPE
#if ! NO_VA_ARGS
#undef DEBUG_TYPE_STRUCT
#undef DEBUG_LIVE_MEMBER
#endif/*! NO_VA_ARGS */
			}
		}
		Result &= fputs("\n\n#include \""__FILE__"\"\n#define LIVE_VAR_CONFIG_H\n#endif", File) >= 0;
		fclose(File);
	}
	return Result;
}
#define DEBUG_LIVE_IMPLEMENTATION_H
#endif/*DEBUG_LIVE_IMPLEMENTATION && !DEBUG_LIVE_IMPLEMENTATION_H*/

