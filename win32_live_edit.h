#ifndef WIN32_LIVE_EDIT_H

typedef void void_func();
typedef int b32;
typedef unsigned int uint;

typedef struct library_function
{
	char *Name;
	void_func *Function;
} library_function;

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
typedef struct exe_info
{
	char PathName[WIN32_STATE_FILE_NAME_COUNT];
	char *Filename_FromPath; // Points to the char after the last \ in the pathname
} exe_info;

typedef struct lib_filepaths
{
	char SourceDLL[WIN32_STATE_FILE_NAME_COUNT];
	char TempDLL[WIN32_STATE_FILE_NAME_COUNT];
	char Lock[WIN32_STATE_FILE_NAME_COUNT];
} lib_filepaths;

typedef struct win32_library
{
	HMODULE CodeDLL;
	FILETIME DLLLastWriteTime;

	// IMPORTANT: callbacks can be 0! Check before calling.
	uint NumFunctions;
	library_function *Functions;

	lib_filepaths Paths;
	exe_info EXE;

	b32 IsValid;
} win32_library;

#ifdef LIVE_EDIT_STANDALONE
static void
CatStrings(size_t SourceACount, char *SourceA,
		   size_t SourceBCount, char *SourceB,
		   size_t DestCount, char *Dest)
{
	// TODO: Dest bounds checking!
	DestCount;
	for(int Index = 0;
			Index < SourceACount;
			++Index)
	{
		*Dest++ = *SourceA++;
	}

	for(int Index = 0;
			Index < SourceBCount;
			++Index)
	{
		*Dest++ = *SourceB++;
	}

	*Dest++ = 0;
}

static int
StringCopy(char *Dest, char *Src)
{
	int Count = 0;
	while(*Src)
	{
		*Dest++ = *Src++;
		++Count;
	}
	return Count;
}

static int
StringLength(char *String)
{
	int Count = 0;
	while(*String++) { ++Count; }
	return Count;
}

/// Finds file with name in same path as String
// "DirNameFromPath"?
static void
Win32BuildPathFilename(char *String, char *LastChar, char *Filename,
						  int DestCount, char *Dest)
{
	size_t PathLength = LastChar - String;
	CatStrings(PathLength, String,
			   StringLength(Filename), Filename,
			   DestCount, Dest);
}

/// Fills String with name of currently running executable
// TODO: GetEXEInfo? return exe_info
static void
Win32GetEXEFilename(char *PathNameBuf, int StringLen, char **Filename)
{
	// NOTE: Never use MAX_PATH in code that is user-facing, because it
	// can be dangerous and lead to bad results.
	//DWORD SizeOfFilename =
		GetModuleFileNameA(0, PathNameBuf, StringLen);
	*Filename = PathNameBuf;
	for(char *Scan = PathNameBuf;
			*Scan;
			++Scan)
	{
		if(*Scan == '\\')
		{
			*Filename = Scan + 1;
		}
	}
}

static inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
	FILETIME LastWriteTime = {0};

	WIN32_FILE_ATTRIBUTE_DATA Data;
	if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
	{
		LastWriteTime = Data.ftLastWriteTime;
	}

	return LastWriteTime;
}
#endif // LIVE_EDIT_STANDALONE

