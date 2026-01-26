project(stb)

add_library(stb STATIC stb_image.h stb_image.cpp)

target_include_directories(stb PUBLIC include)