#include "LH3DVRAMTex.h"

#include <d3d.h>    /* For IDirect3DTexture2, D3DENUMRET_OK */
#include <ddraw.h>  /* For DDSURFACEDESC2, DDSCAPS_*, DDPF_* */
#include <string.h> /* For memset */

#include "LH3DMem.h"     /* For struct LH3DMem */
#include "LH3DRender.h"  /* For struct LH3DRender */
#include "LH3DTech.h"    /* For Report3D__FPCce, LH3DTech */
#include "LH3DTexture.h" /* For struct LH3DTexture */

#include <Lionhead/LHLib/ver5.0/LHSurface.h> /* For struct LHSurface */
#include <Lionhead/LHLib/ver5.0/LHSystem.h>  /* For struct LHSys */

#define LH3D_VRAM_TEX_DEFAULT_MAX_AGP_MEM  (8 * 1024 * 1024)
#define LH3D_VRAM_TEX_DEFAULT_MAX_VRAM_MEM (256 * 1024 * 1024)
#define LH3D_VRAM_TEX_DEFAULT_555_COUNT    3
#define LH3D_VRAM_TEX_32X32_COUNT          16
#define LH3D_VRAM_TEX_LOW_RES_COUNT        6
#define LH3D_VRAM_TEX_565_GREEN_MASK       0x7e0
#define LH3D_VRAM_TEX_4444_ALPHA_MASK      0xf000
#define LH3D_VRAM_TEX_4444_RED_MASK        0xf00
#define LH3D_VRAM_TEX_4444_GREEN_MASK      0xf0
#define LH3D_VRAM_TEX_4444_BLUE_MASK       0xf
#define LH3D_VRAM_TEX_555_RED_MASK         0x7c00
#define LH3D_VRAM_TEX_555_GREEN_MASK       0x3e0
#define LH3D_VRAM_TEX_555_BLUE_MASK        0x1f
#define LH3D_VRAM_TEX_CHECK_PERIOD         1000
#define LH3D_VRAM_TEX_MAX_ACTIVATE_COUNT   0x10000
#define LH3D_VRAM_TEX_SWAP_DELAY_FRAMES    10
#define LH3D_VRAM_TEX_CAPS                 (DDSCAPS_VIDEOMEMORY | DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX)
#define LH3D_VRAM_TEX_DESC_FLAGS           (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT)

long LH3DVRAMTex::g_max_amount_agp_mem = LH3D_VRAM_TEX_DEFAULT_MAX_AGP_MEM;
long LH3DVRAMTex::g_max_amount_vram_mem = LH3D_VRAM_TEX_DEFAULT_MAX_VRAM_MEM;
// BW1W120 00c392c8 BW1M119 011d17f0 (LHCombined Release)
int g_count_555 = LH3D_VRAM_TEX_DEFAULT_555_COUNT;

bool32_t                 LH3DVRAMTex::g_b_open = false;
int                      LH3DVRAMTex::g_count = 0;
LH3DVRAMTex*             LH3DVRAMTex::g_first = NULL;
LH3DVRAMTex::PixelFormat LH3DVRAMTex::g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_COUNT];
uint32_t                 LH3DVRAMTex::g_agp_amount_mem = 0;
uint32_t                 LH3DVRAMTex::g_vram_amount_mem = 0;
bool32_t                 LH3DVRAMTex::g_b_alloc_mouse_surface = false;
LHSurface*               LH3DVRAMTex::g_surface_mouse = NULL;
// BW1W120 00f05218 BW1M119 01342220 (LHCombined Release)
int g_caca_d3d_lost_mem = 0;

