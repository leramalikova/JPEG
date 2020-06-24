#pragma once

#include <istream>

int Left(int byte) {
    return (byte & 240) >> 4;
}

int  Right(int byte) {
    return byte & 15;
}

class ByteBitReader {
public:
    explicit  ByteBitReader(std::istream &fin) : fin_(fin), last_byte_(0), bit_index_(8) {
    }

    int GetByte() {
        if (fin_.eof()) {
            throw std::runtime_error("couldn't get byte");
        }
        return fin_.get();
    }

    void SkipBytes(size_t n) {
        fin_.ignore(n);
    }

    int GetByteForSOS() {
        if (fin_.eof()) {
            throw std::runtime_error("couldn't get byte");
        }
        auto byte = fin_.get();
        // FF FF = FF
        if (!fin_.eof() && byte == 255 && fin_.peek() == 255) {
            byte = fin_.get();
        }
        return byte;
    }

    size_t GetTwoBytes() {
        auto result = static_cast<size_t>(GetByte()) * 256;
        return result += GetByte();
    }

    size_t GetTwoBytesForSOS() {
        auto result = static_cast<size_t>(GetByteForSOS()) * 256;
        return result += GetByteForSOS();
    }

    bool GetBit() {
        // 8 как раз обзначает что прошлый байт полностью побитово отдан
        if (bit_index_ == 8) {
            last_byte_ = GetByte();
            // FF 00 = 00
            if (!fin_.eof() && last_byte_ == 255 && fin_.peek() == 0) {
                last_byte_ = fin_.get();
                last_byte_ = 255;
            }
            bit_index_ = 0;
        }
        auto result = static_cast<bool>((last_byte_ & (1 << (7 - bit_index_))) >> (7 - bit_index_));
        bit_index_++;
        return result;
    }

    void SetEndOfBitReading() {
        bit_index_ = 8;
    }

private:
    std::istream &fin_;
    int last_byte_;
    int bit_index_;
};
