﻿#include "Pomme.h"
#include "GamePatches.h"
#include "PommeFiles.h"
#include <iostream>
#include <fstream>
#include <SDL.h>

fs::path DoOpenDialog(const char* expectedArchiveName);

static fs::path gDataLocation;

#ifdef PRO_MODE
	constexpr const char* ARCHIVE_NAME = "nanosaurextreme134.bin";
	constexpr const char8_t* APP_FILE_INSIDE_ARCHIVE = u8":Nanosaur\u2122 Extreme";
#else
	constexpr const char* ARCHIVE_NAME = "nanosaur134.bin";
	constexpr const char8_t* APP_FILE_INSIDE_ARCHIVE = u8":Nanosaur\u2122";
#endif

constexpr const char* DATA_LOCATION_PREF = "DataLocation";

extern "C"
{
	extern SDL_Window* gSDLWindow;
	extern FSSpec gDataSpec;
}

void DrawLocatePromptScreen()
{
	ExclusiveOpenGLMode_Begin();
	
	ClearBackdrop(0xFF000000);
	BackColor(blackColor);
	MoveTo(96, 96);
	ForeColor(whiteColor);
	DrawStringC("Please locate either \"");
	ForeColor(redColor);
	DrawStringC(ARCHIVE_NAME);
	ForeColor(whiteColor);
	DrawStringC("\",");
	MoveTo(96, 96+16);
	DrawStringC("or the \"");
	ForeColor(greenColor);
	DrawStringC("Nanosaur");
	ForeColor(whiteColor);
	DrawStringC("\" Classic app inside the game's disk image.");
	RenderBackdropQuad(BACKDROP_FILL);
	
	ExclusiveOpenGLMode_End();
}

fs::path ReadDataLocationSetting()
{
	FSSpec spec;
	short refNum;
	long length;
	
	MakePrefsFSSpec(DATA_LOCATION_PREF, &spec);

	if (noErr != FSpOpenDF(&spec, fsRdPerm, &refNum)
		|| noErr != GetEOF(refNum, &length))
	{
		return "";
	}
	
	char buf[2048];
	length = std::min((long)sizeof(buf), length);
	FSRead(refNum, &length, buf);
	FSClose(refNum);

	return std::u8string(buf, buf+length);
}

void NukeDataLocationSetting()
{
	FSSpec spec;

	MakePrefsFSSpec(DATA_LOCATION_PREF, &spec);
	FSpDelete(&spec);
}

void WriteDataLocationSetting()
{
	FSSpec spec;
	short refNum;
	long length;
	
	MakePrefsFSSpec(DATA_LOCATION_PREF, &spec);
	FSpDelete(&spec);

	if (noErr != FSpCreate(&spec, 'NanO', 'path', smSystemScript)) {
		return;
	}
	
	if (noErr != FSpOpenDF(&spec, fsWrPerm, &refNum)) {
		FSpDelete(&spec);
		return;
	}

	auto u8location = gDataLocation.u8string();
	length = u8location.size();
	FSWrite(refNum, &length, (const Ptr)u8location.data());
	FSClose(refNum);
}

enum FindGameData_Outcome
{
	OK,
	RETRY,
	ABORT
};

static FindGameData_Outcome _FindGameData()
{
	gDataLocation = ReadDataLocationSetting();

	if (gDataLocation.empty())
	{
		DrawLocatePromptScreen();
		gDataLocation = DoOpenDialog(ARCHIVE_NAME);
		if (gDataLocation.empty()) {
			return ABORT;
		}
	}

	bool isPowerPCExecutable = false;
	bool isStuffItArchive = false;

	{
		std::ifstream file(gDataLocation, std::ios::binary | std::ios::in);
		if (!file.is_open()) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Nanosaur", "Couldn't open game data.", gSDLWindow);
			NukeDataLocationSetting();
			return RETRY;
		}

		char magic[17];

		file.read(magic, 12);
		magic[12] = '\0';
		isPowerPCExecutable = 0 == strncmp("Joy!peffpwpc", magic, 12);

		if (!isPowerPCExecutable) {
			file.seekg(0x80, std::ios::beg);
			file.read(magic, 16);
			magic[16] = '\0';
			isStuffItArchive = 0 == strncmp("StuffIt (c)1997-", magic, 16);
		}
	}

	FSSpec applicationSpec = {};

	if (isStuffItArchive) {
		// Mount game archive as data volume
		short archiveVolumeID = Pomme::Files::MountArchiveAsVolume(gDataLocation);
		gDataSpec.vRefNum = archiveVolumeID;

		if (noErr != FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, (const char*)APP_FILE_INSIDE_ARCHIVE, &applicationSpec)) {
			throw std::runtime_error("Can't find application resource file");
		}
	}
	else if (isPowerPCExecutable) {
		applicationSpec = Pomme::Files::HostPathToFSSpec(gDataLocation);
		gDataSpec.vRefNum = applicationSpec.vRefNum;
		gDataSpec.parID = applicationSpec.parID;
	}
	else {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Nanosaur", "File type not recognized.", gSDLWindow);
		NukeDataLocationSetting();
		return RETRY;
	}

	// Use application resource file
	short resFileRefNum = FSpOpenResFile(&applicationSpec, fsRdPerm);
	UseResFile(resFileRefNum);

	return OK;
}

bool FindGameData()
{
	FindGameData_Outcome outcome = RETRY;
	while (outcome == RETRY) {
		outcome = _FindGameData();
	}
	return outcome == OK;
}