bool32_t LH3DVRAMTex::Open()
{
	Report3D__FPCce(">> lh3dvram open\n");
	if (g_b_open)
	{
		Report3D__FPCce("LH3DVRAMTex::Open already open!!\n");
		return false;
	}
	g_b_open = true;
	g_count = 0;
	g_first = NULL;
	for (int i = 0; i < LH3D_VRAM_TEX_PIXEL_FORMAT_COUNT; i++)
	{
		g_pixel_format[i].Found = false;
		memset(&g_pixel_format[i].Format, 0, sizeof(DDPIXELFORMAT));
	}
	LH3DRender::D3DDevice->EnumTextureFormats(FindTextureCB, NULL);
	Report3D__FPCce(
		"We choose  : (%2d bits) %s%s%s %08X %08X %08X %08X \n",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwRGBBitCount,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA) ? "ALPHA "
																										 : "      ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwFlags & DDPF_RGB ? "RGB " : "    ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwFlags & (DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED8)
			? "PAL "
			: "    ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwRGBAlphaBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwRBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwGBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format.dwBBitMask);
	Report3D__FPCce(
		"We choose  : (%2d bits) %s%s%s %08X %08X %08X %08X \n",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwRGBBitCount,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA) ? "ALPHA "
																										: "      ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwFlags & DDPF_RGB ? "RGB " : "    ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwFlags & (DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED8)
			? "PAL "
			: "    ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwRGBAlphaBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwRBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwGBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwBBitMask);
	if (g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format.dwGBitMask == LH3D_VRAM_TEX_565_GREEN_MASK)
	{
		LH3DTexture::g_b_use_565 = true;
	}
	Report3D__FPCce(
		"We choose  : (%2d bits) %s%s%s %08X %08X %08X %08X \n",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwRGBBitCount,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA) ? "ALPHA "
																										 : "      ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwFlags & DDPF_RGB ? "RGB " : "    ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwFlags & (DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED8)
			? "PAL "
			: "    ",
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwRGBAlphaBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwRBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwGBitMask,
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format.dwBBitMask);
	Report3D__FPCce("\n");
	AllocAllVRAM();
	return true;
}

bool32_t LH3DVRAMTex::Close()
{
	if (!g_b_open)
	{
		Report3D__FPCce("LH3DVRAMTex::Close not open.\n");
		return false;
	}
	Report3D__FPCce(">> lh3dvram close\n");
	ReleaseAll();
	g_b_open = false;
	return true;
}

LH3DVRAMTex* LH3DVRAMTex::GetElderOrFree(TextureFormat format, bool32_t alloc)
{
	LH3DVRAMTex* vram = g_first;
	while (vram != NULL && vram->Format.Type != format.Type)
	{
		vram = vram->Next;
	}
	LH3DVRAMTex* elder = NULL;
	if (vram == NULL)
	{
		return elder;
	}
	if (alloc)
	{
		while (vram != NULL && (!vram->Alloc || vram->Format.Type != format.Type))
		{
			vram = vram->Next;
		}
		while (vram != NULL)
		{
			if (vram->Texture == NULL || elder == NULL)
			{
				elder = vram;
				vram = NULL;
			}
			else
			{
				if (vram->Texture->LastUsedTime < elder->Texture->LastUsedTime)
				{
					elder = vram;
				}
				vram = vram->Next;
				while (vram != NULL && (!vram->Alloc || vram->Format.Type != format.Type))
				{
					vram = vram->Next;
				}
			}
		}
	}
	else
	{
		elder = vram;
		while (vram != NULL)
		{
			if (vram->Texture == NULL)
			{
				elder = vram;
				vram = NULL;
			}
			else
			{
				if (vram->Texture->LastUsedTime < elder->Texture->LastUsedTime)
				{
					elder = vram;
				}
				vram = vram->Next;
				while (vram != NULL && vram->Format.Type != format.Type)
				{
					vram = vram->Next;
				}
			}
		}
	}
	if (elder != NULL && elder->Format.Type != format.Type)
	{
		return elder;
	}
	return elder;
}

LH3DVRAMTex* LH3DVRAMTex::AssignTexture(LH3DTexture* texture, bool32_t alloc)
{
	LH3DVRAMTex* vram = GetElderOrFree(texture->GetFormat(), alloc);
	if (alloc && vram == NULL)
	{
		vram = GetElderOrFree(texture->GetFormat(), false);
	}
	if (vram == NULL)
	{
		return NULL;
	}
	if (vram->Texture != NULL)
	{
		vram->Texture->YouLostYourVRAM();
	}
	vram->Texture = texture;
	return vram;
}

