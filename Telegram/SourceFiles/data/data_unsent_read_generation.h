/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <cstdint>

namespace Data {

class UnsentReadGeneration final {
public:
	using Generation = std::uint32_t;

	[[nodiscard]] explicit operator bool() const {
		return _pending;
	}

	[[nodiscard]] Generation generation() const {
		return _pending ? _generation : 0;
	}

	void add() {
		if (++_generation == 0) {
			++_generation;
		}
		_pending = true;
	}

	void sent(Generation generation) {
		if (_pending && _generation == generation) {
			_pending = false;
		}
	}

private:
	Generation _generation = 0;
	bool _pending = false;

};

} // namespace Data
