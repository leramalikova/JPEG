#pragma once

#include <vector>
#include <unordered_map>
#include "image.h"

using table = std::vector<std::vector<int64_t>>;
using std::unordered_map;

struct Dimensions {
    size_t hor, vert;

    Dimensions() : hor(0), vert(0) {
    }

    Dimensions(size_t hh, size_t vv) : hor(hh), vert(vv) {
    }
};

template <typename T>
void FillSubTable(std::vector<std::vector<T>> &big_table,
                  const std::vector<std::vector<T>> &small_table, size_t xx, size_t yy) {
    for (size_t i = 0; i < small_table.size(); ++i) {
        for (size_t j = 0; j < small_table[0].size(); ++j) {
            big_table[yy + i][xx + j] = small_table[i][j];
        }
    }
}

class DecodingInfo {
private:
    unordered_map<int, table> quant_tables_;
    unordered_map<int, std::shared_ptr<HTDecoder>> dc_huff_decoders_, ac_huff_decoders_;
    unordered_map<int, ChannelInfo> channel_info_;
    unordered_map<int, int64_t> last_dc_coef_;

    unordered_map<int, std::vector<std::vector<double>>> pixel_tables_;
    unordered_map<int, Dimensions> thin_, block_count_in_mcu_, current_mcu_;
    Dimensions image_size_, mcu_size_, mcu_count_;

public:
    DecodingInfo() {}

    std::tuple<size_t, size_t> GetMCUCount() const {
        return {mcu_count_.vert, mcu_count_.hor};
    }

    std::tuple<size_t, size_t> GetBlockCountInMCU(int comp_id) const {
        return {block_count_in_mcu_.at(comp_id).vert, block_count_in_mcu_.at(comp_id).hor};
    }

    std::tuple<std::shared_ptr<HTDecoder>, std::shared_ptr<HTDecoder>> GetHuffDecoders(int comp_id) const {
        auto iter_dc = dc_huff_decoders_.find(channel_info_.at(comp_id).huff_dc_id);
        if (iter_dc == dc_huff_decoders_.end()) {
            throw std::runtime_error("dc huff doesn't exist");
        }
        auto iter_ac= ac_huff_decoders_.find(channel_info_.at(comp_id).huff_ac_id);
        if (iter_ac == dc_huff_decoders_.end()) {
            throw std::runtime_error("ac huff doesn't exist");
        }
        return {iter_dc->second, iter_ac->second};
    }

    table GetQuantTable(int comp_id) {
        auto iter = quant_tables_.find(channel_info_[comp_id].quant_id);
        if (iter == quant_tables_.end()) {
            throw std::runtime_error("quant table doesn't exist");
        }
        return iter->second;
    }

    void AddMCU(const std::vector<std::vector<double>> &mcu, int id) {
        FillSubTable(pixel_tables_[id], mcu,
                8 * block_count_in_mcu_[id].hor * current_mcu_[id].hor,
                8 * block_count_in_mcu_[id].vert * current_mcu_[id].vert);
        if (current_mcu_[id].hor + 1 == mcu_count_.hor) {
            current_mcu_[id].hor = 0;
            ++current_mcu_[id].vert;
        } else {
            ++current_mcu_[id].hor;
        }
    }

    int64_t GetAndUpdateLastDC(int comp_id, int64_t dc_coef_) {
        if (last_dc_coef_.find(comp_id) == last_dc_coef_.end()) {
            last_dc_coef_[comp_id] = 0;
        }
        last_dc_coef_[comp_id] += dc_coef_;
        auto a = last_dc_coef_[comp_id];
        return last_dc_coef_[comp_id];
    }

    void SetQuantTable(int id, table quant_table) {
        auto iter = quant_tables_.find(id);
        if (iter != quant_tables_.end()) {
            throw std::runtime_error("quant table already exists");
        }
        quant_tables_[id] = std::move(quant_table);
    }

