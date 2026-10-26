// Fuzz MaterialX document parsing, version upgrade, validation, traversal and serialization.
#include <MaterialXCore/Document.h>
#include <MaterialXFormat/XmlIo.h>

#include <string>

namespace mx = MaterialX;

static void noInclude(mx::DocumentPtr, const mx::FilePath&, const mx::FileSearchPath&, const mx::XmlReadOptions*) {}

static void exercise(mx::DocumentPtr doc)
{
    std::string message;
    doc->validate(&message);

    for (mx::ElementPtr elem : doc->traverseTree())
    {
        if (mx::ValueElementPtr ve = elem->asA<mx::ValueElement>())
        {
            ve->getValue();
            ve->getResolvedValueString();
            ve->getDefaultValue();
        }
        if (mx::NodePtr node = elem->asA<mx::Node>())
        {
            node->getNodeDef();
            node->getImplementation();
            node->getDownstreamPorts();
        }
        elem->getNamePath();
        elem->getActiveColorSpace();
    }

    for (mx::NodeGraphPtr graph : doc->getNodeGraphs())
    {
        graph->getDownstreamPorts();
        graph->getNodeDef();
        graph->topologicalSort();
        graph->getMaterialOutputs();
    }
    for (mx::NodePtr node : doc->getMaterialNodes())
    {
        mx::getShaderNodes(node);
        mx::getGeometryBindings(node);
    }
    for (mx::LookPtr look : doc->getLooks())
    {
        look->getActiveVariantAssigns();
        look->getActivePropertySetAssigns();
    }
    doc->getMatchingPorts("");
    doc->getUnitTypeDefs();
    doc->getReferencedSourceUris();

    for (mx::NodeDefPtr nd : doc->getNodeDefs())
    {
        nd->getActiveInputs();
        nd->getImplementation();
        doc->getMatchingNodeDefs(nd->getNodeString());
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::string text(reinterpret_cast<const char*>(data), size);
    mx::XmlReadOptions options;
    options.readXIncludeFunction = noInclude;
    options.readComments = (size & 1) != 0;
    options.readNewlines = (size & 2) != 0;

    mx::DocumentPtr doc = mx::createDocument();
    try
    {
        mx::readFromXmlBuffer(doc, text.c_str(), mx::FileSearchPath(), &options);
    }
    catch (mx::Exception&)
    {
        return -1;
    }

    try
    {
        exercise(doc);

        mx::XmlWriteOptions wopts;
        wopts.writeXIncludeEnable = false;
        std::string out = mx::writeToXmlString(doc, &wopts);

        mx::DocumentPtr copy = mx::createDocument();
        copy->copyContentFrom(doc);
        copy->upgradeVersion();

        mx::DocumentPtr round = mx::createDocument();
        mx::readFromXmlBuffer(round, out.c_str(), mx::FileSearchPath(), &options);
    }
    catch (mx::Exception&)
    {
    }
    return 0;
}
