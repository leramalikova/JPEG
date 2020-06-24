#pragma once

#include <image.h>
#include <catch.hpp>
#include <png_encoder.h>
#include <libjpg_reader.h>

#include <cmath>
#include <string>

#include <decoder.h>
#include <iostream>

int sqr(int x) {
    return x * x;
}

double Distance(const RGB& lhs, const RGB& rhs) {
    return sqrt(sqr(lhs.r - rhs.r) + sqr(lhs.g - rhs.g) + sqr(lhs.b - rhs.b));
}

void Compare(const Image& actual, const Image& expected) {
    double max = 0;
    double mean = 0;
    REQUIRE(actual.Width() == expected.Width());
    REQUIRE(actual.Height() == expected.Height());
    for (size_t y = 0; y < actual.Height(); ++y) {
        for (size_t x = 0; x < actual.Width(); ++x) {
            auto actual_data = actual.GetPixel(y, x);
            auto expected_data = expected.GetPixel(y, x);
            auto diff = Distance(actual_data, expected_data);
            max = std::max(max, diff);
            mean += diff;
        }
    }

    mean /= actual.Width() * actual.Height();
    REQUIRE(mean <= 5);
}

void CheckImage(
    const std::string& filename,
    const std::string& expected_comment = "",
    bool write_png = true)
{
    auto image = Decode("/Users/valeriamalikova/shad-cpp/jpeg-decoder/tests/" + filename);
    REQUIRE(image.GetComment() == expected_comment);
    auto ok_image = ReadJpg("/Users/valeriamalikova/shad-cpp/jpeg-decoder/tests/" + filename);
    Compare(image, ok_image);
    if (write_png) {
        auto dot_pos = filename.find(".");
        std::string png_filename(filename.substr(0, dot_pos) + ".png");
        WritePng(png_filename, image);
    }
}

void ExpectFail(const std::string& filename) {
    CHECK_THROWS(Decode("/Users/valeriamalikova/shad-cpp/jpeg-decoder/tests/bad/" + filename));
}
