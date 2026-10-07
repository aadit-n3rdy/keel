#pragma once

#include "memtable.hpp"

#include "keel.hpp"

#include "cursor.hpp"
#include "types.hpp"

#include <optional>
#include <vector>

#include <random>
#include <iostream>

namespace keel {
    class SkipList : public Memtable
    {
    private:
        struct Node {
            Node* next_node;
            Node* higher_node;
            Node* lower_node;
            keel::Key key;
            std::optional<keel::Value> value;

            Node(keel::Key p_key, keel::Value p_value);
        };
        std::vector<Node*> layers;
        std::default_random_engine prng;

        inline Node *drillDown(Node *cur) {
            if (cur == nullptr)
            {
                return nullptr;
            }
            while (cur->lower_node != nullptr)
            {
                cur = cur->lower_node;
            }
            return cur;
        }

        void insertFirst(keel::Key const key, keel::Value const value);
        void insertFirstAtLayer(Node* node, std::size_t layer);

        void insert(keel::Key const key, keel::Value const value, Node* prevNode);
        void insertAtLayer(Node* node, std::size_t layer, Node *prevNode);

        class Cursor: public keel::Cursor<const KVPair> {
        private:
            const Node* startNode;
            const Node* curNode;
        public:
            inline Cursor(const Node* p_startNode) : startNode(p_startNode), curNode(p_startNode) {};
            std::optional<const KVPair> getNext() override;
            bool hasNext() override;
            ~Cursor();
        };

    public:
        SkipList();
        std::optional<const keel::Value> getOne(keel::Key key) override;
        void set(keel::Key const key, keel::Value const value) override;
        keel::SkipList::Node* getClosest(keel::Key const key);
        keel::Cursor<const KVPair>* getAll() override;
        ~SkipList() override ;

        bool verifyInnerHasLower();
        bool verifyHigher();
    };

    void print_key(Key key);
}