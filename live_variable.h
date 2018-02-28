/* NOTE: `#if NO_VA_ARGS == 0` is for e.g. C89, where __VA_ARGS__ is not available -> more work needed to make structs live
TODO:
 - add known types first time (like DEBUG_LIVE_IF)
 - add C basic types?
 - add "Leave Empty" comment under macro lists
 - allow types to be specified elsewhere?
 - watch arrays
 - construct format strings from e.g. nested structs
 - define something to return to normal behaviour
 - also capture __FILE__ and __LINE__
*/

/* allow for va_args to have commas replaced */
#define _ ,

#ifndef LIVE_VARIABLE_H
#include <stdio.h>

#ifndef DEBUG_ATOMIC_EXCHANGE /* user can define it to e.g. C11 atomic_exchange or Windows InterlockedExchange */
/* sets bool to 1, evaluates to previous value of bool */
#define DEBUG_ATOMIC_EXCHANGE(ptr, desired) (*(ptr) ? 1 : (*(ptr))++)
#endif

typedef struct debug_variable debug_variable;

typedef int debug_if;
#define DEBUG_VAR_DECLARE(type, Name) static type DEBUG_LIVE_VAR_## Name
#define DEBUG_LIVE_if(Name) if(DEBUG_LIVE_VAR_## Name)

/* Declare the if statement variables */
#define DEBUG_LIVE_IF(Name, init) DEBUG_VAR_DECLARE(debug_if, Name);
#if NO_VA_ARGS
#define DEBUG_LIVE_TWEAK(type, Name, init)
#else
#define DEBUG_LIVE_TWEAK(type, Name, ...)
#endif/*NO_VA_ARGS*/
	DEBUG_LIVE_VARS
#undef DEBUG_LIVE_TWEAK
#undef DEBUG_LIVE_IF
/* from now on, treat ifs like other vars */
#define DEBUG_LIVE_IF(Name, init) DEBUG_LIVE_TWEAK(debug_if, Name, init)

#define LIVE_VARIABLE_H
#endif/*LIVE_VARIABLE_H*/

/* WATCHED ******************************************************************/
/* Usage:    int X = 6;    =>    DEBUG_WATCH(int, X) = 6;    */
#ifndef DEBUG_WATCHED_H
#define DEBUG_WATCH_COUNTER __COUNTER__
debug_variable DebugWatchVariables[];
static unsigned int DebugWatchCount = 0;
static unsigned int DebugWatchArrayLen;

/* allows for the proper expansion */
#define DEBUG_WATCH_STR(x) #x

#define DEBUG_WATCH_DEF_INIT(type, name, ...)        DEBUG_WATCHED_DEF_INIT_(type, #name, name, __VA_ARGS__)
#define DEBUG_WATCHED_DEF_INIT(type, path, var, ...) DEBUG_WATCHED_DEF_INIT_(type, DEBUG_WATCH_STR(path ##_## var), var,  __VA_ARGS__)
#define DEBUG_WATCH_DEF_EQ(type, name, ...)          DEBUG_WATCHED_DEF_EQ_(  type, #name, name, __VA_ARGS__)
#define DEBUG_WATCHED_DEF_EQ(type, path, var, ...)   DEBUG_WATCHED_DEF_EQ_(  type, DEBUG_WATCH_STR(path ##_## var), var,  __VA_ARGS__)

