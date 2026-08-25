#include "../RuntimeConfig.h"

#include "gtest/gtest.h"

using namespace cali;

namespace
{

const char* json_spec = R"json(
{
 "name": "test",
 "config":
 [
  { 
   "name": "string_val",
   "type": "string",
   "value": "string-default"
  },{ 
   "name": "list_val",
   "type": "string",
   "value": "first, second, \"third,but not fourth\""
  },{ 
   "name": "int_val",
   "type": "int",
   "value": "1337"
  },{ 
   "name": "another_int",
   "type": "int",
   "value": "4242"
  }
 ]
}
)json";

} // namespace

TEST(RuntimeConfigTest, ConfigFile)
{
    RuntimeConfig cfg;

    cfg.set("CALI_CONFIG_FILE", "caliper-common_test.config");

    cfg.preset("CALI_TEST_STRING_VAL", "wrong value!");
    cfg.set("CALI_TEST_INT_VAL", "42");

    ConfigSet config = cfg.from_spec(::json_spec);

    EXPECT_EQ(config.get("string_val").to_string(), std::string("profile1 string from file"));
    EXPECT_EQ(config.get("int_val").to_int(), 42);
    EXPECT_EQ(config.get("another_int").to_int(), 4242);
}

TEST(RuntimeConfigTest, ConfigFileProfile2)
{
    RuntimeConfig cfg;

    cfg.preset("CALI_CONFIG_FILE", "caliper-common_test.config");
    cfg.set("CALI_CONFIG_PROFILE", "file-profile2");

    ConfigSet config = cfg.from_spec(::json_spec);

    EXPECT_EQ(config.get("string_val").to_string(), std::string("string-default"));
    EXPECT_EQ(config.get("int_val").to_int(), 42);
}
