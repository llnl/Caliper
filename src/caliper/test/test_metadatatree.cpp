// Tests for the metadata tree

#include "../MetadataTree.h"

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"

#include <gtest/gtest.h>

using namespace cali;
using namespace cali::internal;

TEST(MetadataTreeTest, BigTree)
{
    // just create a lot of nodes
    Caliper c;

    Attribute str_attr = c.create_attribute("test.metatree.replaceall.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute int_attr = c.create_attribute("test.metatree.replaceall.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    const std::string chars { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" };

    MetadataTree tree;
    Node*        node = tree.root();

    for (int i = 0; i < 400000; ++i) {
        std::string sval = chars.substr(i % (chars.length() / 2));

        node = tree.get_child(str_attr, Variant(CALI_TYPE_STRING, sval.data(), sval.size()), node);
        node = tree.get_child(int_attr, Variant(2 * i + 1), node);

        ASSERT_NE(node, nullptr);
    }

    int str_count = 0;
    int int_count = 0;

    for (; node; node = node->parent()) {
        if (node->attribute() == int_attr.id())
            ++int_count;
        if (node->attribute() == str_attr.id())
            ++str_count;
    }

    EXPECT_EQ(str_count, 400000);
    EXPECT_EQ(int_count, 400000);

    tree.print_statistics(std::cout) << std::endl;
}

TEST(MetadataTreeTest, ReplaceFirstInPath)
{
    Caliper c;

    Attribute str_attr = c.create_attribute("test.metatree.replaceall.str", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    Attribute int_attr = c.create_attribute("test.metatree.replaceall.int", CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    MetadataTree tree;

    // make a tree branch with alternating string / int attributes

    const std::string chars { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" };

    Node* node = tree.root();

    for (int i = 0; i < 2000; ++i) {
        std::string sval = chars.substr(i % (chars.length() / 2));

        node = tree.get_child(str_attr, Variant(CALI_TYPE_STRING, sval.data(), sval.size()), node);
        node = tree.get_child(int_attr, Variant(2 * i + 1), node);

        ASSERT_NE(node, nullptr);
    }

    // add something on top still

    Variant top_int_p[1] = { Variant(43) };
    Variant top_str_p[2] = { Variant(CALI_TYPE_STRING, "notthetop", 10), Variant(CALI_TYPE_STRING, "thetop", 7) };

    node = tree.get_path(int_attr, 1, top_int_p, node);
    node = tree.get_path(str_attr, 2, top_str_p, node);

    ASSERT_NE(node, nullptr);

    Variant v_rpl_int(24);

    node = tree.replace_first_in_path(node, int_attr, v_rpl_int);
    ASSERT_NE(node, nullptr);

    EXPECT_EQ(node->attribute(), int_attr.id());
    EXPECT_EQ(node->data().to_int(), 24);

    int str_count = 0;
    int int_count = 0;

    for (Node* tmp = node; tmp; tmp = tmp->parent()) {
        if (tmp->attribute() == int_attr.id())
            ++int_count;
        if (tmp->attribute() == str_attr.id())
            ++str_count;
    }

    EXPECT_EQ(str_count, 2002);
    EXPECT_EQ(int_count, 2001);

    tree.print_statistics(std::cout) << std::endl;
}
