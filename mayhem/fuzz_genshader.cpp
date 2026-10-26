// Fuzz shader generation: the first input byte selects the backend, the rest is a MaterialX
// document that is linked against the standard data libraries baked into the image.
#include <MaterialXCore/Document.h>
#include <MaterialXFormat/XmlIo.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXGenShader/DefaultColorManagementSystem.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/ShaderGenerator.h>
#include <MaterialXGenShader/UnitSystem.h>
#include <MaterialXGenShader/Util.h>
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXGenMdl/MdlShaderGenerator.h>
#include <MaterialXGenMsl/MslShaderGenerator.h>
#include <MaterialXGenSlang/SlangShaderGenerator.h>

#include <string>

namespace mx = MaterialX;

namespace
{

struct Backend
{
    mx::ShaderGeneratorPtr generator;
    std::unique_ptr<mx::GenContext> context;
};

struct State
{
    mx::FileSearchPath searchPath;
    mx::DocumentPtr stdlib;
    Backend backends[5];

    State()
    {
        searchPath = mx::getDefaultDataSearchPath();
        stdlib = mx::createDocument();
        mx::loadLibraries({ "libraries" }, searchPath, stdlib);

        mx::UnitConverterRegistryPtr registry = mx::UnitConverterRegistry::create();
        for (const char* name : { "distance", "angle" })
        {
            mx::UnitTypeDefPtr def = stdlib->getUnitTypeDef(name);
            if (def)
                registry->addUnitConverter(def, mx::LinearUnitConverter::create(def));
        }

        mx::ShaderGeneratorPtr gens[5] = {
            mx::GlslShaderGenerator::create(), mx::OslShaderGenerator::create(), mx::MdlShaderGenerator::create(),
            mx::MslShaderGenerator::create(), mx::SlangShaderGenerator::create()
        };
        for (int i = 0; i < 5; i++)
        {
            mx::ShaderGeneratorPtr gen = gens[i];
            mx::ColorManagementSystemPtr cms = mx::DefaultColorManagementSystem::create(gen->getTarget());
            cms->loadLibrary(stdlib);
            gen->setColorManagementSystem(cms);
            mx::UnitSystemPtr units = mx::UnitSystem::create(gen->getTarget());
            gen->setUnitSystem(units);
            units->loadLibrary(stdlib);
            units->setUnitConverterRegistry(registry);

            backends[i].generator = gen;
            backends[i].context = std::make_unique<mx::GenContext>(gen);
            backends[i].context->registerSourceCodeSearchPath(searchPath);
            backends[i].context->registerSourceCodeSearchPath(searchPath.find("libraries/stdlib/genosl/include"));
        }
    }
};

void noInclude(mx::DocumentPtr, const mx::FilePath&, const mx::FileSearchPath&, const mx::XmlReadOptions*) {}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static State state;
    if (size < 2)
        return -1;
    Backend& backend = state.backends[data[0] % 5];
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);

    mx::XmlReadOptions options;
    options.readXIncludeFunction = noInclude;
    mx::DocumentPtr doc = mx::createDocument();
    try
    {
        mx::readFromXmlBuffer(doc, text.c_str(), mx::FileSearchPath(), &options);
        doc->setDataLibrary(state.stdlib);
    }
    catch (mx::Exception&)
    {
        return -1;
    }

    try
    {
        std::string message;
        if (!doc->validate(&message))
            return 0;
        std::vector<mx::TypedElementPtr> elements = mx::findRenderableElements(doc);
        size_t count = 0;
        for (mx::TypedElementPtr elem : elements)
        {
            if (count++ >= 4)
                break;
            backend.generator->generate(elem->getName(), elem, *backend.context);
        }
    }
    catch (mx::Exception&)
    {
    }
    return 0;
}