void LH3DVRAMTex::Release()
{
	LH3DVRAMTex* prev = NULL;
	for (LH3DVRAMTex* vram = g_first; vram != this; vram = vram->Next)
	{
		prev = vram;
	}
	if (prev != NULL)
	{
		prev->Next = Next;
	}
	else
	{
		g_first = Next;
	}
	Next = NULL;
	g_count--;
	if (D3DTexture != NULL)
	{
		D3DTexture->Release();
	}
	if (Surface != NULL)
	{
		Surface->Release();
	}
	Surface = NULL;
	if (Texture != NULL)
	{
		Texture->YouLostYourVRAM();
	}
	Texture = NULL;
	LH3DMem::Free(this);
}

void LH3DVRAMTex::ReleaseAll()
{
	Report3D__FPCce(">> lh3dvram releaseall\n");
	int count = 0;
	while (g_first != NULL)
	{
		count++;
		g_first->Release();
	}
	Report3D__FPCce("LH3DVRAMTex::ReleaseAll: %d/%d\n", g_count, count);
	LHSurface* surface = g_surface_mouse;
	if (surface != NULL)
	{
		surface->~LHSurface();
		LHSys::TheSystem.mem.Free(surface);
		g_surface_mouse = NULL;
	}
	g_vram_amount_mem = 0;
}

void LH3DVRAMTex::LoadInVRAM(IDirectDrawSurface7* surface)
{
	Surface->Blt(NULL, surface, NULL, DDBLT_WAIT, NULL);
}

HRESULT CALLBACK LH3DVRAMTex::FindTextureCB(DDPIXELFORMAT* pixel_format, void* context)
{
	Report3D__FPCce("FindTexture: (%2d bits) %s%s%s %08X %08X %08X %08X \n", pixel_format->dwRGBBitCount,
	                pixel_format->dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA) ? "ALPHA " : "      ",
	                pixel_format->dwFlags & DDPF_RGB ? "RGB " : "    ",
	                pixel_format->dwFlags & (DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED8) ? "PAL " : "    ",
	                pixel_format->dwRGBAlphaBitMask, pixel_format->dwRBitMask, pixel_format->dwGBitMask,
	                pixel_format->dwBBitMask);
	if ((pixel_format->dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA)) && pixel_format->dwRGBBitCount == 16 &&
	    (pixel_format->dwFlags & DDPF_RGB))
	{
		if (g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Found)
		{
			return D3DENUMRET_OK;
		}
		if (pixel_format->dwRGBAlphaBitMask == LH3D_VRAM_TEX_4444_ALPHA_MASK &&
		    pixel_format->dwRBitMask == LH3D_VRAM_TEX_4444_RED_MASK &&
		    pixel_format->dwGBitMask == LH3D_VRAM_TEX_4444_GREEN_MASK &&
		    pixel_format->dwBBitMask == LH3D_VRAM_TEX_4444_BLUE_MASK)
		{
			g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Found = true;
		}
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_4444].Format = *pixel_format;
		return D3DENUMRET_OK;
	}
	if (!(pixel_format->dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA)) && pixel_format->dwRGBBitCount == 16 &&
	    (pixel_format->dwFlags & DDPF_RGB))
	{
		if (g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Found)
		{
			return D3DENUMRET_OK;
		}
		if (pixel_format->dwRBitMask == LH3D_VRAM_TEX_555_RED_MASK &&
		    pixel_format->dwGBitMask == LH3D_VRAM_TEX_555_GREEN_MASK &&
		    pixel_format->dwBBitMask == LH3D_VRAM_TEX_555_BLUE_MASK)
		{
			g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Found = true;
		}
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_555].Format = *pixel_format;
		return D3DENUMRET_OK;
	}
	if ((pixel_format->dwFlags & (DDPF_ALPHAPIXELS | DDPF_ALPHA)) && pixel_format->dwRGBBitCount == 32 &&
	    (pixel_format->dwFlags & DDPF_RGB))
	{
		if (g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Found)
		{
			return D3DENUMRET_OK;
		}
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Found = true;
		g_pixel_format[LH3D_VRAM_TEX_PIXEL_FORMAT_8888].Format = *pixel_format;
		return D3DENUMRET_OK;
	}
	return D3DENUMRET_OK;
}

