/* NOTE:
 *  - path => C:\Documents\my_file.txt
 *  - dir  => C:\Documents\
 *  - name => myfile.txt
 */

#ifndef WIN32_LIVE_EDIT_H
#include <assert.h>

typedef void VoidFn();

typedef struct LiveEditLibSym
{
	char *name;
	union {
		VoidFn *function;
		void *data;
	}; // symbol;
} LiveEditLibSym;


typedef enum LiveEditPathType {
	LIVE_EDIT_REL_PATH,
	LIVE_EDIT_ABS_PATH,
} LiveEditPathType;

// NOTE: Never use MAX_PATH in code that is user-facing, because it
// can be dangerous and lead to bad results.
#define LIVE_EDIT_MAX_PATH MAX_PATH

typedef struct LiveEditLib
{
	HMODULE dll;
	FILETIME dll_last_write_time;

	// IMPORTANT: callbacks can be 0! Check before calling.
	unsigned int syms_n;
	LiveEditLibSym *syms;

	struct {
		char dll[LIVE_EDIT_MAX_PATH];
		char lock[LIVE_EDIT_MAX_PATH];
	} paths;

	int is_valid;
} LiveEditLib;

// concatenates dir with name to make path
static void
Win32PathFromDirName(char *dir_str, size_t dir_str_len, char *name_str, char *path_buf, size_t path_buf_len)
{
	size_t Count = 0;
	for(size_t i = 0; i < dir_str_len && Count++ <= path_buf_len; ++i)
	{ *path_buf++ = *dir_str++; }

	while(*name_str && Count++ <= path_buf_len)
	{ *path_buf++ = *name_str++; }

	assert(Count < path_buf_len);
	*path_buf = 0;
}

static inline FILETIME
le__Get_last_write_time(char *Filename)
{
	FILETIME LastWriteTime = {0};

	WIN32_FILE_ATTRIBUTE_DATA Data;
	if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
	{ LastWriteTime = Data.ftLastWriteTime; }

	return LastWriteTime;
}

static inline int
le__file_is_present(char *file_path)
{
	WIN32_FILE_ATTRIBUTE_DATA Ignored;
	return file_path && GetFileAttributesEx(file_path, GetFileExInfoStandard, &Ignored);
}

/// 1 for success, 0 for failure
static int
Win32LoadLib(LiveEditLib *lib)
{
	if (! le__file_is_present(lib->paths.lock)) // don't try and load while the lock file is there
	{
		lib->dll_last_write_time = le__Get_last_write_time(lib->paths.dll);

		lib->dll = LoadLibrary(lib->paths.dll);
		// NOTE: if this fails, you may be trying to load an invalid path
		if (! lib->dll) {
			int err = GetLastError(); err;
			assert(0);
		}

		lib->is_valid = 1;
		for(unsigned int sym_i = 0; sym_i < lib->syms_n; ++sym_i)
		{
			lib->syms[sym_i].function = GetProcAddress(lib->dll, lib->syms[sym_i].name);

			// NOTE: if this fails, check you are exporting the function from the dll
			assert(lib->syms[sym_i].function); //{   lib->is_valid = 0;   }
		}
	}

	assert(lib->is_valid);
	return !! lib->is_valid;
}

static void
Win32UnloadLib(LiveEditLib *lib)
{
	if (lib->dll)
	{   FreeLibrary(lib->dll);   }

	for(unsigned int sym_i = 0; sym_i < lib->syms_n; ++sym_i)
	{   lib->syms[sym_i].function = 0;   }

	lib->is_valid = 0;
	lib->dll      = 0;
}

static int
Win32ReloadLibOnRecompile(LiveEditLib *lib)
{
	int LibLoaded = 0;
	FILETIME new_dll_write_time = le__Get_last_write_time(lib->paths.dll);
	int dll_has_been_changed    = CompareFileTime(&new_dll_write_time, &lib->dll_last_write_time) != 0;
	int lock_has_been_removed   = ! le__file_is_present(lib->paths.lock);
	// See if file has been changed and the lockfile has been removed
	if (dll_has_been_changed && lock_has_been_removed)
	{
		Win32UnloadLib(lib);
		LibLoaded = Win32LoadLib(lib);
	}
	return LibLoaded;
}

static void le__strcpy(char *Dest, char *Src) { while(*Src) { *Dest++ = *Src++; } }

static LiveEditLib
Win32Library(LiveEditLibSym *syms, unsigned int syms_n, LiveEditPathType path_type,
			char *dll_path, char *lock_path)
{
	LiveEditLib lib = {0};
	lib.syms_n = syms_n;
	lib.syms = syms;

	assert(dll_path);
	if (path_type == LIVE_EDIT_ABS_PATH)
	{
		le__strcpy(lib.paths.dll, dll_path);
		if (lock_path)
		{   le__strcpy(lib.paths.lock, lock_path);   }
	}

	else
	{ // construct utility paths from names and exe path
		char exe_path[LIVE_EDIT_MAX_PATH];
		char *exe_name_from_path = exe_path; // Points to the char after the last \ in exe_path
		unsigned int path_str_len = 0;
		{ // Get name and path for currently running exe
			path_str_len = GetModuleFileName(0, exe_path, sizeof(exe_path));
			assert(path_str_len);
			if (path_str_len == sizeof(exe_path)) // maybe perfectly filled, or maybe truncated
			{   assert(GetLastError() != ERROR_INSUFFICIENT_BUFFER);   }

			for(char *at = exe_path; *at; ++at)
			{
				if (*at == '\\')
				{ exe_name_from_path = at + 1; } // this may be '\0' (an empty string), which is intentional
			}
		}
		size_t dir_str_len = exe_name_from_path - exe_path;

		Win32PathFromDirName(exe_path, dir_str_len, dll_path, lib.paths.dll, sizeof(lib.paths.dll));
		if (lock_path)
		{   Win32PathFromDirName(exe_path, dir_str_len, lock_path, lib.paths.lock, sizeof(lib.paths.lock));   }
	}

	return lib;
}

#define WIN32_LIVE_EDIT_H
#endif//WIN32_LIVE_EDIT_H
