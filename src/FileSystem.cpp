#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#include <new>
#include <windows.h>
#elif __cplusplus >= 201703L
#include <filesystem>
#else // Assume POSIX
#include <sys/stat.h>
#endif

#include "FileSystem.hpp"
#include "pbg3/Pbg3Archive.hpp"
#include "utils.hpp"

u32 g_LastFileSize;

// Directory the .elf was launched from, with a trailing slash (e.g. "mass:/th06/")
static char s_BasePath[256] = {0};

void FileSystem::SetBasePath(const char *argv0)
{
    s_BasePath[0] = '\0';
    if (argv0 == NULL)
    {
        utils::DebugPrint2("base path: argv0 is null, using cwd-relative paths\n");
        return;
    }

    // Cut at the last separator to keep just the directory
    const char *lastSlash = std::strrchr(argv0, '/');
    const char *lastBackslash = std::strrchr(argv0, '\\');
    const char *cut = lastSlash > lastBackslash ? lastSlash : lastBackslash;
    if (cut == NULL)
    {
        // No directory in argv0 (e.g. just "host:"); keep the device part if any
        const char *colon = std::strrchr(argv0, ':');
        cut = colon;
    }
    if (cut != NULL)
    {
        size_t len = (size_t)(cut - argv0) + 1;
        if (len >= sizeof(s_BasePath))
        {
            len = sizeof(s_BasePath) - 1;
        }
        std::memcpy(s_BasePath, argv0, len);
        s_BasePath[len] = '\0';
    }
    utils::DebugPrint2("base path: '%s' (from argv0 '%s')\n", s_BasePath, argv0);
}

const char *FileSystem::ResolvePath(const char *path, char *dst, std::size_t size)
{
    // Absolute or device-qualified paths (mass:/, host:/, /foo) pass through
    bool hasDevice = std::strchr(path, ':') != NULL;
    bool isAbsolute = path[0] == '/' || path[0] == '\\';
    if (s_BasePath[0] == '\0' || hasDevice || isAbsolute)
    {
        std::snprintf(dst, size, "%s", path);
    }
    else
    {
        std::snprintf(dst, size, "%s%s", s_BasePath, path);
    }

    if (std::strncmp(dst, "cdrom", 5) == 0)
    {
        char *colon = std::strchr(dst, ':');
        if (colon != NULL)
        {
            for (char *c = colon + 1; *c != '\0'; c++)
            {
                *c = std::toupper((unsigned char)*c);
            }
        }
    }

    return dst;
}

FILE *FileSystem::FopenUTF8(const char *filepath, const char *mode)
{
#ifndef _WIN32
    char resolved[512];
    ResolvePath(filepath, resolved, sizeof(resolved));

    FILE *file = std::fopen(resolved, mode);

    if (file == NULL && std::strncmp(resolved, "cdrom", 5) == 0)
    {
        for (int attempt = 0; attempt < 16 && file == NULL; attempt++)
        {
            for (volatile int spin = 0; spin < 2000000; spin++)
            {
            }
            file = std::fopen(resolved, mode);
        }
    }
    return file;
#else
    u32 filepathWLen = MultiByteToWideChar(CP_UTF8, 0, filepath, -1, NULL, 0) * 2;
    u32 modeWLen = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0) * 2;

    if (filepathWLen == 0 || modeWLen == 0)
    {
        return NULL;
    }

    wchar_t *filepathW = new wchar_t[filepathWLen];
    wchar_t *modeW = new wchar_t[modeWLen];

    MultiByteToWideChar(CP_UTF8, 0, filepath, -1, filepathW, filepathWLen / 2);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, modeW, modeWLen / 2);

    FILE *f = _wfopen(filepathW, modeW);

    delete[] filepathW;
    delete[] modeW;

    return f;
#endif
}

void FileSystem::CreateDir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#elif __cplusplus >= 201703L
    auto p = std::filesystem::path(path);
    std::filesystem::create_directory(p);
#else
    mkdir(path, 0755);
#endif
}

u8 *FileSystem::OpenPath(const char *filepath, int isExternalResource)
{
    u8 *data;
    FILE *file;
    size_t fsize;
    i32 entryIdx;
    const char *entryname;
    i32 pbg3Idx;

    entryIdx = -1;
    if (isExternalResource == 0)
    {
        entryname = std::strrchr(filepath, '\\');
        if (entryname == (char *)0x0)
        {
            entryname = filepath;
        }
        else
        {
            entryname = entryname + 1;
        }
        entryname = std::strrchr(entryname, '/');
        if (entryname == (char *)0x0)
        {
            entryname = filepath;
        }
        else
        {
            entryname = entryname + 1;
        }
        if (g_Pbg3Archives != NULL)
        {
            for (pbg3Idx = 0; pbg3Idx < 0x10; pbg3Idx += 1)
            {
                if (g_Pbg3Archives[pbg3Idx] != NULL)
                {
                    entryIdx = g_Pbg3Archives[pbg3Idx]->FindEntry(entryname);
                    if (entryIdx >= 0)
                    {
                        break;
                    }
                }
            }
        }
        if (entryIdx < 0)
        {
            return NULL;
        }
    }
    if (entryIdx >= 0)
    {
        utils::DebugPrint2("%s Decode ... \n", entryname);
        data = g_Pbg3Archives[pbg3Idx]->ReadDecompressEntry(entryIdx, entryname);
        g_LastFileSize = g_Pbg3Archives[pbg3Idx]->GetEntrySize(entryIdx);
    }
    else
    {
        char resolved[512];
        ResolvePath(filepath, resolved, sizeof(resolved));
        utils::DebugPrint2("%s Load (-> %s) ... \n", filepath, resolved);
        file = FopenUTF8(filepath, "rb");
        if (file == NULL)
        {
            utils::DebugPrint2("error : %s is not found (resolved: %s).\n", filepath, resolved);
            return NULL;
        }
        else
        {
            std::fseek(file, 0, SEEK_END);
            fsize = std::ftell(file);
            g_LastFileSize = fsize;
            std::fseek(file, 0, SEEK_SET);
            data = (u8 *)std::malloc(fsize);
            std::fread(data, 1, fsize, file);
            std::fclose(file);
        }
    }
    return data;
}

int FileSystem::WriteDataToFile(const char *path, const void *data, size_t size)
{
    FILE *f;

    f = FopenUTF8(path, "wb");
    if (f == NULL)
    {
        return -1;
    }
    else
    {
        if (std::fwrite(data, 1, size, f) != size)
        {
            std::fclose(f);
            return -2;
        }
        else
        {
            std::fclose(f);
            return 0;
        }
    }
}
