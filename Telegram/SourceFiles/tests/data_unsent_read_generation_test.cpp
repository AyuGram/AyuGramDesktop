/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_unsent_read_generation.h"

#include <cstdlib>

namespace {

void Require(bool condition) {
	if (!condition) {
		std::abort();
	}
}

} // namespace

int main() {
	auto debt = Data::UnsentReadGeneration();
	Require(!debt);
	Require(debt.generation() == 0);

	debt.add();
	const auto first = debt.generation();
	Require(static_cast<bool>(debt));
	Require(first != 0);

	debt.add();
	const auto second = debt.generation();
	Require(second != first);

	debt.sent(first);
	Require(static_cast<bool>(debt));
	Require(debt.generation() == second);

	debt.sent(second);
	Require(!debt);
	Require(debt.generation() == 0);

	debt.add();
	const auto third = debt.generation();
	Require(third != second);
	debt.sent(second);
	Require(static_cast<bool>(debt));
	Require(debt.generation() == third);
	debt.sent(third);
	Require(!debt);
}
