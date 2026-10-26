// Scratch-file helper for harnesses whose library entry point only takes a path.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

struct ScratchFile
{
    std::string path;

    ScratchFile(const uint8_t* data, size_t size, const char* suffix)
    {
        const char* dir = getenv("TMPDIR");
        std::string tmpl = std::string(dir && *dir ? dir : "/tmp") + "/mxfuzz_XXXXXX" + suffix;
        int fd = mkstemps(&tmpl[0], (int) std::char_traits<char>::length(suffix));
        if (fd < 0)
            return;
        size_t off = 0;
        while (off < size)
        {
            ssize_t n = write(fd, data + off, size - off);
            if (n <= 0)
                break;
            off += (size_t) n;
        }
        close(fd);
        if (off == size)
            path = tmpl;
        else
            unlink(tmpl.c_str());
    }
    ~ScratchFile()
    {
        if (!path.empty())
            unlink(path.c_str());
    }
    ScratchFile(const ScratchFile&) = delete;
    ScratchFile& operator=(const ScratchFile&) = delete;
};
