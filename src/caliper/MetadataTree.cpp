// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// MetadataTree implementation

#include "caliper/caliper-config.h"

#include "MetadataTree.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include "../common/util/spinlock.hpp"

#include <cstring>
#include <utility>

using namespace cali;
using namespace cali::internal;

MetadataTree::GlobalData::GlobalData(MemoryPool& pool)
    : config { RuntimeConfig::get_default_config().from_spec(s_spec) },
      root   { CALI_INV_ID, CALI_INV_ID, Variant() },
      next_block    { 1 },
      node_blocks   { 0 },
      skipped_nodes { 0 },
      g_mempool  { pool }
{
    num_blocks      = config.get("num_blocks").to_uint();
    nodes_per_block = std::min<uint64_t>(config.get("nodes_per_block").to_uint(), 256);

    node_blocks = new NodeBlock[num_blocks];

    Node* chunk = pool.aligned_alloc<Node>(nodes_per_block);

    // --- make the bootstrap nodes

    static const struct { uint64_t id; cali_attr_type type; } bootstrap_type_nodes[] = {
        {  0, CALI_TYPE_USR    },
        {  1, CALI_TYPE_INT    },
        {  2, CALI_TYPE_UINT   },
        {  3, CALI_TYPE_STRING },
        {  4, CALI_TYPE_ADDR   },
        {  5, CALI_TYPE_DOUBLE },
        {  6, CALI_TYPE_BOOL   },
        {  7, CALI_TYPE_TYPE   },
        { 11, CALI_TYPE_PTR    }
    };

    for (const auto &t : bootstrap_type_nodes) {
        Node* node = new (chunk + t.id) Node (t.id, 9, cali_make_variant_from_type(t.type));
        type_nodes[t.type] = node;
    }

    Node* attr_name_node = new (chunk +  8) Node( 8, 8, Variant("cali.attribute.name"));
    Node* attr_type_node = new (chunk +  9) Node( 9, 8, Variant("cali.attribute.type"));
    Node* attr_prop_node = new (chunk + 10) Node(10, 8, Variant("cali.attribute.prop"));
    type_nodes[CALI_TYPE_STRING]->append(attr_name_node);
    type_nodes[CALI_TYPE_TYPE  ]->append(attr_type_node);
    type_nodes[CALI_TYPE_INT   ]->append(attr_prop_node);

    node_blocks[0].chunk = chunk;
    node_blocks[0].index = 12;
}

MetadataTree::GlobalData::~GlobalData()
{
    delete[] node_blocks;
}

MetadataTree::MetadataTree() : m_nodeblock(nullptr), m_num_nodes(0), m_num_blocks(0)
{
    GlobalData* g = mG.load();

    if (!g) {
        GlobalData* new_g = new GlobalData(m_mempool);

        // Set mG. If mG != new_g, some other thread has set it,
        // so just delete our new object.
        if (mG.compare_exchange_strong(g, new_g)) {
            m_nodeblock = new_g->node_blocks;

            ++m_num_blocks;
            m_num_nodes = m_nodeblock->index;
        } else
            delete new_g;
    }
}

MetadataTree::~MetadataTree()
{
    GlobalData* g = mG.load();
    g->g_mempool.merge(m_mempool);
}

bool MetadataTree::have_free_nodeblock()
{
    GlobalData* g = mG.load();

    if (!m_nodeblock || m_nodeblock->index + 1 >= g->nodes_per_block) {
        size_t block_index = g->next_block++;
        if (block_index >= g->num_blocks) {
            g->skipped_nodes++;
            return false;
        }

        Node* chunk = m_mempool.aligned_alloc<Node>(g->nodes_per_block);
        if (!chunk) {
            g->skipped_nodes++;
            return false;
        }

        m_nodeblock = g->node_blocks + block_index;

        m_nodeblock->chunk = chunk;
        m_nodeblock->index = 0;

        ++m_num_blocks;
    }

    return true;
}

//
// --- Modifying tree operations
//

