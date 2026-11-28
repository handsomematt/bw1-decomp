#ifndef BW1_DECOMP_BINK_INCLUDED_H
#define BW1_DECOMP_BINK_INCLUDED_H

// Bink 1.0w output ABI verified against the game DLL. Public names corroborated
// by RAD's Bink 1.5v SDK. Movie instance layouts differ, so keep the handle opaque.
struct BINK;
typedef BINK* HBINK;

struct BINKREALTIME
{
	unsigned long FrameNum;
	unsigned long FrameRate;
	unsigned long FrameRateDiv;
	unsigned long Frames;
	unsigned long FramesTime;
	unsigned long FramesVideoDecompTime;
	unsigned long FramesAudioDecompTime;
	unsigned long FramesReadTime;
	unsigned long FramesIdleReadTime;
	unsigned long FramesThreadReadTime;
	unsigned long FramesBlitTime;
	unsigned long ReadBufferSize;
	unsigned long ReadBufferUsed;
	unsigned long FramesDataRate;
};

struct BINKSUMMARY
{
	unsigned long Width;
	unsigned long Height;
	unsigned long TotalTime;
	unsigned long FileFrameRate;
	unsigned long FileFrameRateDiv;
	unsigned long FrameRate;
	unsigned long FrameRateDiv;
	unsigned long TotalOpenTime;
	unsigned long TotalFrames;
	unsigned long TotalPlayedFrames;
	unsigned long SkippedFrames;
	unsigned long SkippedBlits;
	unsigned long SoundSkips;
	unsigned long TotalBlitTime;
	unsigned long TotalReadTime;
	unsigned long TotalVideoDecompTime;
	unsigned long TotalAudioDecompTime;
	unsigned long TotalIdleReadTime;
	unsigned long TotalBackReadTime;
	unsigned long TotalReadSpeed;
	unsigned long SlowestFrameTime;
	unsigned long Slowest2FrameTime;
	unsigned long SlowestFrameNum;
	unsigned long Slowest2FrameNum;
	unsigned long AverageDataRate;
	unsigned long AverageFrameSize;
	unsigned long HighestMemAmount;
	unsigned long TotalIOMemory;
	unsigned long HighestIOUsed;
	unsigned long Highest1SecRate;
	unsigned long Highest1SecFrame;
};

// BinkOpen flags
#define BINKNOSKIP       0x00080000
#define BINKNOTHREADEDIO 0x08000000

// BinkCopyToBuffer surface types
#define BINKSURFACE4444 7
#define BINKSURFACE555  9
#define BINKSURFACE565  10

struct BINKSND;
typedef long(__stdcall* BINKSNDOPEN)(BINKSND* sound, unsigned long freq, long bits, long chans, unsigned long flags,
                                     HBINK bink);
typedef BINKSNDOPEN(__stdcall* BINKSNDSYSOPEN)(unsigned long param);

extern "C" __declspec(dllimport) HBINK __stdcall       BinkOpen(const char* name, unsigned long flags);
extern "C" __declspec(dllimport) void __stdcall        BinkClose(HBINK bink);
extern "C" __declspec(dllimport) void __stdcall        BinkGetSummary(HBINK bink, BINKSUMMARY* summary);
extern "C" __declspec(dllimport) long __stdcall        BinkDoFrame(HBINK bink);
extern "C" __declspec(dllimport) long __stdcall        BinkCopyToBuffer(HBINK bink, void* dest, long dest_pitch,
                                                                        unsigned long dest_height, unsigned long dest_x,
                                                                        unsigned long dest_y, unsigned long flags);
extern "C" __declspec(dllimport) long __stdcall        BinkSetSoundSystem(BINKSNDSYSOPEN open, unsigned long param);
extern "C" __declspec(dllimport) BINKSNDOPEN __stdcall BinkOpenDirectSound(unsigned long param);
extern "C" __declspec(dllimport) void __stdcall        BinkService(HBINK bink);
extern "C" __declspec(dllimport) void __stdcall        BinkGetRealtime(HBINK bink, BINKREALTIME* realtime,
                                                                       unsigned long frames);
extern "C" __declspec(dllimport) void __stdcall        BinkGoto(HBINK bink, unsigned long frame, long flags);
extern "C" __declspec(dllimport) void __stdcall        BinkNextFrame(HBINK bink);
extern "C" __declspec(dllimport) long __stdcall        BinkWait(HBINK bink);
extern "C" __declspec(dllimport) long __stdcall        BinkSetSoundOnOff(HBINK bink, long on_off);

#endif /* BW1_DECOMP_BINK_INCLUDED_H */
