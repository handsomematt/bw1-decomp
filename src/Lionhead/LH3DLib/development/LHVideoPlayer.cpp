#include "LHVideoPlayer.h"

#include <string.h> /* For memset, memcpy */

#include "LH3DMaterial.h" /* For struct LH3DMaterial */
#include "LH3DRender.h"   /* For struct LH3DRender, DrawAndClip2D */
#include "LH3DTexture.h"  /* For struct LH3DTexture, struct LH3DSRAMTex */
#include "LH3DVRAMTex.h"  /* For CreateAtLeastN555, LeaveOnly3_555 */

#include <Lionhead/LHAudio/ver7.0/LH_AudioSystem.h> /* For class LH_AudioSystem */
#include <Lionhead/LHLib/ver5.0/LHSystem.h>         /* For struct LHSys */
#ifdef VERSION_BW1WCI
#include <Lionhead/LHFile/ver3.0/LHOSFile.h> /* For LHOSFile::Exists */

// Windows already defines INT32.
#define XMD_H
extern "C"
{
#include <jpeg/jpeglib.h>
}
#endif

// The 555 textures kept in video memory while a video is playing.
#define LH_VIDEO_PLAYER_555_COUNT 8
// Each blur pixel steps this many video pixels, so neighbouring samples overlap.
#define LH_VIDEO_PLAYER_BLUR_STEP 8
// Keeps texture coordinates half a texel inside the edges of a texture.
#define LH_VIDEO_PLAYER_HALF_TEXEL (0.5f / LH_VIDEO_PLAYER_TEXTURE_SIZE)
#define LH_VIDEO_PLAYER_Z          (1.0f - 0.99f)
#define LH_VIDEO_PLAYER_RHW        0.99f

#ifdef VERSION_BW1WCI
// Creature Isle plays numbered JPEG files instead of a Bink video.
#define LH_VIDEO_PLAYER_FRAME_FILE_NAME "%s%d.jpg"

enum LH_VIDEO_PLAYER_JPEG_FORMAT
{
	LH_VIDEO_PLAYER_JPEG_FORMAT_4444,
	LH_VIDEO_PLAYER_JPEG_FORMAT_555,
	LH_VIDEO_PLAYER_JPEG_FORMAT_565,
};
#endif

enum LH_VIDEO_PLAYER_STATE
{
	LH_VIDEO_PLAYER_STATE_IDLE,
	LH_VIDEO_PLAYER_STATE_NEXT_FRAME,
	LH_VIDEO_PLAYER_STATE_DO_FRAME,
	LH_VIDEO_PLAYER_STATE_COPY_TO_BUFFER,
};

CRITICAL_SECTION VideoSection;
// BW1W120 00ef74f0 BW1M119 013421e8 (LHCombined Release)
LH_VIDEO_PLAYER_STATE cowsrus;
int                   LHVideoPlayer::instances = 0;
// BW1W120 00ef7514 BW1M119 013421f0 (LHCombined Release)
LHVideoPlayer* activevideo = NULL;
// BW1W120 00ef7518 BW1M119 013421ec (LHCombined Release)
long videosem = false;

Vertex3D LHVideoPlayer::g_list_vertex[LH_VIDEO_PLAYER_VERTICES] = {
	{0.0f, 0.0f, LH_VIDEO_PLAYER_Z, LH_VIDEO_PLAYER_RHW, 0, 0, {0.0f, 0.0f}},
	{0.0f, 0.0f, LH_VIDEO_PLAYER_Z, LH_VIDEO_PLAYER_RHW, 0, 0, {0.0f, 0.0f}},
	{0.0f, 0.0f, LH_VIDEO_PLAYER_Z, LH_VIDEO_PLAYER_RHW, 0, 0, {0.0f, 0.0f}},
	{0.0f, 0.0f, LH_VIDEO_PLAYER_Z, LH_VIDEO_PLAYER_RHW, 0, 0, {0.0f, 0.0f}},
};
uint16_t LHVideoPlayer::g_index[LH_VIDEO_PLAYER_INDICES] = {0, 1, 3, 1, 2, 3};

// BW1W120 00845170 BW1M119 010cc0e0 (LHCombined Release)
uint16_t BlurPixels(uint16_t* pixels, int pitch, int red_mask, int green_mask, int blue_mask, int alpha);

