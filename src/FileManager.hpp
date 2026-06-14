#pragma once

#include "inttypes.hpp"
#include <cstddef>

#define SAVE_FILE_CONFIG "config.dat"
#define SAVE_FILE_SCORE "score.dat"

enum class StorageTarget
{
    None,
    MemoryCard1,
    MemoryCard2,
};

struct FileManager
{
    void Init(StorageTarget target);

    bool Exists(const char *name);
    bool Write(const char *name, const void *data, size_t size);
    u8 *Read(const char *name, size_t *outSize);
    bool Delete(const char *name);

    StorageTarget Target() const
    {
        return target;
    }

  private:
    static const i32 MAX_MEMORY_FILES = 32;

    struct MemoryFile
    {
        char name[64];
        u8 *data;
        size_t size;
    };

    StorageTarget target = StorageTarget::None;
    char baseDir[32] = {0};
    MemoryFile memoryFiles[MAX_MEMORY_FILES] = {};
    i32 memoryFileCount = 0;

    static const char *Basename(const char *name);
    void ResolveName(const char *name, char *dst, size_t size);
    MemoryFile *FindMemoryFile(const char *name);
};

extern FileManager g_FileManager;
