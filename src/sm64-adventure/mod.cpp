#include "headers.h"

#include <stdio.h>

#include "../libsm64.h"
#include "audio_thread.h"

HMODULE multi = NULL;
HelperFunctions helperFuncs;

extern "C" {
	#include "../decomp/include/PR/ultratypes.h"
	#include "../decomp/include/audio_defines.h"

	__declspec(dllexport) void __cdecl Init(const char* path, const HelperFunctions& helperFunctions)
	{
		helperFuncs = helperFunctions;

		char fullFile[256] = {0};
		strcpy(fullFile, path);
		strcat(fullFile, "\\sm64.us.z64");
		FILE* f = fopen(fullFile, "rb");
		if (!f)
		{
			MessageBoxA(0, "Super Mario 64 US ROM not found\nPlace \"sm64.us.z64\" on the \"mods\\SM64 Adventure\" mod folder", "SM64 Adventure", 0);
			return;
		}

		fseek(f, 0, SEEK_END);
		size_t length = (size_t)ftell(f);
		rewind(f);
		uint8_t* rom = (uint8_t*)malloc(length + 1);
		fread(rom, 1, length, f);
		rom[length] = 0;
		fclose(f);

		uint8_t* texture = (uint8_t*)malloc(4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT);
		sm64_global_init(rom, texture);
		free(texture);

		sm64_audio_init(rom);
		audio_thread_init();

		sm64_play_sound_global(SOUND_MENU_STAR_SOUND);
	}

	__declspec(dllexport) void __cdecl OnFrame()
	{
		
	}

	__declspec(dllexport) ModInfo SADXModInfo = { ModLoaderVer };
}