void LHVideoPlayer::InitAudio(LH_AudioSystem* audio_system)
{
#ifndef VERSION_BW1WCI
	BinkSetSoundSystem(BinkOpenDirectSound, (unsigned long)audio_system->LHWaveGetQMixerDirectSoundObject());
#endif
}

void EnterVideoSection()
{
	EnterCriticalSection(&VideoSection);
	InterlockedExchange(&videosem, true);
}

void LeaveVideoSection()
{
	InterlockedExchange(&videosem, false);
	LeaveCriticalSection(&VideoSection);
}

void LHVideoPlayer::SetZero()
{
	memset(this, 0, sizeof(LHVideoPlayer));
	IsOpen = false;
	Height = 0;
	Width = 0;
	TextureCount = 0;
	TotalFrames = 0;
	FrameRate = 0;
	Materials = NULL;
	Textures = NULL;
	CurrentFrame = 0;
	FrameDecoded = false;
	Bink = NULL;
	Buffer = NULL;
	DrawPending = false;
}

LHVideoPlayer::LHVideoPlayer()
{
	SetZero();
	if (++instances == 1)
	{
		CreateAtLeastN555(LH_VIDEO_PLAYER_555_COUNT);
	}
}

LHVideoPlayer::~LHVideoPlayer()
{
	instances--;
	if (activevideo == this)
	{
		activevideo = NULL;
	}
	if (instances == 0)
	{
		LeaveOnly3_555();
	}
	Close();
}

void LHVideoPlayer::Close()
{
	if (IsOpen)
	{
		IsOpen = false;
#ifndef VERSION_BW1WCI
		if (Bink)
		{
			BinkClose(Bink);
		}
#endif
		Bink = NULL;
		if (Materials)
		{
			for (int i = 0; i < TextureCount; i++)
			{
				Materials[i]->Release();
			}
			delete Materials;
			Materials = NULL;
		}
		if (Textures)
		{
			for (int i = 0; i < TextureCount; i++)
			{
				Textures[i]->Release();
			}
			delete Textures;
			Textures = NULL;
		}
		if (BlurMaterial)
		{
			BlurMaterial->Release();
			BlurMaterial = NULL;
		}
		if (BlurTexture)
		{
			BlurTexture->Release();
			BlurTexture = NULL;
		}
		delete Buffer;
	}
	SetZero();
}

void __stdcall LHVideoPlayer::thedraw(void* context)
{
	LHVideoPlayer* video = activevideo;
	if (video && video->DrawPending)
	{
		video->DrawPending = false;
		video->DoDrawToScreen(video->DrawColor, video->DrawX, video->DrawY, video->DrawWidth, video->DrawHeight,
		                      video->DrawZWrite, video->DrawAlpha);
	}
}

#ifdef VERSION_BW1WCI
inline void LoadJpeg(char* file_name, uint16_t* dest, int* width, int* height, LH_VIDEO_PLAYER_JPEG_FORMAT format)
{
	jpeg_decompress_struct info;
	jpeg_error_mgr         error;
	info.err = jpeg_std_error(&error);
	FILE* file = fopen(file_name, "rb");
	if (file == NULL)
	{
		return;
	}
	jpeg_create_decompress(&info);
	jpeg_stdio_src(&info, file);
	jpeg_read_header(&info, TRUE);
	jpeg_start_decompress(&info);
	if (height)
	{
		*height = info.output_height;
	}
	if (width)
	{
		*width = info.output_width;
	}
	uint16_t*  out = dest;
	int        row_stride = info.output_width * info.output_components;
	JSAMPARRAY line = (*info.mem->alloc_sarray)((j_common_ptr)&info, JPOOL_IMAGE, row_stride, 1);
	while (info.output_scanline < info.output_height)
	{
		jpeg_read_scanlines(&info, line, 1);
		JSAMPLE* pixel = line[0];
		if (dest)
		{
			if (format == LH_VIDEO_PLAYER_JPEG_FORMAT_4444)
			{
				for (JDIMENSION x = 0; x < info.output_width; x++)
				{
					*out++ = ((pixel[0] >> 4) << 8) | ((pixel[1] >> 4) << 4) | (pixel[2] >> 4);
					pixel += info.output_components;
				}
			}
			else if (format == LH_VIDEO_PLAYER_JPEG_FORMAT_565)
			{
				for (JDIMENSION x = 0; x < info.output_width; x++)
				{
					*out++ = ((pixel[0] >> 3) << 11) | ((pixel[1] >> 2) << 5) | (pixel[2] >> 3);
					pixel += info.output_components;
				}
			}
			else if (format == LH_VIDEO_PLAYER_JPEG_FORMAT_555)
			{
				for (JDIMENSION x = 0; x < info.output_width; x++)
				{
					*out++ = ((pixel[0] >> 3) << 10) | ((pixel[1] >> 3) << 5) | (pixel[2] >> 3);
					pixel += info.output_components;
				}
			}
		}
	}
	jpeg_finish_decompress(&info);
	jpeg_destroy_decompress(&info);
	fclose(file);
}
#endif

