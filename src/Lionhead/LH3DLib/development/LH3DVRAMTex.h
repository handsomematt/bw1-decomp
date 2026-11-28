#ifndef BW1_DECOMP_LH3D_VRAM_TEX_INCLUDED_H
#define BW1_DECOMP_LH3D_VRAM_TEX_INCLUDED_H

#include <assert.h> /* For static_assert */
#include <ddraw.h>  /* For DDPIXELFORMAT, IDirectDrawSurface7 */
#include <stdint.h> /* For uint32_t */

#include <re_common.h> /* For bool32_t */

#include "LH3DTexture.h" /* For struct TextureFormat */

// Forward Declares

struct IDirect3DTexture2;
struct LH3DTexture;
struct LHSurface;

enum LH3D_VRAM_TEX_PIXEL_FORMAT
{
	LH3D_VRAM_TEX_PIXEL_FORMAT_4444,
	LH3D_VRAM_TEX_PIXEL_FORMAT_555,
	LH3D_VRAM_TEX_PIXEL_FORMAT_8888,
	LH3D_VRAM_TEX_PIXEL_FORMAT_COUNT,
};

inline int TextureFormat::GetPixelFormat()
{
	if (Type == LH3D_TEXTURE_FORMAT_555)
	{
		return LH3D_VRAM_TEX_PIXEL_FORMAT_555;
	}
	return Type == LH3D_TEXTURE_FORMAT_256X256_8888 ? LH3D_VRAM_TEX_PIXEL_FORMAT_8888 : LH3D_VRAM_TEX_PIXEL_FORMAT_4444;
}

struct LH3DVRAMTex
{
	// A texture format the device can use.
	struct PixelFormat
	{
		DDPIXELFORMAT Format;
		bool32_t      Found;
	};

	// BW1W120 00f05214 BW1M119 01342224 (LHCombined Release)
	static LHSurface* g_surface_mouse;
	// BW1W120 00f05210 BW1M119 01342228 (LHCombined Release)
	static bool32_t g_b_alloc_mouse_surface;
	// BW1W120 00f0520c BW1M119 0134222c (LHCombined Release)
	static uint32_t g_vram_amount_mem;
	// BW1W120 00f05208 BW1M119 01342230 (LHCombined Release)
	static uint32_t g_agp_amount_mem;
	// BW1W120 00f05190 BW1M119 01342234 (LHCombined Release)
	static PixelFormat g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_COUNT];
	// BW1W120 00f05204 BW1M119 013422a0 (LHCombined Release)
	static LH3DVRAMTex* g_first;
	// BW1W120 00f05200 BW1M119 013422a4 (LHCombined Release)
	static int g_count;
	// BW1W120 00f051fc BW1M119 013422a8 (LHCombined Release)
	static bool32_t g_b_open;
	// BW1W120 00c392c0 BW1M119 011d17e8 (LHCombined Release)
	static long g_max_amount_agp_mem;
	// BW1W120 00c392c4 BW1M119 011d17ec (LHCombined Release)
	static long g_max_amount_vram_mem;

	IDirectDrawSurface7* Surface;
	LH3DTexture*         Texture;
	LH3DVRAMTex*         Next;
	IDirect3DTexture2*   D3DTexture;
	bool32_t             Alloc;
	bool32_t             Loaded;
	uint32_t             ActivateCount;
	TextureFormat        Format;

	// Static methods

	// BW1W120 0085d7f0 BW1M119 010d15b0 (LHCombined Release)
	static bool32_t Open();
	// BW1W120 0085d9c0 BW1M119 010d14c0 (LHCombined Release)
	static bool32_t Close();
	// BW1W120 0085da00 BW1M119 010d1310 (LHCombined Release)
	static LH3DVRAMTex* GetElderOrFree(TextureFormat format, bool32_t alloc);
	// BW1W120 0085dac0 BW1M119 010d1220 (LHCombined Release)
	static LH3DVRAMTex* AssignTexture(LH3DTexture* texture, bool32_t alloc);
	// BW1W120 0085db90 BW1M119 010d1030 (LHCombined Release)
	static void ReleaseAll();
	// BW1W120 0085dc20 BW1M119 010d0db0 (LHCombined Release)
	static HRESULT CALLBACK FindTextureCB(DDPIXELFORMAT* pixel_format, void* context);
	// BW1W120 0085dd60 BW1M119 010d0b90 (LHCombined Release)
	static void AllocAllVRAM();
	// BW1W120 0085deb0 BW1M119 010d08b0 (LHCombined Release)
	static LH3DVRAMTex* AllocVRAM(bool32_t agp, bool32_t loaded, TextureFormat format);
	// BW1W120 0085e080 BW1M119 null
	static void CheckAgpVsLocal();

	// Non-virtual methods

	// BW1W120 0085db10 BW1M119 010d1110 (LHCombined Release)
	void Release();
	// BW1W120 0085dc00 BW1M119 0100c7b0 (LHCombined Release)
	void LoadInVRAM(IDirectDrawSurface7* surface);
};
static_assert(sizeof(LH3DVRAMTex::PixelFormat) == 0x24, "Data type is of wrong size");
static_assert(sizeof(LH3DVRAMTex) == 0x20, "Data type is of wrong size");

// BW1W120 0085e320 BW1M119 010d07a0 (LHCombined Release)
void LeaveOnly3_555();
// BW1W120 0085e3a0 BW1M119 010d0630 (LHCombined Release)
void CreateAtLeastN555(int count);

#endif /* BW1_DECOMP_LH3D_VRAM_TEX_INCLUDED_H */
