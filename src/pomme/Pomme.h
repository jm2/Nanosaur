#pragma once

#include <sstream>
#include <Quesa.h>

#include "PommeTypes.h"
#include "PommeEnums.h"

// https://www.cocoawithlove.com/blog/porting-from-macos8-to-sierra.html

// basic types: https://opensource.apple.com/source/CarbonHeaders/CarbonHeaders-18.1/MacTypes.h.auto.html
// Time Manager: http://mirror.informatimago.com/next/developer.apple.com/documentation/Carbon/Reference/Time_Manager/Time_Manager.pdf
// Quickdraw.h: https://github.com/phracker/MacOSX-SDKs/blob/master/MacOSX10.9.sdk/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/QD.framework/Versions/A/Headers/Quickdraw.h
// Sound manager: http://www.few.vu.nl/~A.Eliens/research/media/lib-of-vs-libs-QTDevWin-CIncludes-Sound.h

//-----------------------------------------------------------------------------
// PowerPC intrinsics

#define __fres(x) (1.0f/x)
#define __fabs(x) fabs(x)
#define __frsqrte(x) (1.0f/sqrtf(x))

//-----------------------------------------------------------------------------
// Source code compat

#define nil NULL

//-----------------------------------------------------------------------------
// My todos

void ImplementMe(const char* fn, std::string msg, int severity);

#define TODOCUSTOM(message, severity) { \
    std::stringstream ss; \
    ss << message; \
    ImplementMe(__func__, ss.str(), severity); \
}

#define TODOMINOR()      TODOCUSTOM("", 0)
#define TODOMINOR2(x)    TODOCUSTOM(x, 0)
#define TODO()           TODOCUSTOM("", 1)
#define TODO2(x)         TODOCUSTOM(x, 1)
#define TODOFATAL()      TODOCUSTOM("", 2)
#define TODOFATAL2(x)    TODOCUSTOM(x, 2)

//-----------------------------------------------------------------------------
// FSSpec

OSErr FSMakeFSSpec(short vRefNum, long dirID, ConstStr255Param fileName, FSSpec* spec);

short FSpOpenResFile(const FSSpec* spec, char permission);

// Open a file's data fork
OSErr FSpOpenDF(const FSSpec* spec, char permission, short* refNum);

// Open a file's resource fork
OSErr FSpOpenRF(const FSSpec* spec, char permission, short* refNum);

OSErr FSpCreate(const FSSpec* spec, OSType creator, OSType fileType, ScriptCode scriptTag);

OSErr FSpDelete(const FSSpec* spec);

OSErr ResolveAlias(const FSSpec* spec, AliasHandle alias, FSSpec* target, Boolean* wasChanged);

//-----------------------------------------------------------------------------
// File I/O

OSErr FSRead(short refNum, long* count, Ptr buffPtr);

OSErr FSWrite(short refNum, long* count, Ptr buffPtr);

OSErr FSClose(short refNum);

OSErr GetEOF(short refNum, long* logEOF);

OSErr SetEOF(short refNum, long logEOF);

//-----------------------------------------------------------------------------
// Resource file management

// MoreMacintoshToolbox.pdf p174
OSErr ResError(void);

void UseResFile(short refNum);

// Gets the file reference number of the current resource file.
short CurResFile();

void CloseResFile(short refNum);

int Count1Resources(ResType);

Handle GetResource(ResType theType, short theID);

void ReleaseResource(Handle theResource);

void RemoveResource(Handle theResource);

void AddResource(Handle theData, ResType theType, short theID, const char* name);

void WriteResource(Handle theResource);

void DetachResource(Handle theResource);

//-----------------------------------------------------------------------------
// QuickDraw 2D

// pictureID = resource ID for a 'PICT' resource
PicHandle GetPicture(short pictureID);

void DisposeGWorld(GWorldPtr offscreenGWorld);

//-----------------------------------------------------------------------------
// QuickDraw 3D

TQ3StorageObject Q3FSSpecStorage_New(const FSSpec* fs);

//-----------------------------------------------------------------------------
// Misc

void ExitToShell();

void SysBeep(short duration);

void FlushEvents(short, short);

void NumToString(long theNum, Str255 theString);

//-----------------------------------------------------------------------------
// Input

void GetKeys(KeyMap);

Boolean Button(void);

//-----------------------------------------------------------------------------
// Memory: No-op

void MaxApplZone(void);

void MoreMasters(void);

Size CompactMem(Size);

// Compact system heap zone manually.
Size CompactMemSys(Size);

void PurgeMem(Size);

void PurgeMemSys(Size);

Size MaxMem(Size* grow);

void HNoPurge(Handle);

void HLock(Handle);

void HLockHi(Handle);

void NoPurgePixels(PixMapHandle);

// To prevent the base address for an offscreen pixel image from being moved
// while you draw into or copy from its pixel map.
Boolean LockPixels(PixMapHandle); // shall return true always

//-----------------------------------------------------------------------------
// Memory: Handle

// Allocate prezeroed memory
Handle NewHandleClear(Size);

// Allocate temp memory
Handle TempNewHandle(Size, OSErr*);

Size GetHandleSize(Handle);

// Change the logical size of the relocatable block corresponding to a handle
void SetHandleSize(Handle, Size);

void DisposeHandle(Handle);

//-----------------------------------------------------------------------------
// Memory: Ptr

Ptr NewPtr(Size);

Ptr NewPtrSys(Size);

void DisposePtr(Ptr p);

//-----------------------------------------------------------------------------
// Memory: BlockMove

// Copies a sequence of bytes from one location in memory to another
void BlockMove(const void* srcPtr, void* destPtr, Size byteCount);

void BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount);

//-----------------------------------------------------------------------------
// Time Manager

// Number of seconds elapsed since 1904-01-01 00:00
void GetDateTime(unsigned long* secs);

// Number of usecs elapsed since system startup
void Microseconds(UnsignedWide* microTickCount);

// Number of ticks elapsed since system startup (1 tick = approx. 1/60 of a second)
UInt32 TickCount();

//-----------------------------------------------------------------------------
// Mouse cursor

void HideCursor();

void InitCursor();

//-----------------------------------------------------------------------------
// Sound Manager

OSErr GetDefaultOutputVolume(long*);

void SetDefaultOutputVolume(long);

OSErr SndNewChannel(SndChannelPtr* chan, short synth, long init, /*ProcPtr*/void* userRoutine);

OSErr SndChannelStatus(SndChannelPtr chan, short theLength, SCStatusPtr theStatus);

OSErr SndDoImmediate(SndChannelPtr chan, const SndCommand* cmd);

OSErr GetSoundHeaderOffset(SndListHandle sndHandle, long* offset);

OSErr SndStartFilePlay(SndChannelPtr chan, short fRefNum, short resNum, long bufferSize, Ptr theBuffer, /*AudioSelectionPtr*/ void* theSelection, FilePlayCompletionUPP theCompletion, Boolean async);

OSErr SndPauseFilePlay(SndChannelPtr chan);

OSErr SndStopFilePlay(SndChannelPtr chan, Boolean quietNow);

//-----------------------------------------------------------------------------
// Fade

void MakeFadeEvent(Boolean fadeIn);

void GammaFadeOut();

void GammaFadeIn();

//-----------------------------------------------------------------------------

void InitFakeMac();