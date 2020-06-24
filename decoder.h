#pragma once

#include "image.h"
#include "bit-byte-reader.h"
#include "parser.h"
#include <string>
#include "/usr/local/include/fftw3.h"
#include "decoding-info.h"
#include <istream>
#include <fstream>
#include <iostream>

class Decoder {
private:
    Image image_;
    DecodingInfo info_;
    std::shared_ptr<ByteBitReader> reader_;
    Parser parser_;

    void ConstructImage() {
        for (size_t yy = 0; yy < image_.Height(); ++yy) {
            for (size_t xx = 0; xx < image_.Width(); ++xx) {
                image_.SetPixel(yy, xx, info_.CalculateRGB(yy, xx));
            }
        }
    }

    bool ProcessMarker(uint8_t marker) {
        if (marker == 216) {
            // soi
            return true;
        }
        if (marker == 254) {
            if (!image_.GetComment().empty()) {
                throw std::runtime_error("repeating COM section");
            }
            image_.SetComment(parser_.ParseCOM());
            return true;
        }
        if (marker == 219) {
            // dqt
            int len = reader_->GetTwoBytes() - 2;
            while (len > 0) {
                auto byte = reader_->GetByte();
                bool double_length = Left(byte);
                if (double_length) {
                    len -= 129;
                } else {
                    len -= 65;
                }
                int id = Right(byte);
                auto quant_table = parser_.ParseDQT(double_length);
                info_.SetQuantTable(id, std::move(quant_table));
            }
            return true;
        }
        if (marker == 192) {
            // sof0
            if (info_.SOF0Initialized()) {
                throw std::runtime_error("repeating SOF0 marker");
            }
            auto [hight, width, vect] = parser_.ParseSOF0();
            info_.SetDimentions(hight, width); //here
            image_.SetSize(width, hight);

            for (size_t iter = 0; iter < vect.size(); ++iter) {
                info_.SetSOF0(vect[iter].channel_id, vect[iter].horizontal, vect[iter].vertical,
                              vect[iter].quant_id);
            }
            return true;
        }
        if (marker == 196) {
            // dht
            int len = reader_->GetTwoBytes() - 2;
            while (len > 0) {
                int byte = reader_->GetByte();
                auto htable = parser_.ParseDHT();
                if (Left(byte)) {
                    info_.SetHuffTableAC(Right(byte), std::make_shared<HTDecoder>(htable));
                } else {
                    info_.SetHuffTableDC(Right(byte), std::make_shared<HTDecoder>(htable));
                }
                len -= htable.size() + 1;
            }
            return true;
        }
        if (marker <= 239 && marker >= 224) {
            // app_n
            parser_.ParseAPPn();
            return true;
        }
        if (marker == 218) {
            // sos
            auto vect = parser_.ParseSOS();

            // есть инфа по каждому каналу
            if (vect.empty()) {
                throw std::runtime_error("not enough info about channels");
            }

            for (size_t iter = 0; iter < vect.size(); ++iter) {
                info_.SetSOS(vect[iter].channel_id, vect[iter].huff_dc_id, vect[iter].huff_ac_id);
            }
            info_.CalculateAllInfoBeforeDecoding();
            ReadAllBlocks();
            reader_->SetEndOfBitReading();
            marker = parser_.ParseMarker();
            if (marker != 217) {
                throw std::runtime_error("expected end marker");
            }
            return false;
        }
        throw std::runtime_error("wrong marker");
    }

    std::vector<std::vector<double>> DecodeBlock(const std::vector<std::vector<int64_t>> &block,
                                              std::vector<std::vector<int64_t>> quant,
                                              int64_t last_dc) {
        std::vector<std::vector<double>> result(8, std::vector<double>(8));
        //std::cout << "DC : " << last_dc << '\n';

        double * in = (double*)fftw_malloc(sizeof(double) * 8 * 8);
        double * out = (double*)fftw_malloc(sizeof(double) * 8 * 8);

        fftw_plan p = fftw_plan_r2r_2d(8, 8, in, out, FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE);

        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                in[i * 8 + j] = static_cast<double>(block[i][j] * quant[i][j]);
                if (i + j == 0) {
                    in[i * 8 + j] *= 2;
                } else if (i * j == 0) {
                    in[i * 8 + j] *= sqrt(2);
                } else {
                    in[i * 8 + j] *= 1;
                }
            }
        }
        in[0] = static_cast<double>(2 * last_dc * quant[0][0]);
        /*
        std::cout << '\n';
        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                std::cout << block[i][j] << ' ';
            }
            std::cout << '\n';
        }*/

        fftw_execute(p);

        for (size_t i = 0; i < 8; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                result[i][j] = out[i * 8 + j] * 0.0625;
            }
        }

        fftw_destroy_plan(p);
        fftw_free(in);
        fftw_free(out);

        return result;
    }

    void ReadAllBlocks() {
        auto [vert_mcu_count, hor_mcu_count] = info_.GetMCUCount();
        for (size_t vert = 0; vert < vert_mcu_count; ++vert) {
            for (size_t hor = 0; hor < hor_mcu_count; ++hor) {
                for (uint8_t comp = 1; comp < 4; ++ comp) {
                    info_.AddMCU(ReadComponentMCU(comp), comp);
                }
            }
        }
    }

    std::vector<std::vector<double>> ReadComponentMCU(uint8_t component_id) {
        auto [vert_block_count, hor_block_count] = info_.GetBlockCountInMCU(component_id);

        if (!vert_block_count || !hor_block_count) {
            return std::vector<std::vector<double>>(8, std::vector<double>(8));
        }

        auto [dc_huff, ac_huff] = info_.GetHuffDecoders(component_id);

        std::vector<std::vector<double>> result(8 * vert_block_count, std::vector<double >(8 * hor_block_count));
        for (size_t vert = 0; vert < vert_block_count; ++vert) {
            for (size_t hor = 0; hor < hor_block_count; ++hor) {
                auto block = parser_.ParseBlock(dc_huff, ac_huff);
                auto decoded_block = DecodeBlock(block, info_.GetQuantTable(component_id),
                                    info_.GetAndUpdateLastDC(component_id, block[0][0]));
                FillSubTable(result, decoded_block, 8 * hor, 8 * vert);
            }
        }

        return result;
    }

public:
    Decoder(std::istream &in) : reader_(std::make_shared<ByteBitReader>(in)), parser_(reader_) {
        while (ProcessMarker(parser_.ParseMarker())) {}
        ConstructImage();
    }

    Image GetImage() {
        return image_;
    }

    ~Decoder() {}
};

Image Decode(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    Decoder decoder(file);
    return decoder.GetImage();
}
