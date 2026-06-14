#include "FileManager.hpp"
#include "FileSystem.hpp"
#include "MemAlloc.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstring>

#include <libmc.h>
#include <loadfile.h>

FileManager g_FileManager;

void FileManager::Init(StorageTarget target)
{
    this->target = target;
    this->memoryFileCount = 0;
    std::memset(this->memoryFiles, 0, sizeof(this->memoryFiles));

    if (target == StorageTarget::None)
    {
        this->baseDir[0] = '\0';
        utils::DebugPrint2("FileManager: in-memory only, nothing is persisted");
        return;
    }

    i32 port = (target == StorageTarget::MemoryCard1) ? 0 : 1;
    std::snprintf(this->baseDir, sizeof(this->baseDir), "mc%d:/TH06", port);

    SifLoadModule("rom0:XMCMAN", 0, NULL);
    SifLoadModule("rom0:XMCSERV", 0, NULL);
    mcInit(MC_TYPE_XMC);

    mcMkDir(port, 0, "/TH06");
    i32 result;
    mcSync(0, NULL, &result);

    utils::DebugPrint2("FileManager: using %s", this->baseDir);
}

const char *FileManager::Basename(const char *name)
{
    const char *slash = std::strrchr(name, '/');
    const char *backslash = std::strrchr(name, '\\');
    const char *cut = slash > backslash ? slash : backslash;
    return cut != nullptr ? cut + 1 : name;
}

void FileManager::ResolveName(const char *name, char *dst, size_t size)
{
    std::snprintf(dst, size, "%s/%s", this->baseDir, name);
}

FileManager::MemoryFile *FileManager::FindMemoryFile(const char *name)
{
    for (i32 i = 0; i < this->memoryFileCount; i++)
    {
        if (std::strcmp(this->memoryFiles[i].name, name) == 0)
        {
            return &this->memoryFiles[i];
        }
    }
    return nullptr;
}

bool FileManager::Exists(const char *name)
{
    name = Basename(name);

    if (this->target == StorageTarget::None)
    {
        return FindMemoryFile(name) != nullptr;
    }

    char path[128];
    ResolveName(name, path, sizeof(path));
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    std::fclose(file);
    return true;
}

bool FileManager::Write(const char *name, const void *data, size_t size)
{
    name = Basename(name);

    if (this->target == StorageTarget::None)
    {
        MemoryFile *file = FindMemoryFile(name);
        if (file == nullptr)
        {
            if (this->memoryFileCount >= MAX_MEMORY_FILES)
            {
                utils::DebugPrint2("FileManager: memory file table is full");
                return false;
            }
            file = &this->memoryFiles[this->memoryFileCount++];
            std::strncpy(file->name, name, sizeof(file->name) - 1);
        }
        else
        {
            MemAlloc::Free(file->data);
        }

        file->data = (u8 *)MemAlloc::Alloc(size);
        if (file->data == nullptr)
        {
            file->size = 0;
            return false;
        }
        std::memcpy(file->data, data, size);
        file->size = size;
        return true;
    }

    char path[128];
    ResolveName(name, path, sizeof(path));
    FILE *file = std::fopen(path, "wb");
    if (file == nullptr)
    {
        utils::DebugPrint2("FileManager: cannot open %s for writing", path);
        return false;
    }
    bool ok = std::fwrite(data, 1, size, file) == size;
    std::fclose(file);
    return ok;
}

u8 *FileManager::Read(const char *name, size_t *outSize)
{
    name = Basename(name);

    if (this->target == StorageTarget::None)
    {
        MemoryFile *file = FindMemoryFile(name);
        if (file == nullptr)
        {
            g_LastFileSize = 0;
            if (outSize != nullptr)
            {
                *outSize = 0;
            }
            return nullptr;
        }

        u8 *copy = (u8 *)MemAlloc::Alloc(file->size);
        if (copy == nullptr)
        {
            return nullptr;
        }
        std::memcpy(copy, file->data, file->size);
        g_LastFileSize = (u32)file->size;
        if (outSize != nullptr)
        {
            *outSize = file->size;
        }
        return copy;
    }

    char path[128];
    ResolveName(name, path, sizeof(path));
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr)
    {
        g_LastFileSize = 0;
        if (outSize != nullptr)
        {
            *outSize = 0;
        }
        return nullptr;
    }

    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    u8 *data = (u8 *)MemAlloc::Alloc(size);
    if (data == nullptr)
    {
        std::fclose(file);
        return nullptr;
    }
    std::fread(data, 1, size, file);
    std::fclose(file);

    g_LastFileSize = (u32)size;
    if (outSize != nullptr)
    {
        *outSize = (size_t)size;
    }
    return data;
}

bool FileManager::Delete(const char *name)
{
    name = Basename(name);

    if (this->target == StorageTarget::None)
    {
        MemoryFile *file = FindMemoryFile(name);
        if (file == nullptr)
        {
            return false;
        }
        MemAlloc::Free(file->data);
        i32 index = (i32)(file - this->memoryFiles);
        this->memoryFiles[index] = this->memoryFiles[--this->memoryFileCount];
        std::memset(&this->memoryFiles[this->memoryFileCount], 0, sizeof(MemoryFile));
        return true;
    }

    char path[128];
    ResolveName(name, path, sizeof(path));
    return std::remove(path) == 0;
}
