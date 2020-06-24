#include <catch.hpp>
#include "test_commons.h"
#include <iostream>
#include "/usr/local/include/fftw3.h"
#include <istream>
#include <fstream>


TEST_CASE("small jfif (4:2:0)", "[jpg]") {
    CheckImage("small.jpg", ":)");
}


TEST_CASE("jfif (4:4:4)", "[jpg]") {
    CheckImage("lenna.jpg");
}

TEST_CASE("jfif (4:2:2)", "[jpg]") {
    CheckImage("bad_quality.jpg", "so quality");
}


TEST_CASE("tiny jfif (4:4:4)", "[jpg]") {
    CheckImage("tiny.jpg");
}

TEST_CASE("exif (4:2:2)", "[jpg]") {
    CheckImage("chroma_halfed.jpg");
}

TEST_CASE("exif (grayscale)", "[jpg]") {
    CheckImage("grayscale.jpg");
}

TEST_CASE("jfif/exif (4:2:0)", "[jpg]") {
    CheckImage("test.jpg");
}

TEST_CASE("exif (4:4:4)", "[jpg]") {
    CheckImage("colors.jpg");
}

TEST_CASE("photoshop (4:4:4)", "[jpg]") {
    CheckImage("save_for_web.jpg");
}

TEST_CASE("Error handling22", "[jpg]") {
    ExpectFail("bad" + std::to_string(2) + ".jpg");
}

TEST_CASE("Error handling10", "[jpg]") {
    ExpectFail("bad" + std::to_string(10) + ".jpg");
}

TEST_CASE("Error handling2", "[jpg]") {
    for (int i = 1; i <= 24; ++i) {
        ExpectFail("bad" + std::to_string(i) + ".jpg");
    }
    //ExpectFail("bad" + std::to_string(i) + ".jpg");
}