void LHVideoPlayer::Init(char* file_name, bool hi_colour, bool make_blur_texture)
{
	Close();
#ifdef VERSION_BW1WCI
	Bink = NULL;
	CurrentFrame = 1;
	strcpy(FileName, file_name);
	char path[256];
	sprintf(path, LH_VIDEO_PLAYER_FRAME_FILE_NAME, file_name, CurrentFrame);
	if (LHOSFile::Exists(path) == LH_FILE_RESULT_OK)
	{
		Bink = (HBINK) true;
		LoadJpeg(path, NULL, &Width, &Height, LH_VIDEO_PLAYER_JPEG_FORMAT_4444);
		TotalFrames = 1;
		CurrentFrame = -1;
		LastFrame = -1;
	}
#else
	Bink = BinkOpen(file_name, BINKNOTHREADEDIO | BINKNOSKIP);
#endif
	HiColour = hi_colour;
	FrameRate = 1;
	if (Bink)
	{
#ifndef VERSION_BW1WCI
		BINKSUMMARY summary;
		BinkGetSummary(Bink, &summary);
		FrameRate = summary.FileFrameRate / summary.FileFrameRateDiv;
		TotalFrames = summary.TotalFrames;
		Width = summary.Width;
		Height = summary.Height;
		CurrentFrame = 0;
#endif
		TexturesWide = (Width + LH_VIDEO_PLAYER_TEXTURE_SIZE - 1) / LH_VIDEO_PLAYER_TEXTURE_SIZE;
		TexturesHigh = (Height + LH_VIDEO_PLAYER_TEXTURE_SIZE - 1) / LH_VIDEO_PLAYER_TEXTURE_SIZE;
		TextureCount = TexturesWide * TexturesHigh;
		BlurWidth = Width / LH_VIDEO_PLAYER_BLUR_STEP;
		BlurHeight = Height / LH_VIDEO_PLAYER_BLUR_STEP;
		if (Width / LH_VIDEO_PLAYER_BLUR_STEP > LH_VIDEO_PLAYER_TEXTURE_SIZE)
		{
			make_blur_texture = false;
		}
		if (Height / LH_VIDEO_PLAYER_BLUR_STEP > LH_VIDEO_PLAYER_TEXTURE_SIZE)
		{
			make_blur_texture = false;
		}
		Materials = new LH3DMaterial*[TextureCount];
		Textures = new LH3DTexture*[TextureCount];
		for (int i = 0; i < TextureCount; i++)
		{
			TextureFormat format;
			format.Type = hi_colour ? LH3D_TEXTURE_FORMAT_555 : LH3D_TEXTURE_FORMAT_4444;
			Textures[i] = LH3DTexture::Create(NULL,
			                                  (hi_colour ? LH3D_TEXTURE_FLAGS_HI_COLOUR : LH3D_TEXTURE_FLAGS_ALPHA) |
			                                      LH3D_TEXTURE_TYPE_SYSTEM_MEMORY,
			                                  0, &format);
			Materials[i] = LH3DRender::CreateMaterial(LH3DMaterial::LH3D_MATERIAL_RENDER_MODE_ALPHA_TEXTURED_ALPHA_NZ,
			                                          Textures[i]);
			Materials[i]->Flags &= ~LH3DMaterial::FLAG_NEED_TILLING;
			Materials[i]->Flags |= LH3DMaterial::FLAG_DOUBLE_SIDED;
		}
		if (make_blur_texture)
		{
			TextureFormat format;
			format.Type = hi_colour ? LH3D_TEXTURE_FORMAT_555 : LH3D_TEXTURE_FORMAT_4444;
			BlurTexture = LH3DTexture::Create(NULL,
			                                  (hi_colour ? LH3D_TEXTURE_FLAGS_HI_COLOUR : LH3D_TEXTURE_FLAGS_ALPHA) |
			                                      LH3D_TEXTURE_TYPE_SYSTEM_MEMORY,
			                                  0, &format);
			BlurMaterial = LH3DRender::CreateMaterial(LH3DMaterial::LH3D_MATERIAL_RENDER_MODE_ALPHA_TEXTURED_ALPHA_NZ,
			                                          BlurTexture);
			BlurMaterial->Flags &= ~LH3DMaterial::FLAG_NEED_TILLING;
			BlurMaterial->Flags |= LH3DMaterial::FLAG_DOUBLE_SIDED;
			Buffer = new uint16_t[Width * (Height + LH_VIDEO_PLAYER_BLUR_SIZE)];
		}
		else
		{
			Buffer = new uint16_t[Width * Height];
		}
		IsOpen = true;
		activevideo = this;
		memset(Buffer, 0, Width * Height * sizeof(uint16_t));
		CopyToTextures();
	}
}

