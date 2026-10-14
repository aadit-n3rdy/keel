#pragma once

#include "memtable.hpp"

#include "cursor.hpp"
#include "types.hpp"

#include <optional>
#include <shared_mutex>
#include <vector>
#include <random>
#include <atomic>

namespace keel {
    class SkipList : public Memtable
    {
    private:
        typedef uint64_t version_t;

        std::atomic<version_t> next_version;

        struct VersionedValue {
            version_t version;
            keel::Value value;
        };

        struct Node {
            Node* next_node = nullptr;
            Node* higher_node = nullptr;
            Node* lower_node = nullptr;
            Node* lowest_node = nullptr;
            keel::Key key;
            std::vector<VersionedValue> values;
            std::shared_mutex mutex;

            Node(keel::Key p_key, version_t version, keel::Value p_value);
            Node(keel::Key p_key);

            bool hasValidVersion(version_t next_version);
            int32_t searchVersion(version_t next_version);
        };

        std::vector<Node*> layers;
        std::default_random_engine prng;

        inline Node *drillDown(Node *cur) {
            return cur->lowest_node;
        }

        void insertFirst(keel::Key const key, version_t version, keel::Value const value);
        void insertFirstAtLayer(Node* node, std::size_t layer);

        void insert(keel::Key const key, version_t version, keel::Value const value, Node* prevNode);
        void insertAtLayer(Node* node, std::size_t layer, Node *prevNode);

        class SlCursor: public keel::Cursor<const KVPair> {
        private:
            Node* startNode;
            Node* curNode;
            version_t next_version;
            Key limit;
        public:
            inline SlCursor(Node* p_startNode, version_t p_version) : startNode(p_startNode), curNode(p_startNode), next_version(p_version), limit(Key()) {};
            inline SlCursor(Node* p_startNode, version_t p_version, Key p_limit) : startNode(p_startNode), curNode(p_startNode), next_version(p_version), limit(p_limit) {};
            std::optional<const KVPair> getNext() override;
            ~SlCursor() override;
        };


    public:
        SkipList();
        std::optional<const keel::Value> getOne(keel::Key key) override;
        void set(keel::Key const key, keel::Value const value) override;
        keel::SkipList::Node* getClosest(keel::Key const key);
        keel::Cursor<const KVPair>* getAll() override;
        keel::Cursor<const KVPair>* getRange(keel::Key const low, keel::Key const high) override;
        ~SkipList() override ;

        bool verifyInnerHasLower();
        bool verifyHigher();
    };

    void print_key(Key key);
}