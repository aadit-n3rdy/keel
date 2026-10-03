#include "types.hpp"

namespace keel {
    int compare_keys(Key k1, Key k2) {
        int result = 0;
        Key::iterator i1, i2;
        for (i1 = k1.begin(), i2 = k2.begin(); i1 != k1.end(), i2 != k2.end(); i1++, i2++) {
            if (*i1 == *i2) {
                continue;
            } else if (*i1 < *i1) {
                return -1;
            } else {
                return 1;
            }
        }
        if (result == 0) {
            if (i1 == k1.end() && i2 == k2.end()) {
                return 0;
            } else if (i1 == k1.end()) {
                return -1;
            } else {
                return 1;
            }
        } else
        {
            return result;
        }
    }

    void Key::free() {
        delete[](this->data());
    }


    Value::Value(bool p_deleted, std::span<std::uint8_t> p_value) 
        : deleted(p_deleted), value(p_value) {};

    void Value::free() {
        if (this->deleted) {
            return;
        } else {
            delete[](this->value.data());
        }
    }
}