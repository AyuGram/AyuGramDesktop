// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include <sstream>
#include "ayu_pipe_wrapper.h"
#include "ayu/libs/bit_converter.hpp"

template<class T>
void ayu_pipe_wrapper::send(T obj) {
    auto s = json(obj).dump();
    auto length = s.length();
    char lengthBuff[4];
    bit_converter::i32_to_bytes(length, false, lengthBuff);

    os.write(lengthBuff, 4);
    os.write(s.c_str(), length);
    os.flush();
}

json ayu_pipe_wrapper::receive() {
    char lengthBuff[4];
    is.read(lengthBuff, 4);

    auto length = bit_converter::bytes_to_i32(lengthBuff, false);

    auto sb = std::stringbuf();
    char buff[4096];

    while (length > 0) {
        auto readSize = std::min(length, (int) sizeof(buff));
        is.read(buff, readSize);
        sb.sputn(buff, readSize);
        length -= readSize;
    }

    auto p = json::parse(sb.str());
    return p;
}