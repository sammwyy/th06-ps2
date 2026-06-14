#include "FileManager.hpp"
#include "FileSystem.hpp"
#include "MemAlloc.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstring>

#include <libmc.h>
#include <loadfile.h>

FileManager g_FileManager;

#define TH06_DIR "/TH06"

#define MC_O_RDONLY 0x0001
#define MC_O_WRONLY 0x0002
#define MC_O_CREAT 0x0200
#define MC_O_TRUNC 0x0400
#define MC_SEEK_SET 0
#define MC_SEEK_END 2

void FileManager::Init(StorageTarget target)
{
    this->target = target;
    this->memoryFileCount = 0;
    std::memset(this->memoryFiles, 0, sizeof(this->memoryFiles));

    if (target == StorageTarget::None)
    {
        this->cardPort = -1;
        utils::DebugPrint2("FileManager: in-memory only, nothing is persisted");
        return;
    }

    this->cardPort = (target == StorageTarget::MemoryCard1) ? 0 : 1;

    SifLoadModule("rom0:XMCMAN", 0, NULL);
    SifLoadModule("rom0:XMCSERV", 0, NULL);
    mcInit(MC_TYPE_XMC);

    i32 cardType, cardFree, cardFormat;
    mcGetInfo(this->cardPort, 0, &cardType, &cardFree, &cardFormat);
    i32 infoResult;
    mcSync(0, NULL, &infoResult);
    utils::DebugPrint2("FileManager: mc%d info=%d type=%d free=%d format=%d", this->cardPort, infoResult, cardType,
                       cardFree, cardFormat);

    if (infoResult == sceMcResNoFormat || cardFormat == 0)
    {
        utils::DebugPrint2("FileManager: mc%d is unformatted, formatting it", this->cardPort);
        mcFormat(this->cardPort, 0);
        i32 formatResult;
        mcSync(0, NULL, &formatResult);
        utils::DebugPrint2("FileManager: format mc%d -> %d", this->cardPort, formatResult);
    }

    mcMkDir(this->cardPort, 0, TH06_DIR);
    i32 mkdirResult;
    mcSync(0, NULL, &mkdirResult);
    utils::DebugPrint2("FileManager: mkdir mc%d:%s -> %d", this->cardPort, TH06_DIR, mkdirResult);
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
    std::snprintf(dst, size, "%s/%s", TH06_DIR, name);
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

    char path[64];
    ResolveName(name, path, sizeof(path));

    i32 fd;
    mcOpen(this->cardPort, 0, path, MC_O_RDONLY);
    mcSync(0, NULL, &fd);
    if (fd < 0)
    {
        return false;
    }
    i32 closeResult;
    mcClose(fd);
    mcSync(0, NULL, &closeResult);
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

    char path[64];
    ResolveName(name, path, sizeof(path));

    i32 fd;
    mcOpen(this->cardPort, 0, path, MC_O_WRONLY | MC_O_CREAT | MC_O_TRUNC);
    mcSync(0, NULL, &fd);
    if (fd < 0)
    {
        utils::DebugPrint2("FileManager: mcOpen(write) mc%d:%s failed %d", this->cardPort, path, fd);
        return false;
    }

    i32 written;
    mcWrite(fd, data, (i32)size);
    mcSync(0, NULL, &written);

    i32 closeResult;
    mcClose(fd);
    mcSync(0, NULL, &closeResult);

    return written == (i32)size;
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

    char path[64];
    ResolveName(name, path, sizeof(path));

    i32 fd;
    mcOpen(this->cardPort, 0, path, MC_O_RDONLY);
    mcSync(0, NULL, &fd);
    if (fd < 0)
    {
        g_LastFileSize = 0;
        if (outSize != nullptr)
        {
            *outSize = 0;
        }
        return nullptr;
    }

    i32 size;
    mcSeek(fd, 0, MC_SEEK_END);
    mcSync(0, NULL, &size);
    i32 seekResult;
    mcSeek(fd, 0, MC_SEEK_SET);
    mcSync(0, NULL, &seekResult);

    u8 *data = (u8 *)MemAlloc::Alloc(size);
    if (data == nullptr)
    {
        i32 closeResult;
        mcClose(fd);
        mcSync(0, NULL, &closeResult);
        return nullptr;
    }

    i32 bytesRead;
    mcRead(fd, data, size);
    mcSync(0, NULL, &bytesRead);

    i32 closeResult;
    mcClose(fd);
    mcSync(0, NULL, &closeResult);

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

    char path[64];
    ResolveName(name, path, sizeof(path));

    i32 deleteResult;
    mcDelete(this->cardPort, 0, path);
    mcSync(0, NULL, &deleteResult);
    return deleteResult >= 0;
}