Node* MetadataTree::create_child(cali_id_t attr_id, const Variant& value, Node* parent)
{
    GlobalData* g = mG.load();

    if (!have_free_nodeblock()) {
        ++g->skipped_nodes;
        return root();
    }

    void* ptr = nullptr;

    if (value.has_unmanaged_data()) {
        ptr = m_mempool.allocate(value.size() + 1 /* ensure 0-padding so we can safely hand out string ptrs */);
        if (!ptr) {
            ++g->skipped_nodes;
            return root();
        }
    }

    size_t index = m_nodeblock->index++;
    Node* node = new (m_nodeblock->chunk + index)
        Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, attr_id, value.copy(ptr));

    if (parent)
        parent->append(node);

    ++m_num_nodes;

    return node;
}

Node* MetadataTree::get_path(const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr)
{
    Node* node = parent ? parent : root();
    const cali_id_t attr_id = attr.id();

    size_t i = 0;
    for ( ; i < n; ++i) {
        Node* tmp = node->find_child_node(attr_id, data[i]);
        if (!tmp)
            break;
        node = tmp;
    }
    for ( ; i < n; ++i)
        node = create_child(attr_id, data[i], node);

    return node;
}

Node* MetadataTree::get_path(size_t n, const Node* nodelist[], Node* parent = nullptr)
{
    Node* node = parent ? parent : root();

    for (size_t i = 0; i < n; ++i)
        if (nodelist[i])
            node = get_or_copy_node(nodelist[i], node);

    return node;
}

Node* MetadataTree::get_or_copy_node(const Node* from, Node* parent)
{
    if (!parent)
        parent = root();

    Node* node = parent->find_child_node(from->attribute(), from->data());

    if (!node) {
        GlobalData* g = mG.load();

        if (!have_free_nodeblock()) {
            ++g->skipped_nodes;
            return root();
        }

        size_t index = m_nodeblock->index++;
        node = new (m_nodeblock->chunk + index)
            Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, from->attribute(), from->data());

        parent->append(node);

        ++m_num_nodes;
    }

    return node;
}

Node* MetadataTree::copy_path_without_attribute(cali_id_t attr_id, Node* node, Node* parent)
{
    if (!node || node == parent)
        return parent;

    Node* tmp = copy_path_without_attribute(attr_id, node->parent(), parent);
    if (attr_id != node->attribute())
        tmp = get_or_copy_node(node, tmp);

    return tmp;
}

Node* MetadataTree::remove_first_in_path(Node* path, const Attribute& attr)
{
    Node* node = path;
    cali_id_t attr_id = attr.id();

    for (; node && node->attribute() != attr_id; node = node->parent())
        ;

    if (node)
        node = node->parent();
    if (!node)
        node = root();

    return copy_path_without_attribute(attr_id, path, node);
}

Node* MetadataTree::replace_first_in_path(Node* path, const Attribute& attr, const Variant& data)
{
    return get_child(attr, data, remove_first_in_path(path, attr));
}

Node* MetadataTree::get_child(const Attribute& attr, const Variant& val, Node* parent)
{
    if (!parent)
        parent = root();

    cali_id_t attr_id = attr.id();

    for (Node* node = parent->first_child(); node; node = node->next_sibling())
        if (node->equals(attr_id, val))
            return node;

    return create_child(attr_id, val, parent);
}

void MetadataTree::release()
{
    GlobalData* g = mG.exchange(nullptr);
    auto skipped_nodes = g->skipped_nodes.load();
    if (skipped_nodes > 0) {
        Log().stream() << "Could not create " << skipped_nodes
            << " context tree nodes: Caliper data is likely incomplete/corrupt!" << std::endl;
    }
    delete g;
}

//
// --- I/O
//

std::ostream& MetadataTree::print_statistics(std::ostream& os) const
{
    m_mempool.print_statistics(
        os << "  Metadata tree: " << m_num_blocks << " blocks, " << m_num_nodes << " nodes\n   "
    );

    return os;
}

std::atomic<MetadataTree::GlobalData*> MetadataTree::mG;

const char* MetadataTree::GlobalData::s_spec = R"json(
{
 "name": "contexttree",
 "config":
 [
  {
   "name": "nodes_per_block",
   "type": "uint",
   "value": "256",
   "description": "Number of context tree nodes in a node block"
  },{
   "name": "num_blocks",
   "type": "uint",
   "value": "16384",
   "description": "Number of context tree node blocks"
  }
 ]
}
)json";