void LH3DVRAMTex::AllocAllVRAM()
{
	TextureFormat shadowFormat;
	shadowFormat.Type = LH3D_TEXTURE_FORMAT_32X32_4444;
	for (int i = 0; i < LH3D_VRAM_TEX_32X32_COUNT; i++)
	{
		if (AllocVRAM(true, true, shadowFormat) == NULL)
		{
			TextureFormat localFormat;
			localFormat.Type = LH3D_TEXTURE_FORMAT_32X32_4444;
			AllocVRAM(false, true, localFormat);
		}
	}
	TextureFormat format;
	if (LH3DTexture::g_b_use_low_res)
	{
		for (int j = LH3D_VRAM_TEX_LOW_RES_COUNT; j != 0; j--)
		{
			format.Type = LH3D_TEXTURE_FORMAT_ALWAYS_256X256_4444;
			AllocVRAM(false, true, format);
		}
	}
	for (uint32_t k = 0; k < g_count_555; k++)
	{
		format.Type = LH3D_TEXTURE_FORMAT_555;
		if (AllocVRAM(false, true, format) == NULL)
		{
			Report3D__FPCce("Alloc 555 failed.\n");
		}
	}
	format.Type = LH3D_TEXTURE_FORMAT_4444;
	while (AllocVRAM(false, true, format) != NULL && g_vram_amount_mem < g_max_amount_vram_mem)
	{
		format.Type = LH3D_TEXTURE_FORMAT_4444;
	}
	Report3D__FPCce("After VRAM: %d\n", g_count);
	if (LH3DRender::g_b_support_AGP)
	{
		Report3D__FPCce("After AGP: %d\n", g_count);
	}

	LH3DVRAMTex* local = NULL;
	LH3DVRAMTex* agp = NULL;
	LH3DVRAMTex* vram = g_first;
	LH3DVRAMTex* next;
	while (vram != NULL)
	{
		next = vram->Next;
		if (vram->Alloc)
		{
			vram->Next = agp;
			agp = vram;
		}
		else
		{
			vram->Next = local;
			local = vram;
		}
		vram = next;
	}
	if (local != NULL)
	{
		g_first = local;
		for (vram = local; vram != NULL; vram = vram->Next)
		{
			if (vram->Next == NULL)
			{
				next = vram;
			}
		}
		next->Next = agp;
	}
	else
	{
		g_first = agp;
	}
	LH3DTexture::CreateBufferSurcfaces();
}

LH3DVRAMTex* LH3DVRAMTex::AllocVRAM(bool32_t agp, bool32_t loaded, TextureFormat format)
{
	if (agp && !LH3DRender::g_b_support_AGP)
	{
		return NULL;
	}
	int width = format.GetWidth();
	int height = format.GetHeight();
	g_count++;
	LH3DVRAMTex* vram = (LH3DVRAMTex*)LH3DMem::Alloc(sizeof(LH3DVRAMTex));
	memset(vram, 0, sizeof(LH3DVRAMTex));
	vram->Next = g_first;
	g_first = vram;
	vram->Loaded = loaded;
	vram->Surface = NULL;
	vram->Texture = NULL;
	vram->ActivateCount = 0;
	vram->Format = format;
	DDSURFACEDESC2 desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.ddpfPixelFormat = g_pixel_format[vram->Format.GetPixelFormat()].Format;
	desc.dwFlags |= LH3D_VRAM_TEX_DESC_FLAGS;
	desc.dwWidth = width;
	desc.dwHeight = height;
	desc.ddsCaps.dwCaps = agp ? DDSCAPS_NONLOCALVIDMEM | LH3D_VRAM_TEX_CAPS : DDSCAPS_LOCALVIDMEM | LH3D_VRAM_TEX_CAPS;
	desc.dwMipMapCount = 1;
	if (LHSys::TheSystem.screen.PDirectDraw->CreateSurface(&desc, &vram->Surface, NULL) != DD_OK)
	{
		Report3D__FPCce("Create VRAM Surface FAILED %d\n", g_count);
		vram->Surface = NULL;
		vram->Release();
		return NULL;
	}
	vram->Surface->GetSurfaceDesc(&desc);
	if ((agp && !(desc.ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM)) ||
	    (!agp && (desc.ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM)))
	{
		vram->Alloc = true;
		g_agp_amount_mem += width * height * sizeof(uint16_t);
	}
	else
	{
		vram->Alloc = false;
		g_vram_amount_mem += width * height * sizeof(uint16_t);
	}
	vram->Surface->QueryInterface(IID_IDirect3DTexture2, (void**)&vram->D3DTexture);
	return vram;
}

