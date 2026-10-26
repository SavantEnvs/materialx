// Fuzz image decoding (stb_image) through MaterialX's StbImageLoader; the first byte picks
// the .hdr (float) or generic (8-bit) path since the loader dispatches on file extension.
#include <MaterialXRender/StbImageLoader.h>
#include "tmpfile.h"

#include <cstring>

namespace mx = MaterialX;

// stb allocates the full declared GIF/PNG canvas before it reads any pixel data, so a tiny header
// declaring a huge canvas is an allocation-size artifact (libFuzzer OOM), not a decoder defect.
static bool declaresHugeCanvas(const uint8_t* d, size_t n)
{
    uint64_t w = 0, h = 0;
    if (n >= 10 && (!memcmp(d, "GIF87a", 6) || !memcmp(d, "GIF89a", 6)))
    {
        w = d[6] | (d[7] << 8);
        h = d[8] | (d[9] << 8);
    }
    else if (n >= 24 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8))
    {
        w = ((uint64_t) d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19];
        h = ((uint64_t) d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23];
    }
    return w * h > (1u << 24);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
        return -1;
    if (declaresHugeCanvas(data + 1, size - 1))
        return -1;
    bool hdr = (data[0] & 1) != 0;
    ScratchFile file(data + 1, size - 1, hdr ? ".hdr" : ".png");
    if (file.path.empty())
        return -1;
    mx::StbImageLoaderPtr loader = mx::StbImageLoader::create();
    mx::ImagePtr image = loader->loadImage(mx::FilePath(file.path));
    if (!image)
        return -1;
    if (image->getWidth() == 0 || image->getHeight() == 0)
        return 0;
    image->getMaxMipCount();
    image->getChannelCount();
    if (image->getWidth() * (size_t) image->getHeight() <= (1u << 16))
    {
        image->getAverageColor();
        image->getTexelColor(0, 0);
        image->copy(4, mx::Image::BaseType::FLOAT);
        image->applyBoxBlur();
        image->applyGaussianBlur();
    }
    return 0;
}