void LHVideoPlayer::UnpackNextFrame()
{
#ifdef VERSION_BW1WCI
	if (LastFrame != CurrentFrame)
	{
		LastFrame = CurrentFrame;
		char path[256];
		sprintf(path, LH_VIDEO_PLAYER_FRAME_FILE_NAME, FileName, CurrentFrame);
		if (LHOSFile::Exists(path) == LH_FILE_RESULT_OK)
		{
			LoadJpeg(path, Buffer, &Width, &Height,
			         HiColour ? (LH3DTexture::g_b_use_565 ? LH_VIDEO_PLAYER_JPEG_FORMAT_565
			                                              : LH_VIDEO_PLAYER_JPEG_FORMAT_555)
			                  : LH_VIDEO_PLAYER_JPEG_FORMAT_4444);
		}
		CopyToTextures();
	}
#else
	if (Bink)
	{
		if (CurrentFrame <= TotalFrames)
		{
			cowsrus = LH_VIDEO_PLAYER_STATE_NEXT_FRAME;
			NextFrame();
			cowsrus = LH_VIDEO_PLAYER_STATE_DO_FRAME;
			if (BinkDoFrame(Bink) == 0)
			{
				cowsrus = LH_VIDEO_PLAYER_STATE_COPY_TO_BUFFER;
				BinkCopyToBuffer(Bink, Buffer, Width * sizeof(uint16_t), Height, 0, 0,
				                 HiColour ? (LH3DTexture::g_b_use_565 ? BINKSURFACE565 : BINKSURFACE555)
				                          : BINKSURFACE4444);
				CopyToTextures();
				FrameDecoded = true;
			}
			cowsrus = LH_VIDEO_PLAYER_STATE_IDLE;
		}
		CurrentFrame++;
	}
#endif
}

uint16_t BlurPixels(uint16_t* pixels, int pitch, int red_mask, int green_mask, int blue_mask, int alpha)
{
	int red = 0;
	int green = 0;
	int blue = 0;
	for (int y = 0; y < LH_VIDEO_PLAYER_BLUR_SIZE; y++)
	{
		int p0, p1, p2, p3, p4, p5, p6, p7;
		p7 = pixels[7];
		p6 = pixels[6];
		p5 = pixels[5];
		p4 = pixels[4];
		p3 = pixels[3];
		p2 = pixels[2];
		p1 = pixels[1];
		p0 = pixels[0];
		red += (p0 & red_mask) + (p1 & red_mask) + (p2 & red_mask) + (p3 & red_mask) + (p4 & red_mask) +
		       (p5 & red_mask) + (p6 & red_mask) + (p7 & red_mask);
		green += (p0 & green_mask) + (p1 & green_mask) + (p2 & green_mask) + (p3 & green_mask) + (p4 & green_mask) +
		         (p5 & green_mask) + (p6 & green_mask) + (p7 & green_mask);
		blue += (p0 & blue_mask) + (p1 & blue_mask) + (p2 & blue_mask) + (p3 & blue_mask) + (p4 & blue_mask) +
		        (p5 & blue_mask) + (p6 & blue_mask) + (p7 & blue_mask);
		pixels += 8;
		p7 = pixels[7];
		p6 = pixels[6];
		p5 = pixels[5];
		p4 = pixels[4];
		p3 = pixels[3];
		p2 = pixels[2];
		p1 = pixels[1];
		p0 = pixels[0];
		red += (p0 & red_mask) + (p1 & red_mask) + (p2 & red_mask) + (p3 & red_mask) + (p4 & red_mask) +
		       (p5 & red_mask) + (p6 & red_mask) + (p7 & red_mask);
		green += (p0 & green_mask) + (p1 & green_mask) + (p2 & green_mask) + (p3 & green_mask) + (p4 & green_mask) +
		         (p5 & green_mask) + (p6 & green_mask) + (p7 & green_mask);
		blue += (p0 & blue_mask) + (p1 & blue_mask) + (p2 & blue_mask) + (p3 & blue_mask) + (p4 & blue_mask) +
		        (p5 & blue_mask) + (p6 & blue_mask) + (p7 & blue_mask);
		pixels += pitch - 8;
	}
	return ((red >> 8) & red_mask) + ((green >> 8) & green_mask) + ((blue >> 8) & blue_mask) + alpha;
}

