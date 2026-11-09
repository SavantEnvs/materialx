// Known-answer probe for the oracle: parses fixed documents and prints computed values.
#include <MaterialXCore/Document.h>
#include <MaterialXFormat/XmlIo.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXGenShader/DefaultColorManagementSystem.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/Util.h>
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXRender/TinyObjLoader.h>

#include <fstream>
#include <iostream>

namespace mx = MaterialX;

int main(int argc, char** argv)
{
    // 1) Core: parse, upgrade a 1.36 document, validate, traverse, round-trip.
    const char* legacy = R"(<?xml version="1.0"?>
<materialx version="1.36">
  <nodedef name="ND_foo" node="foo">
    <input name="a" type="float" value="0.5"/>
    <output name="out" type="color3"/>
  </nodedef>
  <constant name="c1" type="color3"><parameter name="value" type="color3" value="0.25, 0.5, 0.75"/></constant>
  <image name="img" type="color3"><parameter name="file" type="filename" value="a.png"/></image>
  <multiply name="m" type="color3"><input name="in1" type="color3" nodename="c1"/><input name="in2" type="float" value="2"/></multiply>
</materialx>)";
    mx::DocumentPtr doc = mx::createDocument();
    mx::readFromXmlBuffer(doc, legacy);
    std::cout << "version=" << doc->getVersionString() << "\n";
    std::cout << "nodes=" << doc->getNodes().size() << "\n";
    std::cout << "nodedefs=" << doc->getNodeDefs().size() << "\n";
    mx::NodePtr c1 = doc->getNode("c1");
    std::cout << "c1.value=" << (c1 ? c1->getInput("value")->getValueString() : "none") << "\n";
    mx::NodePtr m = doc->getNode("m");
    std::cout << "m.connected=" << (m && m->getConnectedNode("in1") ? m->getConnectedNode("in1")->getName() : "none") << "\n";
    mx::NodePtr img = doc->getNode("img");
    std::cout << "img.file=" << (img ? img->getInput("file")->getValueString() : "none") << "\n";
    std::string xml = mx::writeToXmlString(doc);
    mx::DocumentPtr doc2 = mx::createDocument();
    mx::readFromXmlBuffer(doc2, xml.c_str());
    std::cout << "roundtrip.nodes=" << doc2->getNodes().size() << "\n";

    // 2) Shader generation against the real standard libraries.
    mx::FileSearchPath searchPath = mx::getDefaultDataSearchPath();
    mx::DocumentPtr stdlib = mx::createDocument();
    mx::loadLibraries({ "libraries" }, searchPath, stdlib);
    std::cout << "stdlib.nodedefs>100=" << (stdlib->getNodeDefs().size() > 100) << "\n";
    const char* shaderDoc = R"(<?xml version="1.0"?>
<materialx version="1.39">
  <constant name="k" type="color3"><input name="value" type="color3" value="0.2, 0.4, 0.6"/></constant>
  <surface_unlit name="su" type="surfaceshader"><input name="emission_color" type="color3" nodename="k"/></surface_unlit>
  <surfacematerial name="mat" type="material"><input name="surfaceshader" type="surfaceshader" nodename="su"/></surfacematerial>
</materialx>)";
    mx::DocumentPtr sdoc = mx::createDocument();
    mx::readFromXmlBuffer(sdoc, shaderDoc);
    sdoc->setDataLibrary(stdlib);
    std::cout << "valid=" << sdoc->validate() << "\n";
    std::cout << "renderables=" << mx::findRenderableElements(sdoc).size() << "\n";
    for (int i = 0; i < 2; i++)
    {
        mx::ShaderGeneratorPtr gen = i ? mx::OslShaderGenerator::create() : mx::GlslShaderGenerator::create();
        mx::ColorManagementSystemPtr cms = mx::DefaultColorManagementSystem::create(gen->getTarget());
        cms->loadLibrary(stdlib);
        gen->setColorManagementSystem(cms);
        mx::GenContext ctx(gen);
        ctx.registerSourceCodeSearchPath(searchPath);
        mx::ShaderPtr shader = gen->generate("mat", sdoc->getNode("mat"), ctx);
        const std::string& src = shader->getSourceCode(i ? mx::Stage::PIXEL : mx::Stage::PIXEL);
        std::cout << gen->getTarget() << ".nonempty=" << !src.empty() << "\n";
        std::cout << gen->getTarget() << ".has_0.2=" << (src.find("0.2") != std::string::npos) << "\n";
    }

    // 3) Geometry loader on a fixed OBJ.
    if (argc > 1)
    {
        std::ofstream(argv[1]) << "v 0 0 0\nv 1 0 0\nv 0 2 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n";
        mx::MeshList meshes;
        bool ok = mx::TinyObjLoader::create()->load(mx::FilePath(argv[1]), meshes);
        std::cout << "obj.ok=" << ok << " meshes=" << meshes.size() << "\n";
        if (ok && meshes.size())
        {
            std::cout << "obj.faces=" << meshes[0]->getPartition(0)->getFaceCount() << "\n";
            std::cout << "obj.maxy=" << meshes[0]->getMaximumBounds()[1] << "\n";
        }
    }
    return 0;
}