/* sets the value once - on initialization */
#define DEBUG_WATCHED_DEF_INIT_(type, name, var, ...) \
if(! DEBUG_ATOMIC_EXCHANGE(&DebugWatch_## var ##_IsInit, 1)) { \
	debug_variable DebugWatch = { DebugVarType_## type, name, &var }; \
	type DebugWatchTemp_## var = __VA_ARGS__; \
	var = DebugWatchTemp_## var; \
	DebugWatchVariables[++DebugWatchCount] = DebugWatch; \
	DEBUG_WATCH_COUNTER; \
}
/* sets the value every time through */
#define DEBUG_WATCHED_DEF_EQ_(type, name, var, ...) \
{ type DebugWatchTemp_## var = __VA_ARGS__; var = DebugWatchTemp_## var; } \
if(! DEBUG_ATOMIC_EXCHANGE(&DebugWatch_## var ##_IsInit, 1)) { \
	debug_variable DebugWatch = { DebugVarType_## type, name, &var }; \
	DebugWatchVariables[++DebugWatchCount] = DebugWatch; \
	DEBUG_WATCH_COUNTER; \
}

/* separating declaration and definition allows use without declare-anywhere */
#define DEBUG_WATCH_DECL(type, name) static type name; static int DebugWatch_## name ##_IsInit
#define DEBUG_WATCH_DEF(type, name)        DEBUG_WATCHED_DEF_INIT(type, name, name, {0})
#define DEBUG_WATCHED_DEF(type, name, var) DEBUG_WATCHED_DEF_INIT(type, name, var,  {0})
	
/* typical use: */
#define DEBUG_WATCH(type, name)                   DEBUG_WATCH_DECL(type, name); DEBUG_WATCH_DEF(type, name) name
#define DEBUG_WATCHED(type, path, name)           DEBUG_WATCH_DECL(type, name); DEBUG_WATCHED_DEF(type, path ##_## name, name) name
#define DEBUG_WATCH_INIT(type, name, ...)         DEBUG_WATCH_DECL(type, name); DEBUG_WATCH_DEF_INIT(type, name, __VA_ARGS__)
#define DEBUG_WATCHED_INIT(type, path, name, ...) DEBUG_WATCH_DECL(type, name); DEBUG_WATCHED_DEF_INIT(type, path ##_## name, name, __VA_ARGS__)
#define DEBUG_WATCH_EQ(type, name, ...)           DEBUG_WATCH_DECL(type, name); DEBUG_WATCH_DEF_EQ(type, name, __VA_ARGS__)
#define DEBUG_WATCHED_EQ(type, path, name, ...)   DEBUG_WATCH_DECL(type, name); DEBUG_WATCHED_DEF_EQ(type, path ##_## name, name, __VA_ARGS__)
/* ^^^ suffixing with the name has the feature that it doesn't trigger a warning if unused elsewhere */

#define DEBUG_WATCH_DECLARATION  \
	enum { DebugWatchArrayLenEnum = DEBUG_WATCH_COUNTER + 1 }; \
	debug_variable DebugWatchVariables[DebugWatchArrayLenEnum]; \
	static unsigned int DebugWatchArrayLen = DebugWatchArrayLenEnum;
/* NOTE: 0 is free for invalid queries */
#define DEBUG_WATCHED_H
#endif/*DEBUG_WATCHED_H*/
/* END WATCHED **************************************************************/

/* TWEAK **********************************************************************/
/* TODO: should I include the function definitions above? */
#if defined(DEBUG_TWEAK_IMPLEMENTATION) && !defined(DEBUG_TWEAK_IMPLEMENTATION_H)
#ifndef DEBUG_TYPES
#error You have to #define DEBUG_TYPES! Do so in this format: `DEBUG_TYPE(type, printf_format_string)`
#endif

	/* DEBUG_TYPE(Unknown, "%d") \ */
#define DEBUG_TWEAK_INTERNAL_TYPES \
	DEBUG_TYPE(debug_if, "%u") \

#define DEBUG_TWEAK_TYPES \
	DEBUG_TWEAK_INTERNAL_TYPES \
	DEBUG_TYPES \

/* //// Debug variable type, name and pointer to value */
#if NO_VA_ARGS
#define DEBUG_TYPE_STRUCT(type, format, init) DEBUG_TYPE(type, format)
#else
#define DEBUG_TYPE_STRUCT(type, format, ...)  DEBUG_TYPE(type, format)
#endif/*NO_VA_ARGS */
typedef struct debug_variable
{
	enum {
#define DEBUG_TYPE(type, format) \
		DebugVarType_## type,
		DEBUG_TWEAK_TYPES
#undef DEBUG_TYPE
	} Type;
	char *Name;
	void *Data;
} debug_variable;
#undef DEBUG_TYPE_STRUCT

/* //// Reference debug array instances by name */
enum {
	DebugVarIndexEmpty,
#if NO_VA_ARGS
#define DEBUG_LIVE_TWEAK(type, Name, init) DebugVarIndex_## Name,
#else
#define DEBUG_LIVE_TWEAK(type, Name, ...)  DebugVarIndex_## Name,
#endif/*NO_VA_ARGS */
	DEBUG_LIVE_VARS
#undef DEBUG_LIVE_TWEAK
	DebugLiveCountPlusOne,
	DebugLiveCount = DebugLiveCountPlusOne - 2,
};


/* //// Global variables of the right type */
#if NO_VA_ARGS
#define DEBUG_LIVE_TWEAK(type, Name, init) DEBUG_VAR_DECLARE(type, Name) = init;
#else
#define DEBUG_LIVE_TWEAK(type, Name, ...)  DEBUG_VAR_DECLARE(type, Name) = __VA_ARGS__;
#endif/*!NO_VA_ARGS */
	DEBUG_LIVE_VARS
#undef DEBUG_LIVE_TWEAK

/* //// Array of annotated typed pointers to global vars */
debug_variable DebugTweakVariables[] = {
	{0},
#if NO_VA_ARGS
#define DEBUG_LIVE_TWEAK(type, Name, init) { DebugVarType_## type, #Name, (void *)&DEBUG_LIVE_VAR_## Name },
#else
#define DEBUG_LIVE_TWEAK(type, Name, ...)  { DebugVarType_## type, #Name, (void *)&DEBUG_LIVE_VAR_## Name },
#endif/*NO_VA_ARGS */
		DEBUG_LIVE_VARS
#undef DEBUG_LIVE_TWEAK
};

static int DebugLiveVar_IsCompiling;

/* returns the result of the system call (where 0 is success), or 0 if no action taken */
static int
DebugLiveVar_Recompile(char *BuildCall)
{
	int Result = 0;
	/* if(! DebugLiveVar_IsCompiling) */
	/* if(! (DebugLiveVar_IsCompiling == 1 ? 1 : DebugLiveVar_IsCompiling++)) */
	if(! DEBUG_ATOMIC_EXCHANGE(&DebugLiveVar_IsCompiling, 1))
	{
		/* DebugLiveVar_IsCompiling = 1; */
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
		for(u32 iVar = 0; iVar < ArrayCount(DebugTweakVariables); ++iVar)
		{
			debug_variable Var = DebugTweakVariables[iVar];
			switch(Var.Type)
			{
#if NO_VA_ARGS
#define DEBUG_TYPE_STRUCT(type, format, init) case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name,        init) > 0; break;
#else
#define DEBUG_TYPE_STRUCT(type, format, ...)  case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, __VA_ARGS__) > 0; break;
#endif/*NO_VA_ARGS*/
#define DEBUG_TYPE_MEMBER(struct, member) ((struct *)Var.Data)->member
#define DEBUG_TYPE(type, format)		case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, *(type *)Var.Data) > 0; break;
				DEBUG_TYPES
#undef DEBUG_TYPE
#undef DEBUG_TYPE_STRUCT
#undef DEBUG_TYPE_MEMBER
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
		Result &= fprintf(File, "#ifndef LIVE_VAR_CONFIG_H\n\n#define DEBUG_LIVE_VARS \\\n") > 0;

		for(u32 iVar = 1; iVar < ArrayCount(DebugTweakVariables); ++iVar)
		{
			debug_variable Var = DebugTweakVariables[iVar];
			switch(Var.Type)
			{
				case DebugVarType_debug_if: Result &= fprintf(File, "\tDEBUG_LIVE_IF(%s, %u) \\\n", Var.Name, *(debug_if *)Var.Data) > 0; break;

#define DEBUG_TYPE_MEMBER(struct, member) ((struct *)Var.Data)->member
#if NO_VA_ARGS
#define DEBUG_TYPE_STRUCT(type, format, init) case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_LIVE_TWEAK("#type", %s, "format") \\\n", Var.Name, init) > 0; break;
#else
#define DEBUG_TYPE_STRUCT(type, format, ...)  case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_LIVE_TWEAK("#type", %s, "format") \\\n", Var.Name, __VA_ARGS__) > 0; break;
#endif/*NO_VA_ARGS */
#define DEBUG_TYPE(type, format)              case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_LIVE_TWEAK("#type", %s, "format") \\\n", Var.Name, *(type *)Var.Data) > 0; break;
			DEBUG_TYPES
#undef DEBUG_TYPE
#undef DEBUG_TYPE_STRUCT
#undef DEBUG_TYPE_MEMBER
			}
		}
		Result &= fputs("\n\n#include \""__FILE__"\"\n#define LIVE_VAR_CONFIG_H\n#endif", File) >= 0;
		fclose(File);
	}
	return Result;
}
#define DEBUG_TWEAK_IMPLEMENTATION_H
#endif/*DEBUG_TWEAK_IMPLEMENTATION && !DEBUG_TWEAK_IMPLEMENTATION_H*/

#undef _