void LHVideoPlayer::CopyToTextures()
{
	int index = 0;
	for (int ty = 0; ty < TexturesHigh; ty++)
	{
		for (int tx = 0; tx < TexturesWide; tx++)
		{
			uint16_t* dest = Textures[index]->GetSRamTex()->Lock256x256(true);
			if (dest)
			{
				uint16_t* source = &Buffer[(ty * Width + tx) * LH_VIDEO_PLAYER_TEXTURE_SIZE];
				int       height =
					ty == TexturesHigh - 1 ? Height & (LH_VIDEO_PLAYER_TEXTURE_SIZE - 1) : LH_VIDEO_PLAYER_TEXTURE_SIZE;
				if (height == 0)
				{
					height = LH_VIDEO_PLAYER_TEXTURE_SIZE;
				}
				int width =
					tx == TexturesWide - 1 ? Width & (LH_VIDEO_PLAYER_TEXTURE_SIZE - 1) : LH_VIDEO_PLAYER_TEXTURE_SIZE;
				if (width == 0)
				{
					width = LH_VIDEO_PLAYER_TEXTURE_SIZE;
				}
				for (int y = 0; y < height; y++)
				{
					if (!HiColour)
					{
						uint16_t* pixel = source;
						for (int x = 0; x < width; x++)
						{
							*pixel++ |= 0xf000;
						}
					}
					memcpy(dest, source, width * sizeof(uint16_t));
					dest += LH_VIDEO_PLAYER_TEXTURE_SIZE;
					source += Width;
				}
				Textures[index]->GetSRamTex()->UnlockLock256x256();
				LH3DTexture* texture = Textures[index];
				if (texture->GetType() == LH3D_TEXTURE_TYPE_NAMED ||
				    texture->GetType() == LH3D_TEXTURE_TYPE_SYSTEM_MEMORY)
				{
					texture->ReloadPending = true;
				}
			}
			index++;
		}
	}
	if (BlurTexture && BlurMaterial)
	{
		int alpha;
		int red_mask;
		int green_mask;
		int blue_mask;
		if (HiColour)
		{
			if (LH3DTexture::g_b_use_565)
			{
				alpha = 0;
				red_mask = 0xf800;
				green_mask = 0x7e0;
				blue_mask = 0x1f;
			}
			else
			{
				alpha = 0x8000;
				red_mask = 0x7c00;
				green_mask = 0x3e0;
				blue_mask = 0x1f;
			}
		}
		else
		{
			alpha = 0xf000;
			red_mask = 0xf00;
			green_mask = 0xf0;
			blue_mask = 0xf;
		}
		uint16_t* dest = BlurTexture->GetSRamTex()->Lock256x256(true);
		if (dest)
		{
			uint16_t* source = Buffer;
			for (int y = 0; y < BlurHeight; y++)
			{
				for (int x = 0; x < BlurWidth; x++)
				{
					*dest++ = BlurPixels(source, Width, red_mask, green_mask, blue_mask, alpha);
					source += LH_VIDEO_PLAYER_BLUR_STEP;
				}
				source += (Width - BlurWidth) * LH_VIDEO_PLAYER_BLUR_STEP;
				dest += LH_VIDEO_PLAYER_TEXTURE_SIZE - BlurWidth;
			}
			BlurTexture->GetSRamTex()->UnlockLock256x256();
			LH3DTexture* texture = BlurTexture;
			if (texture->GetType() == LH3D_TEXTURE_TYPE_NAMED || texture->GetType() == LH3D_TEXTURE_TYPE_SYSTEM_MEMORY)
			{
				texture->ReloadPending = true;
			}
		}
	}
}