void LH3DVRAMTex::CheckAgpVsLocal()
{
	static uint32_t time = 0;
	time += LH3DTech::g_delta_time;
	if (time < LH3D_VRAM_TEX_CHECK_PERIOD)
	{
		return;
	}
	time = 0;
	LH3DVRAMTex* agp = NULL;
	LH3DVRAMTex* local = NULL;
	uint32_t     maxCount = 0;
	uint32_t     minCount = LH3D_VRAM_TEX_MAX_ACTIVATE_COUNT;
	LH3DVRAMTex* vram = g_first;
	while (vram != NULL && (vram->Format.Type != LH3D_TEXTURE_FORMAT_4444 || !vram->Loaded))
	{
		vram = vram->Next;
	}
	while (vram != NULL)
	{
		if (vram->Alloc && vram->Texture != NULL && vram->ActivateCount != 0)
		{
			if (agp != NULL)
			{
				if (maxCount < vram->ActivateCount)
				{
					maxCount = vram->ActivateCount;
					agp = vram;
				}
			}
			else
			{
				agp = vram;
				maxCount = vram->ActivateCount;
			}
		}
		else if (!vram->Alloc)
		{
			if (vram->Texture == NULL)
			{
				local = vram;
				minCount = 0;
			}
			else if (vram->ActivateCount < minCount)
			{
				local = vram;
				minCount = vram->ActivateCount;
			}
			else if (vram->ActivateCount == minCount && local != NULL && local->Texture != NULL &&
			         vram->Texture->LastUsedTime < local->Texture->LastUsedTime)
			{
				local = vram;
				minCount = vram->ActivateCount;
			}
		}
		vram->ActivateCount = 0;
		vram = vram->Next;
		while (vram != NULL && (vram->Format.Type != LH3D_TEXTURE_FORMAT_4444 || !vram->Loaded))
		{
			vram = vram->Next;
		}
	}
	if (local != NULL && agp != NULL)
	{
		IDirect3DTexture2* agpTexture;
		IDirect3DTexture2* localTexture;
		if (local->Texture != NULL)
		{
			if (LH3DRender::g_frame - local->Texture->LastUsedTime < LH3D_VRAM_TEX_SWAP_DELAY_FRAMES)
			{
				return;
			}
			if (agp->Surface->Restore() != DD_OK)
			{
				Report3D__FPCce("CheckAgpVsLocal: Restore Failed!!!\n");
				return;
			}
			if (local->Surface->Restore() != DD_OK)
			{
				Report3D__FPCce("CheckAgpVsLocal: Restore Failed!!!\n");
				return;
			}
			IDirect3DTexture2* tempTexture;
			agp->Surface->QueryInterface(IID_IDirect3DTexture2, (void**)&agpTexture);
			local->Surface->QueryInterface(IID_IDirect3DTexture2, (void**)&localTexture);
			LH3DTexture::g_surface_for_load_4444->QueryInterface(IID_IDirect3DTexture2, (void**)&tempTexture);
			tempTexture->Load(agpTexture);
			agpTexture->Load(localTexture);
			localTexture->Load(tempTexture);
			LH3DTexture* texture = local->Texture;
			texture->VRAMTex = agp;
			local->Texture = agp->Texture;
			agp->Texture->VRAMTex = local;
			agp->Texture = texture;
			agpTexture->Release();
			localTexture->Release();
			tempTexture->Release();
		}
		else
		{
			if (agp->Surface->Restore() != DD_OK)
			{
				Report3D__FPCce("LoadInVRAM: Restore Failed!!!\n");
				return;
			}
			if (local->Surface->Restore() != DD_OK)
			{
				Report3D__FPCce("LoadInVRAM: Restore Failed!!!\n");
				return;
			}
			agp->Surface->QueryInterface(IID_IDirect3DTexture2, (void**)&agpTexture);
			local->Surface->QueryInterface(IID_IDirect3DTexture2, (void**)&localTexture);
			localTexture->Load(agpTexture);
			local->Texture = agp->Texture;
			agp->Texture->VRAMTex = local;
			agp->Texture = NULL;
			agpTexture->Release();
			localTexture->Release();
		}
	}
}

