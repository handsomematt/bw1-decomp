#include "LH3DMem.h"

#include <stdlib.h> /* For malloc, free */

#include "LH3DTech.h" /* For Report3D__FPCce */

struct LH3DMemHeader
{
	long     Size;
	uint32_t Magic;
};

bool32_t LH3DMem::g_panic = false;
uint32_t LH3DMem::g_total_count = 0;
uint32_t LH3DMem::g_total_amount = 0;
uint32_t LH3DMem::g_current_count = 0;
uint32_t LH3DMem::g_current_amount = 0;
bool32_t LH3DMem::g_open = false;

void* LH3DMem::Alloc(long size)
{
	if (!g_open)
	{
		g_current_amount = 0;
		g_current_count = 0;
		g_total_amount = 0;
		g_total_count = 0;
		g_open = true;
	}
	g_current_count++;
	g_total_count++;
	g_current_amount += size;
	g_total_amount += size;
	LH3DMemHeader* header = (LH3DMemHeader*)malloc(size + sizeof(LH3DMemHeader) + sizeof(uint32_t));
	header->Size = size;
	header->Magic = LH3D_MEM_MAGIC;
	*(uint32_t*)((char*)(header + 1) + size) = LH3D_MEM_MAGIC;
	return header + 1;
}

void LH3DMem::Free(void* ptr)
{
	if (g_open && ptr != NULL)
	{
		LH3DMemHeader* header = (LH3DMemHeader*)ptr - 1;
		g_current_count--;
		g_current_amount -= header->Size;
		if (header->Magic != LH3D_MEM_MAGIC)
		{
			Report3D__FPCce("####################################\n");
			Report3D__FPCce("####################################\n");
		}
		if (*(uint32_t*)((char*)ptr + header->Size) != LH3D_MEM_MAGIC)
		{
			Report3D__FPCce("###################################\n");
			Report3D__FPCce("###################################\n");
		}
		free(header);
	}
}

void* LH3DMem::AllocAligned(long size, int alignment)
{
	char* data = (char*)malloc(size + alignment + 2 * sizeof(void*));
	if (data == NULL)
	{
		return NULL;
	}
	void** aligned = (void**)((uint32_t)(data + sizeof(void*) + alignment - 1) & ~(alignment - 1));
	aligned[-1] = data;
	return aligned;
}

void LH3DMem::FreeAligned(void* ptr)
{
	if (ptr != NULL)
	{
		free(((void**)ptr)[-1]);
	}
}
