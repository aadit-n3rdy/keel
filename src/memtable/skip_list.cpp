#include "memtable/skip_list.hpp"

#include "types.hpp"

#include <cstddef>
#include <format>
#include <mutex>
#include <optional>

#include <shared_mutex>
#include <stdexcept>

#include <random>

#include <cassert>
#include <iostream>

namespace keel {
SkipList::SkipList() {
  layers = std::vector<Node *>(0);
  prng = std::default_random_engine(1);
}

SkipList::Node::Node(Key p_key, version_t p_version, Value p_value)
    : key(p_key), values(1, {p_version, p_value}) {
}

SkipList::Node::Node(Key p_key): key(p_key) {
}

keel::SkipList::Node *SkipList::getClosest(Key const key) {
  if (layers.size() == 0) {
    return nullptr;
  }

  Node *cur_node = nullptr;
  std::size_t layer = layers.size();
  bool found_equal = false;
  while (layer > 0 && !found_equal) {
    layer--;
    if (cur_node == nullptr) {
      Node *candidate = this->layers[layer];
      int cmp_result = compare_keys(key, candidate->key);
      if (cmp_result < 0) {
        // continue with cur_node as nullptr in next layer
        continue;
      } else if (cmp_result == 0) {
        found_equal = true;
        cur_node = candidate;
        break;
      } else {
        cur_node = candidate;
      }
    }

    while (cur_node->next_node != nullptr) {
      Node *next_node = cur_node->next_node;
      int cmp_result = compare_keys(key, next_node->key);
      if (cmp_result < 0) {
        break;
      } else if (cmp_result == 0) {
        cur_node = next_node;
        found_equal = true;
        break;
      } else {
        cur_node = next_node;
      }
    }
    if (layer != 0) {
      cur_node = cur_node->lower_node;
    }
  }

  if (found_equal) {
    return drillDown(cur_node);
  } else {
    return cur_node;
  }
}

std::optional<const keel::Value> SkipList::getOne(keel::Key const key) {
  Node *closest = getClosest(key);
  if (closest != nullptr && compare_keys(closest->key, key) == 0) {
    std::shared_lock lockValues(closest->mutex);
    std::optional<const keel::Value> result = closest->values.back().value;
    return result;
  } else {
    return std::optional<const keel::Value>();
  }
}

void SkipList::insertFirst(keel::Key const key, version_t version,
                           keel::Value const value) {
  // assumes key and value are already owned here
  std::size_t layer = 0;

  Node *lowest_node;

  Node *cur_node = new Node(key, version, value);
  cur_node->lower_node = nullptr;
  cur_node->higher_node = nullptr;
  lowest_node = cur_node;
  cur_node->lowest_node = cur_node;
  insertFirstAtLayer(cur_node, 0);

  Node *lower_node = nullptr;
  while (prng() % 2 == 0) {
    layer += 1;
    lower_node = cur_node;
    cur_node = new Node(lower_node->key);
    cur_node->lower_node = lower_node;
    cur_node->lowest_node = lowest_node;
    insertFirstAtLayer(cur_node, layer);
    lower_node->higher_node = cur_node;
  }
}

/// @brief Inserts `node` into first of layer, creates layer if consecutive
//// sets higher and next always
/// @param node must have key, value, lower set correctly
/// @param layer will be created if is the next consecutive layer
void SkipList::insertFirstAtLayer(Node *node, std::size_t layer) {
  if (layer > this->layers.size()) {
    throw std::runtime_error("layer beyond bounds");
  } else if (layer == this->layers.size()) {
    node->higher_node = nullptr;
    node->next_node = nullptr;
    this->layers.push_back(node);
  } else {
    node->next_node = this->layers[layer];
    node->higher_node = nullptr;
    this->layers[layer] = node;
  }
}

void SkipList::insert(keel::Key const key, version_t version,
                      keel::Value const value, Node *prev_node) {
  Node *cur_node = new Node(key, version, value);
  cur_node->lower_node = nullptr;
  cur_node->lowest_node = cur_node;
  Node *lowest_node = cur_node;

  insertAtLayer(cur_node, 0, prev_node);

  Node *lower_node = nullptr;
  std::size_t layer = 0;
  while (prng() % 2 == 0) {
    layer++;
    prev_node = (prev_node == nullptr) ? nullptr : prev_node->higher_node;
    lower_node = cur_node;
    cur_node = new Node(lower_node->key);
    cur_node->lowest_node = lowest_node;
    cur_node->lower_node = lower_node;
    insertAtLayer(cur_node, layer, prev_node);
    lower_node->higher_node = cur_node;
  }
}

/// @brief Inserts `node` into `layer` after `prevNode`. sets higher and next
/// always.
/// @param node must have key, value, lower set correctly
/// @param layer
/// @param prevNode
void SkipList::insertAtLayer(Node *node, std::size_t layer, Node *prevNode) {
  if (prevNode == nullptr) {
    return insertFirstAtLayer(node, layer);
  }
  if (layer >= this->layers.size()) {
    throw std::runtime_error(
        "can't insert after non-null node in non-existent layer");
  }

  node->next_node = prevNode->next_node;
  node->higher_node = prevNode->higher_node;
  prevNode->next_node = node;
  return;
}

void SkipList::set(keel::Key const key, keel::Value const value) {
  version_t version = next_version++;
  Node *closest = getClosest(key);
  if (closest == nullptr) {
    // insert into start
    Key ownedKey = key.dupe();
    Value ownedValue = value.dupe();
    return this->insertFirst(ownedKey, version, ownedValue);
  } else if (compare_keys(closest->key, key) == 0) {
    std::unique_lock lockVersions(closest->mutex);
    closest->values.push_back(VersionedValue(version, value.dupe()));
  } else {
    Key ownedKey = key.dupe();
    Value ownedValue = value.dupe();
    return this->insert(ownedKey, version, ownedValue, closest);
  }
}

SkipList::~SkipList() {
  // free up all keys and values
  if (layers.size() == 0) {
    return;
  }
  Node *cur_node = layers[0];
  while (cur_node != nullptr) {
    // #ifndef NDEBUG
    //   assert(cur_node->mutex.try_lock_shared());
    //   cur_node->mutex.unlock_shared();

    //   assert(cur_node->mutex.try_lock());
    //   cur_node->mutex.unlock();
    // #endif
    cur_node->key.free();
    for (auto it = cur_node->values.begin(); it != cur_node->values.end();
         it++) {
      it->value.free();
    }
    cur_node->values.clear();
    cur_node = cur_node->next_node;
  }
  for (auto layer = layers.begin(); layer != layers.end(); layer++) {
    cur_node = *layer;
    while (cur_node->next_node != nullptr) {
      Node *tmp = cur_node->next_node;
      delete (cur_node);
      cur_node = tmp;
    }
    delete (cur_node);
  }
}

// Test-related functions

void print_key(Key key) {
  std::cout << "{ ";
  for (auto i : key) {
    std::cout << std::format("{:x}", i) << ' ';
  }
  std::cout << "}";
}

bool SkipList::verifyInnerHasLower() {
  bool result = true;
  for (size_t i = 1; i < layers.size(); i++) {
    Node *cur_node = layers[i];
    while (cur_node != nullptr) {
      if (cur_node->lower_node == nullptr) {
        std::cout << "lower_node was null at key ";
        print_key(cur_node->key);
        std::cout << " at layer " << i << '\n';
        result = false;
      } else {
        if (compare_keys(cur_node->key, cur_node->lower_node->key) != 0) {
          std::cout << "key did not match lower node key at layer " << i
                    << "cur_key: ";
          print_key(cur_node->key);
          std::cout << " lower_key: ";
          print_key(cur_node->lower_node->key);
          std::cout << '\n';
          result = false;
        }
      }
      cur_node = cur_node->next_node;
    }
  }
  return result;
}

keel::Cursor<const KVPair> *SkipList::getAll() {
  return new SkipList::SlCursor(this->layers[0], next_version);
}

keel::Cursor<const KVPair> *SkipList::getRange(Key low, Key high) { // NOLINT
    version_t version = next_version;
    Node* closest = getClosest(low);
    if (closest == nullptr) {
        // return empty cursor
        return new SkipList::SlCursor(nullptr, 0);
    }
    bool crossed_boundary = (compare_keys(closest->key, low) >= 0);
    while (closest != nullptr) {
        if (!crossed_boundary) {
            crossed_boundary =  (compare_keys(closest->key, low) >= 0);
        }
        if (crossed_boundary && closest->hasValidVersion(version)) {
            // next node has valid value for required version constraint, and is >= low
            break;
        }
        closest = closest->next_node;
    }
  return new SkipList::SlCursor(closest, version, high);
}

bool SkipList::Node::hasValidVersion(version_t validate_next_version) {
    return (!values.empty() && values.front().version < validate_next_version);
}


int32_t SkipList::Node::searchVersion(version_t validate_next_version) {
    if (!hasValidVersion(validate_next_version)) {
        return -1;
    }
    int32_t lo = 0;
    int32_t hi = static_cast<int32_t>(values.size()-1);

    while (lo > hi) {
        int32_t mid = (lo+hi)/2;
        version_t cur_ver = values[static_cast<uint32_t>(mid)].version;
        if (cur_ver < validate_next_version) {
            // include [mid:hi]
            lo = mid;
        } else {
            // include [lo:mid)
            hi = mid-1;
        }
    }
    return lo;
}


std::optional<const KVPair> SkipList::SlCursor::getNext() {
  if (curNode == nullptr) {
    return std::optional<const KVPair>();
  }
  Node* nextNode = curNode->next_node;
  bool found = false;
  while (!found && nextNode != nullptr) {
    #ifndef NDEBUG
    {
      std::shared_lock lockValues(nextNode->mutex);
      assert(nextNode->values.size() > 0);
    }
    #endif


    if (!limit.empty() && compare_keys(nextNode->key, limit) >= 0) {
      nextNode = nullptr;
      break;
    }
    version_t next_min_version;
    {
      std::shared_lock lockVersions(nextNode->mutex);
      next_min_version = nextNode->values.front().version;
    }
    if (next_min_version >= next_version) {
        nextNode = nextNode->next_node;
        continue;
    } else {
        found = true;
    }
  }
    int32_t idx = 0;
    {
    std::shared_lock lockVersions(curNode->mutex);
    idx = curNode->searchVersion(next_version);
          const KVPair result =
        KVPair(curNode->key, curNode->values[static_cast<uint32_t>(idx)].value);
        curNode = nextNode;
        return result;
    }
}

SkipList::SlCursor::~SlCursor() {}
} // namespace keel