    void SetHuffTableAC(int id, std::shared_ptr<HTDecoder> huff) {
        auto iter = ac_huff_decoders_.find(id);
        if (iter != ac_huff_decoders_.end()) {
            throw std::runtime_error("ac huff table already exists");
        }
        ac_huff_decoders_[id] = huff;
    }

    void SetHuffTableDC(int id, std::shared_ptr<HTDecoder> huff) {
        auto iter = dc_huff_decoders_.find(id);
        if (iter != dc_huff_decoders_.end()) {
            throw std::runtime_error("dc huff table already exists");
        }
        dc_huff_decoders_[id] = huff;
    }

    bool SOF0Initialized() {
        return (!thin_.empty());
    }

    void SetDimentions(size_t hh, size_t ww) {
        if (!hh|| !ww) {
            throw std::runtime_error("dimensions can't be zero");
        }
        if (image_size_.vert || image_size_.hor) {
            throw std::runtime_error("dimensions were already set");
        }
        image_size_ = Dimensions(ww, hh);
    }

    void SetSOF0(int id, size_t hor, size_t vert, int q_id) {
        auto iter = thin_.find(id);
        if (iter != thin_.end() || channel_info_[id].quant_id != 0) {
            throw std::runtime_error("parameters for channel are already set");
        }
        thin_[id] = Dimensions(hor, vert);
        channel_info_[id].quant_id = q_id;
    }

    void SetSOS(int id, int dc_id, int ac_id) {
        channel_info_[id].huff_dc_id = dc_id;
        channel_info_[id].huff_ac_id = ac_id;
    }

    void CalculateAllInfoBeforeDecoding() {
        // если их задали то задали все thinning
        if (!image_size_.hor || !image_size_.vert) {
            throw std::runtime_error("dimensions were not set");
        }
        size_t max_hor = 0;
        size_t max_vert = 0;
        for (auto &[id, dim] : thin_) {
            max_hor = std::max(max_hor, dim.hor);
            max_vert = std::max(max_vert, dim.vert);
        }

        mcu_size_ = Dimensions(max_hor * 8,max_vert * 8);
        mcu_count_ = Dimensions(ceil(static_cast<double>(image_size_.hor) / mcu_size_.hor),
                                ceil(static_cast<double>(image_size_.vert) / mcu_size_.vert));

        for (int id = 1; id < 4; ++id) {
            auto iter = thin_.find(id);
            if (iter != thin_.end()) {
                block_count_in_mcu_[id] = Dimensions(mcu_size_.hor / (8 * iter->second.hor),
                                                     mcu_size_.vert / (8 * iter->second.vert));
                current_mcu_[id] = Dimensions();
                pixel_tables_[id] = std::vector(8 * block_count_in_mcu_[id].vert * mcu_count_.vert,
                                                std::vector<double > (8 * block_count_in_mcu_[id].hor * mcu_count_.hor, 0));
            } else {
                thin_[id] = Dimensions(1, 1);
                block_count_in_mcu_[id] = Dimensions();
                current_mcu_[id] = Dimensions();
                pixel_tables_[id] = std::vector(image_size_.vert,std::vector<double> (image_size_.hor, 0));
            }
        }
    }

    RGB CalculateRGB(size_t yy, size_t xx) {
        double y = pixel_tables_[1][floor(yy / thin_[1].vert)][ceil(xx / thin_[1].hor)] + 128;
        double cb = pixel_tables_[2][floor(yy / thin_[2].vert)][ceil(xx / thin_[2].hor)];
        double cr = pixel_tables_[3][floor(yy / thin_[3].vert)][ceil(xx / thin_[3].hor)];

        RGB res;
        res.r = std::min(std::max(static_cast<int>(floor(y + 1.402 * cr)), 0), 255);
        res.g = std::min(std::max(static_cast<int>(floor(y - 0.34414 * cb - 0.71414 * cr)), 0), 255);
        res.b = std::min(std::max(static_cast<int>(floor(y + 1.772 * cb)), 0), 255);

        return res;
    }
};
