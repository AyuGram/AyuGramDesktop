// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "ayu/libs/json.hpp"
#include "ayu/libs/pipe.hpp"
#include "ayu/sync/models.h"

using json = nlohmann::json;
using pipein = nes::basic_pipe_istream<unsigned char>;
using pipeout = nes::basic_pipe_ostream<unsigned char>;

class ayu_pipe_wrapper
{
public:
	void connect();

	void send(json p);
	std::optional<json> receive();

private:
	std::unique_ptr<pipein> is;
	std::unique_ptr<pipeout> os;
};
