add_shad_library(decoder
  # list your files here
  decoder.cpp
  SOLUTION_ONLY_SRCS huffman.cpp decoder.cpp)

add_catch(test_baseline test_baseline.cpp)
add_catch(test_progressive test_progressive.cpp)

target_link_libraries(test_baseline decoder ${FFTW_LIBRARIES} ${PNG_LIBRARY} ${JPEG_LIBRARIES})
target_link_libraries(test_progressive decoder ${FFTW_LIBRARIES} ${PNG_LIBRARY} ${JPEG_LIBRARIES})
