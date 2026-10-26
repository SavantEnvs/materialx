// Fuzz the OBJ mesh loader (tinyobjloader) through MaterialX's TinyObjLoader.
#include <MaterialXRender/TinyObjLoader.h>
#include "tmpfile.h"

namespace mx = MaterialX;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ScratchFile file(data, size, ".obj");
    if (file.path.empty())
        return -1;
    mx::MeshList meshes;
    mx::TinyObjLoaderPtr loader = mx::TinyObjLoader::create();
    if (!loader->load(mx::FilePath(file.path), meshes))
        return -1;
    for (mx::MeshPtr mesh : meshes)
    {
        mesh->getMinimumBounds();
        mesh->getMaximumBounds();
        mesh->getSphereRadius();
        for (size_t i = 0; i < mesh->getPartitionCount(); i++)
            mesh->getPartition(i)->getFaceCount();
    }
    return 0;
}
