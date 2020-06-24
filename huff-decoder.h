#pragma once

#include <vector>
#include <memory>
#include "bit-byte-reader.h"

class HTNode {
public:
    HTNode *left;
    HTNode *right;
    HTNode *parent;
    size_t hight;
    bool filled;

    HTNode(size_t hh, HTNode *parent, bool filled = false) : left(nullptr), right(nullptr), parent(parent),
                                                     hight(hh), filled(filled) {
    }

    virtual ~HTNode() = default;
};

void UpdateFlags(HTNode *ptr) {
    if (!ptr) {
        return;
    }
    ptr->filled = ptr->left && ptr->right && ptr->left->filled && ptr->right->filled;
    UpdateFlags(ptr->parent);
}

class HTLeaf :  public HTNode {
public:
    HTLeaf(size_t hh, HTNode *parent, int value) : HTNode(hh, parent, true),
                                                       value(value) {
    }

    int value;
};

class HTDecoder {
private:
    HTNode *htree;

    void Clear(HTNode *ptr) {
        if (!ptr) {
            return;
        }
        Clear(ptr->left);
        Clear(ptr->right);
        delete ptr;
    }

    void Add(size_t length, int value) {
        if (htree->filled) {
            throw std::runtime_error("no space for new huff code");
        }

        auto ptr = htree;
        while (ptr->hight + 1 != length) {
            if (!ptr->left) {
                ptr->left = new HTNode(ptr->hight + 1, ptr);
                ptr = ptr->left;
            } else if (!ptr->left->filled) {
                ptr = ptr->left;
            } else if (!ptr->right) {
                ptr->right = new HTNode(ptr->hight + 1, ptr);
                ptr = ptr->right;
            } else {
                ptr = ptr->right;
            }
        }

        if (!ptr->left) {
            ptr->left = new HTLeaf(length, ptr, value);

        } else {
            ptr->right = new HTLeaf(length, ptr, value);
        }
        UpdateFlags(ptr);
    }

public:
    HTDecoder() : htree(nullptr) {
    }

    explicit HTDecoder(std::vector<int> code) {
        htree = new HTNode(0, nullptr);
        size_t value_idx = 16;

        for (size_t length = 1; length < 17; ++length) {
            for (int iter = 0; iter < code[length - 1]; ++iter) {
                Add(length, code[value_idx++]);
            }
        }
    }

    int Decode(std::shared_ptr<ByteBitReader> reader) {
        std::string s = "";
        auto cur_ptr = htree;
        do {
            if (dynamic_cast<HTLeaf*>(cur_ptr)) {
                return dynamic_cast<HTLeaf*>(cur_ptr)->value;
            }

            bool bit = reader->GetBit();
            if (!bit) {
                cur_ptr = cur_ptr->left;
                s += "0";
            } else {
                cur_ptr = cur_ptr->right;
                s += "1";
            }

        } while (cur_ptr);
        throw std::runtime_error("can't decode " + s);
    }

    ~HTDecoder() {
       Clear(htree);
    }
};