/// 1 for success, 0 for failure
static b32
Win32LoadLib(win32_library *Lib)
{
	WIN32_FILE_ATTRIBUTE_DATA Ignored;
	if(!GetFileAttributesEx(Lib->Paths.Lock, GetFileExInfoStandard, &Ignored))
	{
		Lib->DLLLastWriteTime = Win32GetLastWriteTime(Lib->Paths.SourceDLL);

		CopyFileA(Lib->Paths.SourceDLL, Lib->Paths.TempDLL, FALSE);

		Lib->CodeDLL = LoadLibraryA(Lib->Paths.TempDLL);
		if(Lib->CodeDLL)
		{
			Lib->IsValid = 1;
			for(uint FuncI = 0; FuncI < Lib->NumFunctions; ++FuncI)
			{
				Lib->Functions[FuncI].Function = (void_func *)
					GetProcAddress(Lib->CodeDLL, Lib->Functions[FuncI].Name);

			if(!Lib->Functions[FuncI].Function) Lib->IsValid = 0;
			}
		}
	}

	if(!Lib->IsValid)
	{
		// TODO: invalidate each function individually? does that even make sense?
		/* for(uint FuncI = 0; FuncI < Lib->NumFunctions; ++FuncI) */
		/* { */
		/* 	Lib->Functions[FuncI].Function = 0; */
		/* } */
		return 0;
	}

	return 1;
}
static void
Win32UnloadLib(win32_library *Lib)
{
	if(Lib->CodeDLL)
	{
		FreeLibrary(Lib->CodeDLL);
		Lib->CodeDLL = 0;
	}

	Lib->IsValid = 0;
	for(uint FuncI = 0; FuncI < Lib->NumFunctions; ++FuncI)
	{
		Lib->Functions[FuncI].Function = 0;
	}
}

static void
Win32GetLibraryPaths(win32_library *Lib, char *SourceDLL, char *TempDLL, char *Lock)
{
	Win32BuildPathFilename(Lib->EXE.PathName, Lib->EXE.Filename_FromPath, SourceDLL,
			     		  sizeof(Lib->Paths.SourceDLL), Lib->Paths.SourceDLL);
	Win32BuildPathFilename(Lib->EXE.PathName, Lib->EXE.Filename_FromPath, TempDLL,
			     		  sizeof(Lib->Paths.TempDLL), Lib->Paths.TempDLL);
	Win32BuildPathFilename(Lib->EXE.PathName, Lib->EXE.Filename_FromPath, Lock,
							  sizeof(Lib->Paths.Lock), Lib->Paths.Lock);
}

static b32
Win32ReloadLibOnRecompile(win32_library *Lib)
{
	b32 LibLoaded = 0;
	FILETIME NewDLLWriteTime = Win32GetLastWriteTime(Lib->Paths.SourceDLL);
	WIN32_FILE_ATTRIBUTE_DATA IgnoredFileData;
	// See if file has been changed and the lockfile has been removed
	if(CompareFileTime(&NewDLLWriteTime, &Lib->DLLLastWriteTime) != 0 &&
		!GetFileAttributesEx(Lib->Paths.Lock, GetFileExInfoStandard, &IgnoredFileData))
	{
		Win32UnloadLib(Lib);
		LibLoaded = Win32LoadLib(Lib);
		Assert(LibLoaded);
	}
	return LibLoaded;
}

static win32_library
Win32LibraryFunctions(char **FnNames, int NumFns)
{
	win32_library Lib = {0};
	Lib.Functions = VirtualAlloc(0, NumFns * sizeof(*Lib.Functions), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	/* Lib.Functions = calloc(NumFns * sizeof(*Lib.Functions)); */
	for(int FuncI = 0; FuncI < NumFns; ++FuncI)
	{ Lib.Functions[FuncI].Name = FnNames[FuncI]; }
	return Lib;
}

static win32_library
Win32Library(char **FnNames, int NumFns,
			b32 CompletePaths, char *SourcePath, char *TempPath, char *LockPath)
{
	win32_library Lib = Win32LibraryFunctions(FnNames, NumFns);
	Lib.NumFunctions = NumFns;
	Win32GetEXEFilename(Lib.EXE.PathName, sizeof(Lib.EXE.PathName), &Lib.EXE.Filename_FromPath);

	if(CompletePaths)
	{
		StringCopy(Lib.Paths.SourceDLL, SourcePath);
		StringCopy(Lib.Paths.TempDLL, TempPath);
		StringCopy(Lib.Paths.Lock, LockPath);
	}
	else
	{
		Win32GetLibraryPaths(&Lib, SourcePath, TempPath, LockPath);
	}

	return Lib;
}

#define WIN32_LIVE_EDIT_H
#endif//WIN32_LIVE_EDIT_H
