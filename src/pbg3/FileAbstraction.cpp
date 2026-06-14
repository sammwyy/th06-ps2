#include "pbg3/FileAbstraction.hpp"
#include "FileSystem.hpp"

#include <cstdlib>
#include <cstring>

FileAbstraction::FileAbstraction()
{
    handle = NULL;
    access = ACCESS_INVALID;
    buffer = NULL;
    bufferSize = 0;
    bufferPos = 0;
}

i32 FileAbstraction::Open(const char *filename, const char *mode)
{
    char openMode[] = "*b";

    this->Close();

    const char *curMode;
    for (curMode = mode; *curMode != '\0'; curMode += 1)
    {
        if (*curMode == 'r')
        {
            this->access = ACCESS_READ;
            openMode[0] = 'r';
            break;
        }
        else if (*curMode == 'w')
        {
            this->access = ACCESS_WRITE;
            openMode[0] = 'w';
            break;
        }
        else if (*curMode == 'a')
        {
            this->access = ACCESS_WRITE;
            openMode[0] = 'a';
            break;
        }
    }

    if (*curMode == '\0')
    {
        return 0;
    }

    std::FILE *file = FileSystem::FopenUTF8(filename, openMode);
    if (file == NULL)
    {
        return 0;
    }

    // For reads, slurp the whole file into RAM and close the handle right away.
    // The PS2 cdvd only allows a couple of files open at once, and the PBG3
    // archives are otherwise kept open for the whole game, which makes opening a
    // second archive (e.g. a stage's while the menu's is held) fail on disc.
    if (this->access == ACCESS_READ)
    {
        std::fseek(file, 0, SEEK_END);
        long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        if (size < 0)
        {
            std::fclose(file);
            return 0;
        }

        this->buffer = (u8 *)std::malloc((size_t)size != 0 ? (size_t)size : 1);
        if (this->buffer == NULL)
        {
            std::fclose(file);
            return 0;
        }

        if (size > 0 && std::fread(this->buffer, 1, (size_t)size, file) != (size_t)size)
        {
            std::fclose(file);
            std::free(this->buffer);
            this->buffer = NULL;
            return 0;
        }

        std::fclose(file);
        this->bufferSize = (u32)size;
        this->bufferPos = 0;
        return 1;
    }

    this->handle = file;
    return 1;
}

void FileAbstraction::Close()
{
    if (this->handle != NULL)
    {
        std::fclose(this->handle);
        this->handle = NULL;
    }
    if (this->buffer != NULL)
    {
        std::free(this->buffer);
        this->buffer = NULL;
    }
    this->bufferSize = 0;
    this->bufferPos = 0;
    this->access = ACCESS_INVALID;
}

i32 FileAbstraction::Read(u8 *data, u32 dataLen, u32 *numBytesRead)
{
    if (this->access != ACCESS_READ)
    {
        return false;
    }

    if (this->buffer != NULL)
    {
        u32 available = this->bufferSize - this->bufferPos;
        u32 toRead = dataLen < available ? dataLen : available;
        std::memcpy(data, this->buffer + this->bufferPos, toRead);
        this->bufferPos += toRead;
        *numBytesRead = toRead;
        return !(dataLen != 0 && *numBytesRead < dataLen);
    }

    *numBytesRead = std::fread(data, 1, dataLen, this->handle);

    return !(dataLen != 0 && *numBytesRead < dataLen);
}

i32 FileAbstraction::Write(const u8 *data, u32 dataLen, u32 *outWritten)
{
    if (this->access != ACCESS_WRITE)
    {
        return false;
    }

    *outWritten = std::fwrite(data, 1, dataLen, this->handle);

    return !(dataLen != 0 && *outWritten < dataLen);
}

i32 FileAbstraction::ReadByte()
{
    u8 data;
    u32 outBytesRead;

    if (!this->Read(&data, 1, &outBytesRead))
    {
        return -1;
    }
    else
    {
        if (outBytesRead == 0)
        {
            return -1;
        }
        return data;
    }
}

i32 FileAbstraction::WriteByte(u32 b)
{
    u8 outByte;
    u32 outBytesWritten;

    outByte = b;
    if (!this->Write(&outByte, 1, &outBytesWritten))
    {
        return -1;
    }
    else
    {
        if (outBytesWritten == 0)
        {
            return -1;
        }
        return b;
    }
}

i32 FileAbstraction::Seek(u32 amount, u32 seekFrom)
{
    if (this->buffer != NULL)
    {
        u32 newPos;
        if (seekFrom == SEEK_SET)
        {
            newPos = amount;
        }
        else if (seekFrom == SEEK_CUR)
        {
            newPos = this->bufferPos + amount;
        }
        else if (seekFrom == SEEK_END)
        {
            newPos = this->bufferSize + amount;
        }
        else
        {
            return 0;
        }
        this->bufferPos = newPos > this->bufferSize ? this->bufferSize : newPos;
        return 1;
    }

    if (this->handle == NULL)
    {
        return 0;
    }

    std::fseek(this->handle, amount, seekFrom);
    return 1;
}

u32 FileAbstraction::Tell()
{
    if (this->buffer != NULL)
    {
        return this->bufferPos;
    }

    if (this->handle == NULL)
    {
        return 0;
    }

    return std::ftell(this->handle);
}

u32 FileAbstraction::GetSize()
{
    if (this->buffer != NULL)
    {
        return this->bufferSize;
    }

    if (this->handle == NULL)
    {
        return 0;
    }

    long curPos = std::ftell(this->handle);
    std::fseek(this->handle, 0, SEEK_END);
    u32 fileLen = (u32)std::ftell(this->handle);
    std::fseek(this->handle, curPos, SEEK_SET);

    return fileLen;
}

u8 *FileAbstraction::ReadWholeFile(u32 maxSize)
{
    if (this->access != ACCESS_READ)
    {
        return NULL;
    }

    u32 dataLen = this->GetSize();
    u32 outDataLen;
    if (dataLen <= maxSize)
    {
        u8 *data = new u8[dataLen];
        if (data != NULL)
        {
            u32 oldLocation = this->Tell();
            // Pretty sure the plan here was to seek to 0, but woops the code
            // is buggy.
            if (this->Seek(oldLocation, SEEK_SET) != 0)
            {
                if (this->Read(data, dataLen, &outDataLen) == 0)
                {
                    delete[] data;
                    return NULL;
                }
                this->Seek(oldLocation, SEEK_SET);
                return data;
            }
            // Yes, this case leaks the data. Amazing, I know.
        }
    }
    return NULL;
}

FileAbstraction::~FileAbstraction()
{
    this->Close();
}
