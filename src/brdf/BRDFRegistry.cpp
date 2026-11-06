#include "ltc/BRDFRegistry.h"
#include "ltc/Beckmann.h"
#include "ltc/DisneyDiffuse.h"
#include "ltc/GGX.h"
#include <stdexcept>

namespace ltc {

void BRDFRegistry::registerBRDF(std::string name, Factory factory)
{
    if (m_factories.count(name))
        throw std::invalid_argument("BRDFRegistry: duplicate name '" + name + "'");
    m_factories[name] = std::move(factory);
    m_order.push_back(name);
}

std::unique_ptr<IBRDF> BRDFRegistry::create(std::string_view name) const
{
    auto it = m_factories.find(std::string(name));
    if (it == m_factories.end())
        throw std::invalid_argument("BRDFRegistry: unknown BRDF '" + std::string(name) + "'");
    return it->second();
}

std::vector<std::string> BRDFRegistry::names() const
{
    return m_order;
}

bool BRDFRegistry::contains(std::string_view name) const
{
    return m_factories.count(std::string(name)) > 0;
}

BRDFRegistry BRDFRegistry::createDefault()
{
    BRDFRegistry r;
    r.registerBRDF("ggx",            [] { return std::make_unique<GGX>(); });
    r.registerBRDF("beckmann",       [] { return std::make_unique<Beckmann>(); });
    r.registerBRDF("disney-diffuse", [] { return std::make_unique<DisneyDiffuse>(); });
    return r;
}

} // namespace ltc
