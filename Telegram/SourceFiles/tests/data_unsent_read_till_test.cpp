// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "data/data_unsent_read_till.h"

#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool condition) {
	if (!condition) {
		std::abort();
	}
}

} // namespace

int main() {
	using State = Data::UnsentReadTill<std::uint64_t>;

	auto state = State();
	Require(!state);
	Require(state.with(10) == 10);

	state.add(10);
	state.add(5);
	Require(static_cast<bool>(state));
	Require(state.till() == 10);
	Require(state.with(7) == 10);
	Require(state.with(12) == 12);

	state.sent(9);
	Require(state.till() == 10);
	state.sent(10);
	Require(!state);

	state.add(15);
	state.sent(20);
	Require(!state);
}
