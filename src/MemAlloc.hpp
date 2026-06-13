#pragma once

#include "inttypes.hpp"
#include <cstddef>

namespace MemAlloc
{
void *Alloc(size_t size);
void Free(void *ptr);
}; // namespace MemAlloc

struct MemArena
{
    u8 *base = nullptr;
    size_t capacity = 0;
    size_t offset = 0;

    bool Init(size_t bytes);
    void Destroy();
    void *Alloc(size_t bytes, size_t align = 16);
    void Reset();
};

extern MemArena g_SceneArena;
extern MemArena g_FrameArena;

namespace MemArenas
{
bool InitAll();
void DestroyAll();
}; // namespace MemArenas
