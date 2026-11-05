#pragma once
#include "BRDF.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ltc {

class BRDFRegistry {
public:
    using Factory = std::function<std::unique_ptr<IBRDF>()>;

    // Register a named BRDF factory. Throws std::invalid_argument if name already registered.
    void registerBRDF(std::string name, Factory factory);

    // Create a new BRDF instance by name. Throws std::invalid_argument if name unknown.
    std::unique_ptr<IBRDF> create(std::string_view name) const;

    // List registered names in registration order.
    std::vector<std::string> names() const;

    bool contains(std::string_view name) const;

    // Create the default registry with all built-in BRDFs registered.
    // Order: ggx, beckmann, disney-diffuse
    static BRDFRegistry createDefault();

private:
    std::map<std::string, Factory> m_factories;  // alphabetical lookup
    std::vector<std::string> m_order;             // registration order
};

} // namespace ltc