void LeaveOnly3_555()
{
	int          count = 0;
	LH3DVRAMTex* vram;
	for (vram = LH3DVRAMTex::g_first; vram != NULL; vram = vram->Next)
	{
		if (vram->Format.Type == LH3D_TEXTURE_FORMAT_555)
		{
			count++;
		}
	}
	if (count > LH3D_VRAM_TEX_DEFAULT_555_COUNT)
	{
		int extra = count - LH3D_VRAM_TEX_DEFAULT_555_COUNT;
		Report3D__FPCce("Release %d 555s\n", extra);
		vram = LH3DVRAMTex::g_first;
		for (int i = extra; i != 0;)
		{
			if (vram->Format.Type == LH3D_TEXTURE_FORMAT_555)
			{
				LH3DVRAMTex* release = vram;
				vram = vram->Next;
				release->Release();
				i--;
			}
			else
			{
				vram = vram->Next;
			}
		}
		TextureFormat format;
		for (; extra != 0; extra--)
		{
			format.Type = LH3D_TEXTURE_FORMAT_4444;
			LH3DVRAMTex::AllocVRAM(false, true, format);
		}
		g_count_555 = LH3D_VRAM_TEX_DEFAULT_555_COUNT;
	}
}

void CreateAtLeastN555(int count)
{
	int          found = 0;
	LH3DVRAMTex* vram;
	for (vram = LH3DVRAMTex::g_first; vram != NULL; vram = vram->Next)
	{
		if (vram->Format.Type == LH3D_TEXTURE_FORMAT_555)
		{
			found++;
		}
	}
	Report3D__FPCce("\nJC: find %d 555\n", found);
	if (count > found)
	{
		g_count_555 = count;
		int needed = count - found;
		int toFree = needed;
		for (vram = LH3DVRAMTex::g_first; vram != NULL && toFree != 0;)
		{
			if (vram->Format.Type != LH3D_TEXTURE_FORMAT_4444 || vram->Texture != NULL)
			{
				vram = vram->Next;
			}
			else
			{
				LH3DVRAMTex* release = vram;
				vram = vram->Next;
				release->Release();
				toFree--;
			}
		}
		for (vram = LH3DVRAMTex::g_first; vram != NULL && toFree != 0; vram = vram->Next)
		{
			if (vram->Format.Type == LH3D_TEXTURE_FORMAT_4444)
			{
				LH3DVRAMTex* release = vram;
				vram = vram->Next;
				release->Release();
				toFree--;
			}
		}
		if (toFree != 0)
		{
			Report3D__FPCce("Couldn't free all textures");
		}
		Report3D__FPCce("try alloc %d 555s.\n", needed);
		TextureFormat format;
		for (int i = needed; i != 0; i--)
		{
			format.Type = LH3D_TEXTURE_FORMAT_555;
			LH3DVRAMTex::AllocVRAM(false, true, format);
		}
	}
}
