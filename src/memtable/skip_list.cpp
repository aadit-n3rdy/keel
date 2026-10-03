#include "memtable/skip_list.hpp"

#include "keel.hpp"
#include <optional>

#include <stdexcept>

#include <random>

#include <iostream>


namespace keel {
    SkipList::SkipList() {
        layers = std::vector<Node*>(0);
        prng = std::default_random_engine(1);
    }

    SkipList::Node::Node(Key p_key, Value p_value) : key(p_key), value(p_value) {
        this->lower_node = nullptr;
        this->higher_node = nullptr;
        this->next_node = nullptr;
    }

    keel::SkipList::Node* SkipList::getClosest(Key const key) {
        if (layers.size() == 0) {
            return nullptr;
        }

        Node* cur_node = nullptr;
        std::size_t layer = layers.size();
        bool found_equal = false;
        while (layer > 0 && !found_equal) {
            layer--;

            if (cur_node == nullptr) {
                Node *candidate = this->layers[layer];
                int cmp_result = compare_keys(key, candidate->key);
                if (cmp_result < 0) {
                    continue;
                }  else if (cmp_result == 0) {
                    found_equal = true;
                    cur_node = candidate;
                    break;
                } else {
                    cur_node = candidate;
                }
            }

            while (cur_node->next_node != nullptr) {
                Node* next_node = cur_node->next_node;
                int cmp_result = compare_keys(key, next_node->key);
                if (cmp_result > 0) {
                    break;
                } else if (cmp_result == 0) {
                    cur_node = next_node;
                    found_equal = true;
                    break;
                } else {
                    cur_node = next_node;
                }
            }
        }

        if (found_equal) {
            return drillDown(cur_node);
        } else {
            return cur_node;
        }
    }

    std::optional<const keel::Value> SkipList::getOne(keel::Key const key) {
        Node* closest = getClosest(key);
        if (closest != nullptr && compare_keys(closest->key, key) == 0) {
            return closest->value;
        } else {
            return std::optional<const keel::Value>();
        }
    }

    void SkipList::insertFirst(keel::Key const key, keel::Value const value) {
        // assumes key and value are already owned here
        std::size_t layer = 0;

        Node* cur_node = new Node(key, value);
        cur_node->lower_node = nullptr;
        cur_node->higher_node = nullptr;
        insertFirstAtLayer(cur_node, 0);

        Node* lower_node = nullptr;
        while(prng() % 2 == 0) {
            layer += 1;
            lower_node = cur_node;
            cur_node = new Node(*lower_node);
            cur_node->value = std::optional<Value>();
            insertFirstAtLayer(cur_node, layer);
        }
    }

    /// @brief Inserts `node` into first of layer, creates layer if consecutive
    /// @param node must have key, value, lower set correctly
    /// @param layer will be created if is the next consecutive layer
    void SkipList::insertFirstAtLayer(Node* node, std::size_t layer) {
        if (layer > this->layers.size()) {
            throw std::runtime_error("layer beyond bounds");
        } else if (layer == this->layers.size()) {
            this->layers.push_back(node);
            node->higher_node = nullptr;
        } else {
            node->next_node = this->layers[layer];
            this->layers[layer] = node;
        }
    }

    void SkipList::insert(keel::Key const key, keel::Value const value, Node* prev_node) {
        Node* cur_node = new Node(key, value);
        cur_node->higher_node = prev_node;
        cur_node->lower_node = nullptr;

        insertAtLayer(cur_node, 0, prev_node);

        Node* lower_node = nullptr;
        std::size_t layer = 0;
        while (prng() % 2 == 0) {
            layer++;
            prev_node = (prev_node == nullptr) ? nullptr : prev_node->higher_node;
            lower_node = cur_node;
            cur_node = new Node(*lower_node);
            cur_node->value = std::optional<Value>();
            cur_node->lower_node = lower_node;
            insertAtLayer(cur_node, layer, prev_node);
        }
    }

    /// @brief Inserts `node` into `layer` after `prevNode
    /// @param node must have key, value, lower set correctly
    /// @param layer 
    /// @param prevNode 
    void SkipList::insertAtLayer(Node* node, std::size_t layer, Node *prevNode) {
        if (prevNode == nullptr) {
            return insertFirstAtLayer(node, layer);
        }
        if (layer >= this->layers.size()) {
            throw std::runtime_error("can't insert after non-null node in non-existent layer");
        }

        node->next_node = prevNode->next_node;
        prevNode->next_node = node;
        node->higher_node = prevNode->higher_node;
        return;
    } 

    void SkipList::set(keel::Key const key, keel::Value value) {
        Node* closest = getClosest(key);
        if (closest == nullptr) {
            // insert into start
            Key ownedKey = key.dupe();
            Value ownedValue = value.dupe();
            return this->insertFirst(ownedKey, ownedValue);
        } else if (compare_keys(closest->key, key) == 0) {
            closest->value.value().free();
            closest->value = value.dupe();
        } else {
            Key ownedKey = key.dupe();
            Value ownedValue = value.dupe();
            return this->insert(ownedKey, ownedValue, closest);
        }
    }

    SkipList::~SkipList() {
        // free up all keys and values
        if (layers.size() == 0) {
            return;
        }
        Node* cur_node = layers[0];
        while (cur_node != nullptr) {
            cur_node->key.free();
            if (cur_node->value.has_value()) {
                cur_node->value.value().free();
            }
            cur_node = cur_node->next_node;
        }
        for (auto layer = layers.begin(); layer != layers.end(); layer++) {
            cur_node = *layer;
            while (cur_node->next_node != nullptr) {
                Node* tmp = cur_node->next_node;
                delete(cur_node);
                cur_node = tmp;
            }
            delete(cur_node);
        }
    }
}