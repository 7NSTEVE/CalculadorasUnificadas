#pragma once

#include "doc_types.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace DocStudio {

// Quadtree spatial index in double-precision coordinates for Infinite Canvas (100k+ objects).
class QuadtreeIndex {
public:
    explicit QuadtreeIndex(
        DocRect worldBounds = { -1e6, -1e6, 2e6, 2e6 },
        int maxDepth = 10,
        std::size_t splitThreshold = 24);

    void Clear();
    void Insert(std::uint64_t objectId, const DocRect& bounds);
    bool Remove(std::uint64_t objectId);
    void Update(std::uint64_t objectId, const DocRect& newBounds);

    std::vector<std::uint64_t> QueryRect(const DocRect& viewRect) const;
    std::size_t Size() const noexcept { return entries_.size(); }

private:
    struct Node {
        DocRect bounds{};
        int depth = 0;
        std::vector<std::uint64_t> ids;
        std::unique_ptr<Node> children[4];
        bool isLeaf = true;
    };

    DocRect initialBounds_{};
    DocRect rootBounds_{};
    int maxDepth_ = 10;
    std::size_t splitThreshold_ = 24;
    std::unique_ptr<Node> root_;
    std::unordered_map<std::uint64_t, DocRect> entries_;

    void EnsureBoundsContain(const DocRect& r);
    void InsertIntoNode(Node* node, std::uint64_t id, const DocRect& r);
    void Subdivide(Node* node);
    void QueryNode(const Node* node, const DocRect& viewRect, std::vector<std::uint64_t>& out) const;
    bool RemoveFromNode(Node* node, std::uint64_t id, const DocRect& r);
};

} // namespace DocStudio
