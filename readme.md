# Live Edit
_A collection of single-header libraries to help with reloading, debugging and profiling C(++) code._

## Contents
- [Acknowledgments](#acknowledgments)
- [Overview of headers](#overview-of-headers)
- [win32_live_edit.h](#win32_live_edith)
- [win32_loop_edit.h](#win32_loop_edith)
- [live_variable.h](#live_variableh)
    - [DEBUG_WATCH](#debug_watch)
    - [DEBUG_TWEAK](#debug_tweak)
- [hierarchy.h](#hierarchyh)
- [Recommended libraries](#recommended-libraries)

## Acknowledgments
Most of the content here used Casey Muratori's [Handmade Hero](https://www.handmadehero.org) as a starting point.
It is an invaluable resource for those looking to understand and write low-level code.
Go and watch a [few episodes](https://hero.handmade.network/episode/code) (lovingly annotated by Miblo),
maybe chip in a few quid/dollars, then come back here. I'll wait.

I've put some relevant links in each section.
My implementations have diverged somewhat, but the concepts are mostly the same.

One major difference is that these will work with C.
(They're not all C89-compatible, as some of the implementations require declare-anywhere and/or `__VA_ARGS__`.
Some aren't C89-compatible just because I haven't taken the time to make them so.
File an issue or a pull request if you want this changed. Ditto for C++ incompatibilities.)

## Overview of headers
All of these can be used standalone, but they also work well together.
- `win32_live_edit.h` - reloading DLL code while running (requires a platform and client layer, see video linked above). Currently Windows only.
- `win32_loop_edit.h` - (needs updating) - capture input for a set period of time, reset the system state to the start, and loop over, e.g. for tweaking small changes in timing-sensitive code. Currently Windows only.
- `live_variable.h` - introducing `DEBUG_WATCH()` and `DEBUG_TWEAK()` for watching and tweaking variables (surprisingly!) from the application. Also `DEBUG_LIVE_IF()`, useful for toggling code, similar to `#if 0` blocks.
- `hierarchy.h` - used for sorting arbitrary debug data into a hierarchy/tree structure.
*TODO: add`profiling.h`*

Note: if you use these, please keep a copy and don't pull updates blindly. Assume every update may contain breaking API changes.

## `win32_live_edit.h`
Casey's version:
[Day 021: Loading Game Code Dynamically](https://hero.handmade.network/episode/code/day021/);
[Day 022: Instantaneous Live Code Editing](https://hero.handmade.network/episode/code/day022/)

``` c
// "main.h" -> #included in both the following files
typedef void update_and_render(image_buffer Buffer, memory *Memory);
/***/

// "win32_main.c"
#include "path/to/win32_live_edit.h"

// Once, on startup:
#if !SINGLE_EXECUTABLE    // not needed, but used in a few other places as well,
                          // you can swap between live editing internally and a
                          // single executable for release by changing one number
    char *LibFnNames[] = { "UpdateAndRender" };
    win32_library Lib = Win32Library(LibFnNames, ArrayCount(LibFnNames),
                                     0, "main.dll", "main_temp.dll", "lock.tmp");
#endif // !SINGLE_EXECUTABLE

// later, once every so often (e.g. once per frame)
#if !SINGLE_EXECUTABLE
    Win32ReloadLibOnRecompile(&Lib); 
    update_and_render *UpdateAndRender = ((update_and_render *)Lib.Functions[0].Function);
    // Not much point in continuing if key function failed to load:
    Assert(UpdateAndRender && "loaded library function.");
#endif // !SINGLE_EXECUTABLE
/***/

// "main.c" -> compiled/linked as DLL
void UpdateAndRender(image_buffer Buffer, memory *Memory) {
    // Use Memory rather than global variables, as any pointers to them will be invalidated on recompile (roughly)
    if( ! Memory->IsInitialized) {
        // init stuff on first runthrough
        Memory->IsInitialized = 1;
    }
    // ...
}
```

## `win32_loop_edit.h`
Casey's Version:
[Day 023: Looped Live Code Editing](https://hero.handmade.network/episode/code/day023/)

*TODO: ensure working & explain`*

## `live_variable.h`
Casey experiments with multiple implementations doing something vaguely similar to this between
[Day 192: Implementing Self-Recompilation](https://hero.handmade.network/episode/code/day192/) and
[Day 255: Building a Profile Tree](https://hero.handmade.network/episode/code/day255/).
I think most of the stuff similar to this is in the 212-214 range.

### `DEBUG_WATCH`
WATCH is there to minimize the friction involved with inspecting the state of the system.
``` c
// "main.c"
#define DEBUG_TYPES \
    DEBUG_TYPE(f32, "%ff") \
    DEBUG_TYPE(v2, "{ %ff, %ff }", DEBUG_MEMBER(v2, X), DEBUG_MEMBER(v2, Y))

// Only need to define this if the values being watched/tweaked are declared in many threads
// as far as I'm aware, most platforms have an atomic exchange function. This is from C11:
#define DEBUG_ATOMIC_EXCHANGE(ValuePtr, Desired) atomic_exchange(ValuePtr, Desired)
#include "path/to/live_variable.h"    // as long as the types are defined first
// ...

DEBUG_WATCH(f32, X) = 6.2f;    // previously: `f32 X = 6.2f;`
// Tag the variable with a path without affecting its local name (mostly for interaction with `hierarchy.h`):
DEBUG_WATCHED(v2, Category_Name, Vec) = { 1.f, 0.f };
// ...

for(unsigned int i = 1; i <= DebugWatchCount; ++i) {
    debug_variable Var = DebugWatchVariables[i];
    switch(Var->Type) { // you can do fancy things with the DEBUG_TYPES macro, but let's K.I.S.S. here
        case DebugVarType_f32: printf("%f\n", *(f32 *)Var.Data); break;

        case DebugVarType_v2: {
            v2 V = *(v2 *)Var.Data;
            printf("%f, %f\n", V.X, V.Y);
        } break;
    }
}

// ... after the final function that uses DEBUG_WATCH. (In the global space)
DEBUG_WATCH_DECLARATION
```

### `DEBUG_TWEAK`
TWEAK is a bit more involved than WATCH, but you get a bit more from it: you can edit values and save them between compiles. This is primarily for toggling code sections and tweaking values.
``` c
// "main.c"
#define DEBUG_TYPES \
    DEBUG_TYPE(bool32, "%d") \
    DEBUG_TYPE(f32, "%ff") \
    DEBUG_TYPE_STRUCT(v2, "{ %ff, %ff }", DEBUG_MEMBER(v2, X), DEBUG_MEMBER(v2, Y))

// (see above)
#define DEBUG_ATOMIC_EXCHANGE(ValuePtr, Desired) atomic_exchange(ValuePtr, Desired)
#include "main_live.h"
/***/

// "main_live.h" -> this file should be overwritten when a value changes (as below),
// using the format specifiers above
#define DEBUG_LIVE_VARS \
    DEBUG_LIVE_IF(RenderBadly, 0) \
    DEBUG_LIVE_TWEAK(f32, TestFloat, 999.000000f) \
    DEBUG_LIVE_TWEAK(v2, TestV2, { 2381.000000f, 303.450000f }) \
    /* important empty space here */
#include "path/to/live_variable.h"
/***/

// "main.c"
// important type definitions...
#define DEBUG_TWEAK_IMPLEMENTATION
#include "path/to/live_variable.h"
// ...

DEBUG_LIVE_if(RenderBadly) { // ... lots of things } // use just like normal if statement
else { RenderWell(); }
printf("%f\n", TestFloat);
v2 NewVector = V2Add(SomeV2, TestV2);
//...
for(unsigned int i = 0; i <= 9 && i <= DebugLiveCount; ++i) {
    if(WasPressed(Numpad[i])) {
        debug_variable Var = &DebugLiveVariables[i+1];
        switch(Var->Type) {
            //...
        }
        ChangesMade = 1;
    }
}
if(ChangesMade) DebugLiveVar_RewriteConfig("path/to/main_live.h");

```

## `hierarchy.h`
Casey does implement a hierarchy (e.g. [Day 194: Organizing Debug Variables into a Hierarchy](https://hero.handmade.network/episode/code/day194/)),
but if I remember right it's fairly dependent on its context.
I don't think I referenced it when writing my own.

``` c
#define DEBUG_HIERARCHY_KINDS \
    DEBUG_HIERARCHY_KIND(debug_variable, Tweak) \
    DEBUG_HIERARCHY_KIND(debug_variable, Watch)

// See explanation under DEBUG_WATCH for DEBUG_ATOMIC_EXCHANGE.
// Also, only needed if using the macros, otherwise thread safety is your responsibility!
#define DEBUG_ATOMIC_EXCHANGE(ValuePtr, Desired) atomic_exchange(ValuePtr, Desired)
#include "path/to/hierarchy.h"
//...

for(unsigned int i = 1; i <= DebugTweakCount; ++i) {
    debug_variable *Var = &DebugTweakVariables[i];
    DebugHierarchy_InitElement(Var->Name, Var, DebugHierarchyKind_Tweak);
}
for(unsigned int i = 1; i <= DebugWatchCount; ++i) {
    debug_variable *Var = &DebugWatchVariables[i];
    DebugHierarchy_InitElement(Var->Name, Var, DebugHierarchyKind_Watch);
}

for(debug_hierarchy_el *HVar = DebugHierarchy_Next(DebugHierarchy);
    HVar;
    HVar = DebugHierarchy_Next(HVar))
{
    // indent variables by same amount
    for(int Indent = DebugHierarchy_Level(*HVar); Indent; --Indent) {
        puts("    ");
    }

    switch(HVar->Kind) {
        case DebugHierarchyGroup: {
            printf("%.*s\n", HVar->NameLength, HVar->Name);
            if(SomeInputHappened) {
                HVar->IsClosed = ! HVar->IsClosed;    // collapse and expand group
            }
        } break;

        case DebugHierarchyKind_Tweak: {
            debug_variable *Var = (debug_variable *)HVar.Data;
            InteractWithDebugVariable(Input, Var);
            WatchDebugVariable(OutputFormat, Var);
        } break;

        case DebugHierarchyKind_Watch: {
            debug_variable *Var = (debug_variable *)HVar.Data;
            WatchDebugVariable(OutputFormat, Var);
        } break;
    }
}

DEBUG_HIERARCHY_DECLARATION
```

## Recommended libraries
- All of the [STB single file libraries](https://github.com/nothings/stb) (he recommends others there as well)
- My single file testing library - [`sweet.h`](https://github.com/azmr/sweet)
- My single file header fonts - _Coming soon_
