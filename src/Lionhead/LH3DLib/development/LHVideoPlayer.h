#ifndef BW1_DECOMP_LH_VIDEO_PLAYER_INCLUDED_H
#define BW1_DECOMP_LH_VIDEO_PLAYER_INCLUDED_H

#include <assert.h> /* For static_assert */
#include <stdint.h> /* For uint16_t */
#include <windows.h>

#include <re_common.h> /* For bool32_t */

#include "Bink.h"
#include "LH3DColor.h"
#include "LH3DVertex.h" /* For struct Vertex3D */

class LH_AudioSystem;
struct LH3DMaterial;
struct LH3DTexture;

// The video is cut into square textures of this many pixels a side.
#define LH_VIDEO_PLAYER_TEXTURE_SIZE 256
// Each pixel of the blur texture averages a square of this many video pixels a side.
#define LH_VIDEO_PLAYER_BLUR_SIZE 16
#define LH_VIDEO_PLAYER_VERTICES  4
#define LH_VIDEO_PLAYER_INDICES   6

class LHVideoPlayer
{
public:
	int      Width;
	int      Height;
	int      FrameRate;
	int      TotalFrames;
	int      CurrentFrame; /* 0x10 */
	bool32_t IsOpen;
	bool     HiColour;
	HBINK    Bink; /* 0x1c */
#ifdef VERSION_BW1WCI
	int  LastFrame;
	char FileName[256];
#endif
	bool32_t       FrameDecoded;
	int            TextureCount;
	int            TexturesWide;
	int            TexturesHigh;
	LH3DMaterial** Materials; /* 0x30 */
	LH3DTexture**  Textures;
	LH3DMaterial*  BlurMaterial;
	LH3DTexture*   BlurTexture;
	int            BlurWidth; /* 0x40 */
	int            BlurHeight;
	uint16_t*      Buffer;
	LH3DColor      DrawColor;
	int            DrawX; /* 0x50 */
	int            DrawY;
	int            DrawWidth;
	int            DrawHeight;
	bool           DrawZWrite; /* 0x60 */
	bool           DrawAlpha;
	bool32_t       DrawPending;

	// BW1W120 00c391b0 BW1M119 011d1734 (LHCombined Release)
	static Vertex3D g_list_vertex[LH_VIDEO_PLAYER_VERTICES];
	// BW1W120 00c39230 BW1M119 011d17b4 (LHCombined Release)
	static uint16_t g_index[LH_VIDEO_PLAYER_INDICES];
	// BW1W120 00ef7510 BW1M119 0134220c (LHCombined Release)
	static int instances;

	// BW1W120 00844d00 BW1M119 010ccb70 (LHCombined Release)
	LHVideoPlayer();
	// BW1W120 00844d30 BW1M119 010ccab0 (LHCombined Release)
	~LHVideoPlayer();

	// BW1W120 00844c60 BW1M119 010ccf90 (LHCombined Release)
	static void InitAudio(LH_AudioSystem* audio_system);
	// BW1W120 00844e30 BW1M119 0100a470 (LHCombined Release)
	static void __stdcall thedraw(void* context);
	// BW1W120 00844cc0 BW1M119 010ccc40 (LHCombined Release)
	void SetZero();
	// BW1W120 00844e70 BW1M119 010cc480 (LHCombined Release)
	void Init(char* file_name, bool hi_colour, bool make_blur_texture);
	// BW1W120 00844d70 BW1M119 010cc8f0 (LHCombined Release)
	void Close();
	// BW1W120 inlined BW1M119 inlined
	void NextFrame()
	{
		if (Bink && FrameDecoded)
			BinkNextFrame(Bink);
		FrameDecoded = false;
	}
	// BW1W120 inlined BW1M119 inlined
	bool NextFrameReady()
	{
		NextFrame();
		return Bink ? BinkWait(Bink) == 0 : false;
	}
	// BW1W120 008450b0 BW1M119 010cc330 (LHCombined Release)
	void UnpackNextFrame();
	// BW1W120 00845420 BW1M119 010cbd40 (LHCombined Release)
	void CopyToTextures();
	// BW1W120 00845740 BW1M119 010cb700 (LHCombined Release)
	void DoDrawToScreen(LH3DColor color, int x, int y, int width, int height, bool z_write, bool alpha);
	// BW1W120 008456c0 BW1M119 010cbc60 (LHCombined Release)
	void DrawToScreen(LH3DColor color, int x, int y, int width, int height, bool z_write, bool alpha);
};
#ifdef VERSION_BW1WCI
static_assert(sizeof(LHVideoPlayer) == 0x16c, "Data type is of wrong size");
#else
static_assert(sizeof(LHVideoPlayer) == 0x68, "Data type is of wrong size");
#endif

// BW1W120 00ef74f8 BW1M119 013421f4 (LHCombined Release)
extern CRITICAL_SECTION VideoSection;

// BW1W120 00844c80 BW1M119 01009d90 (LHCombined Release)
void EnterVideoSection();
// BW1W120 00844ca0 BW1M119 01009dc0 (LHCombined Release)
void LeaveVideoSection();

#endif /* BW1_DECOMP_LH_VIDEO_PLAYER_INCLUDED_H */
