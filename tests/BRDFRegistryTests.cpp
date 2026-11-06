#include <gtest/gtest.h>
#include "ltc/BRDFRegistry.h"
#include "ltc/Beckmann.h"
#include "ltc/DisneyDiffuse.h"
#include "ltc/GGX.h"
#include <stdexcept>

using namespace ltc;

TEST(BRDFRegistry, DefaultRegistrationOrder)
{
    auto registry = BRDFRegistry::createDefault();
    auto names = registry.names();
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "ggx");
    EXPECT_EQ(names[1], "beckmann");
    EXPECT_EQ(names[2], "disney-diffuse");
}

TEST(BRDFRegistry, CreateGGX)
{
    auto registry = BRDFRegistry::createDefault();
    auto brdf = registry.create("ggx");
    ASSERT_NE(brdf, nullptr);
    EXPECT_EQ(brdf->name(), "ggx");
}

TEST(BRDFRegistry, CreateBeckmann)
{
    auto registry = BRDFRegistry::createDefault();
    auto brdf = registry.create("beckmann");
    ASSERT_NE(brdf, nullptr);
    EXPECT_EQ(brdf->name(), "beckmann");
}

TEST(BRDFRegistry, CreateDisneyDiffuse)
{
    auto registry = BRDFRegistry::createDefault();
    auto brdf = registry.create("disney-diffuse");
    ASSERT_NE(brdf, nullptr);
    EXPECT_EQ(brdf->name(), "disney-diffuse");
}

TEST(BRDFRegistry, DuplicateRegistrationThrows)
{
    BRDFRegistry r;
    r.registerBRDF("ggx", [] { return std::make_unique<GGX>(); });
    EXPECT_THROW(r.registerBRDF("ggx", [] { return std::make_unique<GGX>(); }),
                 std::invalid_argument);
}

TEST(BRDFRegistry, UnknownNameThrows)
{
    auto registry = BRDFRegistry::createDefault();
    EXPECT_THROW(registry.create("unknown-brdf"), std::invalid_argument);
}

TEST(BRDFRegistry, Contains)
{
    auto registry = BRDFRegistry::createDefault();
    EXPECT_TRUE(registry.contains("ggx"));
    EXPECT_TRUE(registry.contains("beckmann"));
    EXPECT_TRUE(registry.contains("disney-diffuse"));
    EXPECT_FALSE(registry.contains("xyz"));
    EXPECT_FALSE(registry.contains(""));
}

TEST(BRDFRegistry, FactoryCreatesFreshInstances)
{
    auto registry = BRDFRegistry::createDefault();
    auto brdf1 = registry.create("ggx");
    auto brdf2 = registry.create("ggx");
    EXPECT_NE(brdf1.get(), brdf2.get());
}

TEST(BRDFRegistry, EmptyRegistryHasNoNames)
{
    BRDFRegistry r;
    EXPECT_TRUE(r.names().empty());
}

TEST(BRDFRegistry, CustomBRDFRegistration)
{
    BRDFRegistry r;
    r.registerBRDF("ggx", [] { return std::make_unique<GGX>(); });
    r.registerBRDF("beckmann", [] { return std::make_unique<Beckmann>(); });
    auto names = r.names();
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "ggx");
    EXPECT_EQ(names[1], "beckmann");
}
