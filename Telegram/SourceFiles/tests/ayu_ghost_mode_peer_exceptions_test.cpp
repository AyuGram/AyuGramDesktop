// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ghost_mode_peer_exceptions.h"

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
	using Exceptions = Ayu::GhostModePeerExceptions;

	constexpr auto alice = std::uint64_t(101);
	constexpr auto bob = std::uint64_t(202);
	constexpr auto carol = std::uint64_t(303);

	auto exceptions = Exceptions();
	Require(exceptions.shouldSend(true, std::nullopt));
	Require(exceptions.shouldSend(true, alice));
	Require(!exceptions.shouldSend(false, std::nullopt));
	Require(!exceptions.shouldSend(false, alice));

	Require(exceptions.set(alice, true));
	Require(!exceptions.set(alice, true));
	Require(exceptions.contains(alice));
	Require(exceptions.shouldSend(false, alice));
	Require(!exceptions.shouldSend(false, bob));

	Require(exceptions.set(alice, false));
	Require(!exceptions.set(alice, false));
	Require(!exceptions.contains(alice));
	Require(!exceptions.shouldSend(false, alice));

	Require(exceptions.set(carol, true));
	exceptions.replace(Exceptions::Values{ alice, bob });
	Require(exceptions.values().size() == 2);
	Require(exceptions.shouldSend(false, alice));
	Require(exceptions.shouldSend(false, bob));
	Require(!exceptions.contains(carol));
}
