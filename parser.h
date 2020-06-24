#pragma once

#include "bit-byte-reader.h"
#include "huff-decoder.h"

struct ChannelInfo {
    int channel_id, horizontal, vertical, quant_id, huff_dc_id, huff_ac_id;

    ChannelInfo() = default;
};

template <typename T>
std::vector<std::vector<T>> VectorToZigZag(const std::vector<T> vect) {
    size_t counter = 0;
    std::vector<std::vector<T>> result(8, std::vector<T>(8));

    for (int sum = 0; sum < 8; ++sum) {
        for (int i = std::max(0, sum - 7); i <= sum && i < 8; ++i) {
            if (sum % 2 == 0) {
                result[sum - i][i] = vect[counter++];
            } else {
                result[i][sum - i] = vect[counter++];
            }
        }
    }
    return result;
}

class Parser {
private:
    std::shared_ptr<ByteBitReader> reader_;

    int ReadNBitNumber(size_t len) {
        if (len == 0) {
            return 0;
        }
        int number = 0;
        for (size_t i = 0; i < len; ++i) {
            auto byte = reader_->GetBit();
            number = (number << 1) + byte;
        }
        if (number >> (len - 1)) {
            return number;
        }
        return number - (1 << len) + 1;
    }

public:
    explicit Parser(std::shared_ptr<ByteBitReader> reader) : reader_(reader) {
    }

    int ParseMarker() {
        int byte = reader_->GetByte();
        if (byte != 255) {
            std::runtime_error("not a marker");
        }
        byte = reader_->GetByte();
        return byte;
    }

    std::string ParseCOM() {
        size_t len = reader_->GetTwoBytes();
        std::string result;
        for (size_t i = 0; i + 2 < len; ++i) {
            result += static_cast<char>(reader_->GetByte());
        }
        return result;
    }

    //dc/ac, id table, code
    std::vector<int> ParseDHT() {
        int value_counter = 0;
        std::vector<int> result(16);
        for (size_t i = 0; i < 16; ++i) {
            result[i] = reader_->GetByte();
            value_counter += result[i];
        }
        while (value_counter > 0) {
            result.push_back(reader_->GetByte());
            value_counter--;
        }
        return result;
    }

    std::vector<std::vector<int64_t>> ParseDQT(bool double_length) {
        std::vector<int64_t> flat_result(64);
        for (size_t iter = 0; iter < 64; ++iter) {
            if (double_length) {
                flat_result[iter] = reader_->GetTwoBytes();
            } else {
                flat_result[iter] = reader_->GetByte();
            }
        }
        return VectorToZigZag(flat_result);
    }

    std::tuple<size_t, size_t, std::vector<ChannelInfo>> ParseSOF0() {
        reader_->SkipBytes(3);

        size_t hight = reader_->GetTwoBytes();
        size_t width = reader_->GetTwoBytes();

        size_t channel_count = reader_->GetByte();

        if (channel_count == 0 || channel_count > 3) {
            throw std::runtime_error("wrong channel count");
        }
        std::vector<ChannelInfo> result(channel_count);

        int max_hor = 0;
        int max_vert = 0;

        for (size_t i = 0; i < channel_count; ++i) {
            result[i].channel_id = reader_->GetByte();
            int byte = reader_->GetByte();

            result[i].horizontal = Left(byte);
            result[i].vertical = Right(byte);

            max_hor = std::max(max_hor, result[i].horizontal);
            max_vert = std::max(max_vert, result[i].vertical);

            result[i].quant_id = reader_->GetByte();
        }

        for (size_t i = 0; i < channel_count; ++i) {
            if (!result[i].horizontal || !result[i].vertical) {
                throw std::runtime_error("thinning shouldn't be 0");
            }
            result[i].horizontal = max_hor / result[i].horizontal;
            result[i].vertical = max_vert / result[i].vertical;
        }

        return {hight, width, result};
    }

    std::vector<ChannelInfo> ParseSOS() {
        reader_->SkipBytes(2);
        size_t channel_count = reader_->GetByteForSOS();
        std::vector<ChannelInfo> result(3);

        for (size_t i = 0; i < channel_count; ++i) {
            result[i].channel_id = reader_->GetByteForSOS();
            int byte = reader_->GetByteForSOS();
            result[i].huff_dc_id = Left(byte);
            result[i].huff_ac_id = Right(byte);
        }

        reader_->SkipBytes(3);

        return result;
    }

    void ParseAPPn() {
        size_t len = reader_->GetTwoBytes();
        reader_->SkipBytes(len - 2);
    }

    std::vector<std::vector<int64_t>> ParseBlock(std::shared_ptr<HTDecoder> dc_huff, std::shared_ptr<HTDecoder> ac_huff) {
        std::vector<int64_t> flat_result(64);

        auto dc_byte = dc_huff->Decode(reader_);
        if (!dc_byte) {
            flat_result[0] = 0;
        } else {
            flat_result[0] = ReadNBitNumber(dc_byte);
        }

        size_t iter = 1;
        while (iter < 64) {
            auto ac_byte = ac_huff->Decode(reader_);
            if (!ac_byte) {
                break;
            }
            iter += Left(ac_byte);
            auto byte = ReadNBitNumber(Right(ac_byte));
            flat_result[iter++] = byte;
        }

        return VectorToZigZag(flat_result);
    }
};