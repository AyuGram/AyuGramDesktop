// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>

namespace Ayu {

class GhostModePeerExceptions final {
public:
	using SerializedPeerId = std::uint64_t;
	using Values = std::unordered_set<SerializedPeerId>;

	[[nodiscard]] bool shouldSend(
			bool globallyEnabled,
			std::optional<SerializedPeerId> peerId) const {
		return globallyEnabled || (peerId && contains(*peerId));
	}

	[[nodiscard]] bool contains(SerializedPeerId peerId) const {
		return _values.contains(peerId);
	}

	bool set(SerializedPeerId peerId, bool enabled) {
		return enabled
			? _values.emplace(peerId).second
			: (_values.erase(peerId) != 0);
	}

	[[nodiscard]] const Values &values() const {
		return _values;
	}

	void replace(Values values) {
		_values = std::move(values);
	}

private:
	Values _values;

};

} // namespace Ayu
