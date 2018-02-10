#ifndef LOOP_EDIT_H

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
typedef struct win32_replay_buffer
{
	// TODO: use FILE?
	HANDLE FileHandle;
	HANDLE MemoryMap;
	char Filename[WIN32_STATE_FILE_NAME_COUNT];
	void *MemoryBlock;
} win32_replay_buffer;

typedef struct win32_loop_info
{
	win32_replay_buffer ReplayBuffers[4];

	HANDLE RecordingHandle;
	int InputRecordingIndex;

	HANDLE PlaybackHandle;
	int InputPlayingIndex;

	// TODO: should I move this out of the struct? It's somewhat separate...
	exe_info EXE;
} win32_loop_info;

// TODO: allow looping input without recording/resetting state
internal void
Win32GetInputFileLocation(win32_loop_info *LoopInfo, b32 InputStream, int SlotIndex, int DestCount, char *Dest)
{
	char Temp[64];
	wsprintf(Temp, "loop_%d_%s.mem", SlotIndex, InputStream ? "input" : "state");
	Win32BuildPathFilename(LoopInfo->EXE.PathName, LoopInfo->EXE.Filename_FromPath,
						   Temp, DestCount, Dest); 
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_loop_info *LoopInfo, int unsigned Index)
{
	Assert(Index > 0);
	Assert(Index < ArrayCount(LoopInfo->ReplayBuffers));
	win32_replay_buffer *Result = &LoopInfo->ReplayBuffers[Index];
	return Result;
}

internal void
Win32BeginRecordingInput(win32_loop_info *LoopInfo, int InputRecordingIndex, program_memory Memory)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(LoopInfo, InputRecordingIndex);
	if(ReplayBuffer->MemoryBlock)
	{
		LoopInfo->InputRecordingIndex = InputRecordingIndex;

		char Filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(LoopInfo, 1, InputRecordingIndex, sizeof(Filename), Filename);
		LoopInfo->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 

#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = LoopInfo->TotalSize;
		SetFilePointerEx(LoopInfo->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif

		CopyMemory(ReplayBuffer->MemoryBlock, Memory.MemoryBlock, Memory.TotalSize);
	}
}

internal void
Win32EndRecordingInput(win32_loop_info *LoopInfo)
{
	CloseHandle(LoopInfo->RecordingHandle);
	LoopInfo->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_loop_info *LoopInfo, int InputPlayingIndex, program_memory Memory)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(LoopInfo, InputPlayingIndex);
	if(ReplayBuffer->MemoryBlock)
	{
		LoopInfo->InputPlayingIndex = InputPlayingIndex;

		char Filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(LoopInfo, 1, InputPlayingIndex, sizeof(Filename), Filename);
		LoopInfo->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = LoopInfo->TotalSize;
		SetFilePointerEx(LoopInfo->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif

		CopyMemory(Memory.MemoryBlock, ReplayBuffer->MemoryBlock, Memory.TotalSize);
	}
}

internal void
Win32EndInputPlayback(win32_loop_info *LoopInfo)
{
	CloseHandle(LoopInfo->RecordingHandle);
	LoopInfo->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_loop_info *LoopInfo, input *NewInput)
{
	DWORD BytesWritten;
	WriteFile(LoopInfo->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_loop_info *LoopInfo, input *NewInput, program_memory Memory)
{
	DWORD BytesRead = 0;
	if(ReadFile(LoopInfo->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
	{
		if(BytesRead == 0)
		{
			// NOTE: We've hit the end of the stream, go back to the beginning
			int PlayingIndex = LoopInfo->InputPlayingIndex;
			Win32EndInputPlayback(LoopInfo);
			Win32BeginInputPlayback(LoopInfo, PlayingIndex, Memory);
			ReadFile(LoopInfo->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
		}
	}
}

internal void
Win32SingleInputLoopControl(win32_loop_info *LoopInfo, int SlotIndex, program_memory Memory)
{
	if(LoopInfo->InputPlayingIndex == 0)
	{
		if(LoopInfo->InputRecordingIndex == 0)
		{
			Win32BeginRecordingInput(LoopInfo, SlotIndex, Memory);
		}
		else
		{
			Win32EndRecordingInput(LoopInfo);
			Win32BeginInputPlayback(LoopInfo, SlotIndex, Memory);
		}
	}
	else
	{
		Win32EndInputPlayback(LoopInfo);
	}
}

// TODO: allow any input struct (void *Input, size_t sizeofInput)
internal void
Win32RecordOrPlayBackInput(win32_loop_info *LoopInfo, input *Input, program_memory Memory)
{
	if(LoopInfo->InputRecordingIndex)
	{
		Win32RecordInput(LoopInfo, Input);
	}
	// TODO: check HMH... should be else if?
	if(LoopInfo->InputPlayingIndex)
	{
		Win32PlayBackInput(LoopInfo, Input, Memory);
	}
}

internal void
Win32InitReplayBuffers(win32_loop_info *LoopInfo, u64 MemorySize)
{
	for(int ReplayIndex = 1;
			ReplayIndex < ArrayCount(LoopInfo->ReplayBuffers);
			++ReplayIndex)
	{
		win32_replay_buffer *ReplayBuffer = &LoopInfo->ReplayBuffers[ReplayIndex];
		// TODO: Recording system still seems to take too long
		// on record start - find out what Windows is doing and if
		// we can speed up / defer some of that processing.

		Win32GetInputFileLocation(LoopInfo, 0, ReplayIndex,
				sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);

		ReplayBuffer->FileHandle =
			CreateFileA(ReplayBuffer->Filename,
					GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);

		LARGE_INTEGER MaxSize;
		MaxSize.QuadPart = MemorySize;
		ReplayBuffer->MemoryMap = CreateFileMapping(
				ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
				MaxSize.HighPart, MaxSize.LowPart, 0);

		ReplayBuffer->MemoryBlock = MapViewOfFile(
				ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, MemorySize);

		if(ReplayBuffer->MemoryBlock)
		{

		}
		else
		{
			// TODO: Diagnostic
		}
	}
}

internal win32_loop_info
Win32InitLoop(size_t MemorySize)
{
	win32_loop_info LoopInfo = {0};
	Win32GetEXEFilename(LoopInfo.EXE.PathName, sizeof(LoopInfo.EXE.PathName), &LoopInfo.EXE.Filename_FromPath);
	Win32InitReplayBuffers(&LoopInfo, MemorySize);

	return LoopInfo;
}
#define LOOP_EDIT_H
#endif
