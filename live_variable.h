/* NOTE: `#if NO_VA_ARGS == 0` is for e.g. C89, where __VA_ARGS__ is not available -> more work needed to make structs live
TODO:
 - better accommodate C89, e.g. with STRUCT2, STRUCT3 etc
 - add known types first time (like DEBUG_LIVE_IF)
 - add C basic types
 - add "Leave Empty" comment under macro lists
 - allow types to be specified elsewhere?
 - watch arrays
 - construct format strings from e.g. nested structs
*/

#ifndef LIVE_VARIABLE_H
#include <stdio.h>

/* #if !NO_VA_ARGS */
/* #define DEBUG_LIVE_TWEAKER DEBUG_TWEAK_NO_VA */
/* #else */
/* #define DEBUG_LIVE_TWEAKER DEBUG_TWEAK_VA */
/* #endif */

typedef struct debug_variable debug_variable;

typedef int debug_if;
#define DEBUG_VAR_DECLARE(type, Name) static type DEBUG_LIVE_VAR_## Name
#define DEBUG_LIVE_if(Name) if(DEBUG_LIVE_VAR_## Name)

// Declare the if statement variables
#define DEBUG_LIVE_IF(Name, init) DEBUG_VAR_DECLARE(debug_if, Name);
#if NO_VA_ARGS
#define DEBUG_LIVE_TWEAK(type, Name, init)
#else
#define DEBUG_LIVE_TWEAK(type, Name, ...)
#endif//NO_VA_ARGS
	DEBUG_LIVE_IF(Text_FlipVerticalDraw, 0)
	DEBUG_LIVE_IF(MiscTest_IfTester, 0)
	DEBUG_LIVE_IF(Rendering_SmallScreenBoundary, 0)
	DEBUG_LIVE_IF(Debug_VectorCrosshairThing, 0)
	DEBUG_LIVE_IF(Debug_PrintPointDetails, 0)
	DEBUG_LIVE_IF(Rendering_Render, 0)
	DEBUG_LIVE_TWEAK(f32, MiscTest_TestFloat, 999.000000f)
	DEBUG_LIVE_TWEAK(v2, MiscTest_TestV2, { 2381.000000f, 303.000000f })
	DEBUG_LIVE_TWEAK(v2, MiscTest_Test2V2, { 192.000000f, 694.000000f })
	/* DEBUG_LIVE_VARS */
#undef DEBUG_LIVE_TWEAK
#undef DEBUG_LIVE_IF
// from now on, treat ifs like other vars
#define DEBUG_LIVE_IF(Name, init) DEBUG_LIVE_TWEAK(debug_if, Name, init)

#define LIVE_VARIABLE_H
#endif//LIVE_VARIABLE_H

/* WATCHED ******************************************************************/
#ifndef DEBUG_WATCHED_H
#define DEBUG_WATCH_COUNTER __COUNTER__
debug_variable DebugWatchVariables[];
static unsigned int DebugWatchCount = 0;
static unsigned int DebugWatchArrayLen;

/* Usage:    int X = 6;    =>    DEBUG_WATCH(int, X) = 6;    */
/* TODO: do `if` and swap as atomic operation */
#define DEBUG_WATCH_INTERNALS(type, name, var) \
static int DebugWatch_## var ##_NeedsInit = 1; \
if(DebugWatch_## var ##_NeedsInit) { DEBUG_WATCH_COUNTER; \
	DebugWatch_## var ##_NeedsInit = 0; \
	debug_variable DebugWatch = { DebugVarType_## type, name, &var }; \
	DebugWatchVariables[++DebugWatchCount] = DebugWatch; \
}
#define DEBUG_WATCH(type, name)        static type name; { DEBUG_WATCH_INTERNALS(type, #name,         name) } name
#define DEBUG_WATCHED(type, path, name) static type name; { DEBUG_WATCH_INTERNALS(type, #path"_"#name, name) } name
/* ^^^ this has the feature that it doesn't trigger a warning if unused elsewhere */

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
/* #ifndef DEBUG_TWEAK_VARS */
/* #error You have to #define DEBUG_TWEAK_VARS! Do so in this format: `DEBUG_TWEAK_VAR(type, name)` */
/* #endif */

	/* DEBUG_TYPE(Unknown, "%d") \ */
#define DEBUG_TWEAK_INTERNAL_TYPES \
	DEBUG_TYPE(debug_if, "%u") \

#define DEBUG_TWEAK_TYPES \
	DEBUG_TWEAK_INTERNAL_TYPES \
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
		DEBUG_TWEAK_TYPES
#undef DEBUG_TYPE
	} Type;
	char *Name;
	union {
		void *Data; // are these below really needed?
#define DEBUG_TYPE(type, format) \
		type *Value_## type;
		DEBUG_TWEAK_TYPES
#undef DEBUG_TYPE
	};
} debug_variable;
#if !NO_VA_ARGS
#undef DEBUG_TYPE_STRUCT
#endif/*!NO_VA_ARGS */

/* //// Reference debug array instances by name */
enum {
	DebugLiveVarIndexEmpty,
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
debug_variable DebugLiveVariables[] = {
	{0},
#if NO_VA_ARGS
#define DEBUG_LIVE_TWEAK(type, Name, init) { DebugVarType_## type, #Name, (void *)&DEBUG_LIVE_VAR_## Name },
#else
#define DEBUG_LIVE_TWEAK(type, Name, ...)  { DebugVarType_## type, #Name, (void *)&DEBUG_LIVE_VAR_## Name },
#endif/*NO_VA_ARGS */
		DEBUG_LIVE_VARS
#undef DEBUG_LIVE_TWEAK
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
#define DEBUG_TYPE_MEMBER(struct, member) Var.Value_## struct->member
#define DEBUG_TYPE_STRUCT(type, format, ...) case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, __VA_ARGS__) > 0; break;
#endif/*! NO_VA_ARGS */
#define DEBUG_TYPE(type, format)		case DebugVarType_## type: Result &= fprintf(File, "#define DEBUGVAR_%s "format"\n", Var.Name, *Var.Value_## type) > 0; break;
				DEBUG_TYPES
#undef DEBUG_TYPE
#if ! NO_VA_ARGS
#undef DEBUG_TYPE_STRUCT
#undef DEBUG_TYPE_MEMBER
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
		Result &= fprintf(File, "#ifndef LIVE_VAR_CONFIG_H\n\n#define DEBUG_LIVE_VARS \\\n") > 0;

		for(u32 iVar = 1; iVar < ArrayCount(DebugLiveVariables); ++iVar)
		{
			debug_variable Var = DebugLiveVariables[iVar];
			switch(Var.Type)
			{
				case DebugVarType_debug_if: Result &= fprintf(File, "\tDEBUG_LIVE_IF(%s, %u) \\\n", Var.Name, *Var.Value_debug_if) > 0; break;

#define DEBUG_TYPE_MEMBER(struct, member) Var.Value_## struct->member
#if NO_VA_ARGS
#define DEBUG_TYPE_STRUCT(type, format, init) case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_TWEAK_STRUCT("#type", %s, "format") \\\n", Var.Name, init) > 0; break;
#else
#define DEBUG_TYPE_STRUCT(type, format, ...) case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_TWEAK_STRUCT("#type", %s, "format") \\\n", Var.Name, __VA_ARGS__) > 0; break;
#endif/*NO_VA_ARGS */
#define DEBUG_TYPE(type, format)             case DebugVarType_## type: Result &= fprintf(File, "\tDEBUG_TWEAK_VAR("#type", %s, "format") \\\n", Var.Name, *Var.Value_## type) > 0; break;
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