void LHVideoPlayer::DrawToScreen(LH3DColor color, int x, int y, int width, int height, bool z_write, bool alpha)
{
	EnterVideoSection();
	if (instances > 0 && this && Bink && Textures && Materials && Textures[0] && Materials[0] && IsOpen)
	{
		DrawColor = color;
		DrawX = x;
		DrawY = y;
		DrawWidth = width;
		DrawHeight = height;
		DrawZWrite = z_write;
		DrawAlpha = alpha;
		DrawPending = true;
	}
	LeaveVideoSection();
}

void LHVideoPlayer::DoDrawToScreen(LH3DColor color, int x, int y, int width, int height, bool z_write, bool alpha)
{
	EnterVideoSection();
	if (this && Bink && Textures && Materials && Textures[0] && Materials[0] && IsOpen)
	{
		int index = 0;
		if (width == 0)
		{
			width = LHSys::TheSystem.screen.width;
		}
		if (height == 0)
		{
			height = LHSys::TheSystem.screen.height;
		}
		float scaleX = (float)width / Width;
		float scaleY = (float)height / Height;
		for (int ty = 0; ty < TexturesHigh; ty++)
		{
			for (int tx = 0; tx < TexturesWide; tx++)
			{
				int tileHeight =
					ty == TexturesHigh - 1 ? Height & (LH_VIDEO_PLAYER_TEXTURE_SIZE - 1) : LH_VIDEO_PLAYER_TEXTURE_SIZE;
				if (tileHeight == 0)
				{
					tileHeight = LH_VIDEO_PLAYER_TEXTURE_SIZE;
				}
				int tileWidth =
					tx == TexturesWide - 1 ? Width & (LH_VIDEO_PLAYER_TEXTURE_SIZE - 1) : LH_VIDEO_PLAYER_TEXTURE_SIZE;
				if (tileWidth == 0)
				{
					tileWidth = LH_VIDEO_PLAYER_TEXTURE_SIZE;
				}
				float x0 = tx * LH_VIDEO_PLAYER_TEXTURE_SIZE * scaleX + x;
				float y0 = ty * LH_VIDEO_PLAYER_TEXTURE_SIZE * scaleY + y;
				float x1 = tileWidth * scaleX + x0;
				float y1 = tileHeight * scaleY + y0;
				float u = (float)tileWidth / LH_VIDEO_PLAYER_TEXTURE_SIZE;
				float v = (float)tileHeight / LH_VIDEO_PLAYER_TEXTURE_SIZE;
				g_list_vertex[0].x = x0;
				g_list_vertex[0].uv.x = LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[0].y = y0;
				g_list_vertex[0].uv.y = LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[1].x = x1;
				g_list_vertex[1].uv.x = u - LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[1].y = y0;
				g_list_vertex[1].uv.y = LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[2].x = x1;
				g_list_vertex[2].uv.x = u - LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[2].y = y1;
				g_list_vertex[2].uv.y = v - LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[3].x = x0;
				g_list_vertex[3].uv.x = LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[3].y = y1;
				g_list_vertex[3].uv.y = v - LH_VIDEO_PLAYER_HALF_TEXEL;
				g_list_vertex[0].color = g_list_vertex[1].color = g_list_vertex[2].color = g_list_vertex[3].color =
					color.GetColor();
				LH3DRender::SetMaterial(Materials[index], 0);
				LH3DRender::SetRenderState(D3DRENDERSTATE_ZFUNC, alpha ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS);
				LH3DRender::SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, z_write ? true : false);
				DrawAndClip2D(D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX, g_list_vertex, LH_VIDEO_PLAYER_VERTICES, g_index,
				              LH_VIDEO_PLAYER_INDICES);
				index++;
			}
		}
		LH3DRender::SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
		LH3DRender::SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, true);
	}
	LeaveVideoSection();
}
