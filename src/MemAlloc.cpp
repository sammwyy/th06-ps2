#include "MemAlloc.hpp"
#include <cstdlib>

namespace MemAlloc
{
void *Alloc(size_t size)
{
    return std::malloc(size);
}

void Free(void *ptr)
{
    std::free(ptr);
}
}; // namespace MemAlloc

MemArena g_SceneArena;
MemArena g_FrameArena;

#define SCENE_ARENA_BYTES (1 * 1024 * 1024)
#define FRAME_ARENA_BYTES (256 * 1024)

bool MemArena::Init(size_t bytes)
{
    base = (u8 *)MemAlloc::Alloc(bytes);
    if (base == nullptr)
    {
        return false;
    }
    capacity = bytes;
    offset = 0;
    return true;
}

void MemArena::Destroy()
{
    MemAlloc::Free(base);
    base = nullptr;
    capacity = 0;
    offset = 0;
}

void *MemArena::Alloc(size_t bytes, size_t align)
{
    size_t aligned = (offset + (align - 1)) & ~(align - 1);
    if (aligned + bytes > capacity)
    {
        return MemAlloc::Alloc(bytes);
    }
    void *ptr = base + aligned;
    offset = aligned + bytes;
    return ptr;
}

void MemArena::Reset()
{
    offset = 0;
}

namespace MemArenas
{
bool InitAll()
{
    return g_SceneArena.Init(SCENE_ARENA_BYTES) && g_FrameArena.Init(FRAME_ARENA_BYTES);
}

void DestroyAll()
{
    g_FrameArena.Destroy();
    g_SceneArena.Destroy();
}
}; // namespace MemArenas
