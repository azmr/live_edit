// TODO:
// - hash the names for faster comparison
// - add parent node separately from name

#ifndef DEBUG_HIERARCHY_H

// types:
// - live editable variables - local & config for values
// - profiling - added in functions
// - variable inspection/observation - local only
// - ...?

#ifdef DEBUG_HIERARCHY_MAX_LENGTH
#define DEBUG_HIERARCHY_COUNTER DEBUG_HIERARCHY_MAX_LENGTH
#else
#define DEBUG_HIERARCHY_COUNTER __COUNTER__
#endif
#ifndef DEBUG_ATOMIC_EXCHANGE /* user can define it to e.g. C11 atomic_exchange or Windows InterlockedExchange */
/* sets bool to 1, evaluates to previous value of bool */
#define DEBUG_ATOMIC_EXCHANGE(ptr, desired) (*(ptr) ? 1 : (*(ptr))++)
#endif

typedef struct debug_hierarchy_el
{
	enum {
		DebugHierarchyGroup,
#define DEBUG_HIERARCHY_KIND(type, kind) DebugHierarchyKind_## kind,
		DEBUG_HIERARCHY_KINDS
#undef  DEBUG_HIERARCHY_KIND
	} Kind; // reserve 0 for groups
	unsigned int NameLength;
	char *Name;
	struct debug_hierarchy_el *Parent;
	struct debug_hierarchy_el *FirstChild;
	struct debug_hierarchy_el *NextSibling;
	union {
		void *Data;
		int IsClosed;
#define DEBUG_HIERARCHY_KIND(type, kind) type *kind;
		DEBUG_HIERARCHY_KINDS
#undef  DEBUG_HIERARCHY_KIND
	};
} debug_hierarchy_el;


debug_hierarchy_el DebugHierarchy[];
static unsigned int DebugHierarchyCount = 0;
unsigned int DebugHierarchy_ArrayLen;

static int
DebugHierarchy_Level(debug_hierarchy_el El)
{
	int Result = 0;
	for(; El.Parent; El = *El.Parent, ++Result);
	return Result;
}

static int
DebugHierarchy_Equal(debug_hierarchy_el El1, debug_hierarchy_el El2)
{
	int Result = 0;
	if(El1.NameLength == El2.NameLength && El1.Parent == El2.Parent)// && Kind?
	{
		Result = 1;
		unsigned int At = El1.NameLength;
		while(At--)
		{
			if(El1.Name[At] != El2.Name[At])
			{ Result = 0; }
		}
	}
	return Result;
}

static debug_hierarchy_el *
DebugHierarchy_Next(debug_hierarchy_el *Item)
{
	debug_hierarchy_el *Result = 0;
	     if(Item->FirstChild && ! Item->IsClosed) { Result = Item->FirstChild; }
	else if(Item->NextSibling)                    { Result = Item->NextSibling; }
	else while(Item->Parent)
	{
		if(Item->Parent->NextSibling) { Result = Item->Parent->NextSibling; break; }
		else { Item = Item->Parent; }
	}
	return Result;
}

static void
DebugHierarchy_AppendChild(debug_hierarchy_el *Parent, debug_hierarchy_el *Appendee)
{
	Assert(Appendee);
	Assert(Parent);
	Appendee->Parent = Parent;
	debug_hierarchy_el *Child = Parent->FirstChild;
	if(Child)
	{
		int AppendeeAlreadyInHierarchy = DebugHierarchy_Equal(*Child, *Appendee);
		debug_hierarchy_el *NextChild = Child->NextSibling;
		while(NextChild && ! AppendeeAlreadyInHierarchy)
		{
			Child = NextChild;
			NextChild = Child->NextSibling;
			AppendeeAlreadyInHierarchy = DebugHierarchy_Equal(*Child, *Appendee);
		}
		if(AppendeeAlreadyInHierarchy) { return; }
		Child->NextSibling = Appendee;
	}
	else
	{
		Parent->FirstChild = Appendee;
	}
}

// returns 1 if new element was added, 0 if found an existing element
// puts pointer to element added (or existing equivalent element) in Location
static int
DebugHierarchy_AddUniqueEl(debug_hierarchy_el *Hierarchy, unsigned int *Count, unsigned int MaxLen,
                           debug_hierarchy_el El, debug_hierarchy_el **Location)
{
	int AlreadyAdded = 0;
	if(! El.Parent)               // if no real parent,
	{ El.Parent = Hierarchy; }    // set the parent to the root
	for(unsigned int i = 0, N = *Count; i <= N; ++i)
	{
		if(DebugHierarchy_Equal(El, Hierarchy[i]))
		{
			AlreadyAdded = 1;
			*Location = Hierarchy + i;
			break;
		}
	}

	if(! AlreadyAdded)
	{
		unsigned int iInsert = ++*Count;
		if(iInsert < MaxLen)
		{
			Hierarchy[iInsert] = El;
			*Location = Hierarchy + iInsert;
		}
		else
		{
			// TODO: handle error -> make the last element say that the array is full?
		}
	}
	Assert(*Location);
	int Result = ! AlreadyAdded;
	return Result;
}

static debug_hierarchy_el *
DebugHierarchy_InitElement(char *Name, void *Data, int Kind)
{
	char *LevelName = Name;
	debug_hierarchy_el Group = {0};
	for(char *At = Name; *At; ++At)
	{
		if(*At == '_' && Group.NameLength && At[1]) // not the end of the string or repeated '_'
		{
			Group.Name = LevelName;
			if(DebugHierarchy_AddUniqueEl(DebugHierarchy, &DebugHierarchyCount,
						DebugHierarchy_ArrayLen, Group, &Group.Parent))
			{ DebugHierarchy_AppendChild(Group.Parent->Parent, Group.Parent); }

			while(*++At == '_');
			LevelName = At;
			Group.NameLength = 1;
		}
		else
		{
			++Group.NameLength;
		}
	}

	Group.Data = Data;
	Group.Kind = Kind;
	Group.Name = LevelName;
	debug_hierarchy_el *Result = 0;
	DebugHierarchy_AddUniqueEl(DebugHierarchy, &DebugHierarchyCount,
	                           DebugHierarchy_ArrayLen, Group, &Result);
	DebugHierarchy_AppendChild(Result->Parent, Result);

	return Result;
}
#define DEBUG_HIERARCHY_INIT_EL(name, data, kind) \
	static int DebugHierarchy_## name ##_IsInit; \
	if(! DEBUG_ATOMIC_EXCHANGE(DebugHierarchy_## name ##_IsInit)) \
	{ DebugHierarchy_InitElement(#name, data, kind); DEBUG_HIERARCHY_COUNTER; }

#define DEBUG_HIERARCHY_DECLARATION  \
	enum { DebugHierarchy_ArrayLenEnum = 2*DEBUG_HIERARCHY_COUNTER + 1}; \
	debug_hierarchy_el DebugHierarchy[DebugHierarchy_ArrayLenEnum]; \
	unsigned int DebugHierarchy_ArrayLen = DebugHierarchy_ArrayLenEnum;

#define DEBUG_HIERARCHY_H
#endif//DEBUG_HIERARCHY_H
