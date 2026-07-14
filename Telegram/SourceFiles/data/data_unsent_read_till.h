/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Data {

template <typename Id>
class UnsentReadTill final {
public:
	[[nodiscard]] explicit operator bool() const {
		return (_till != Id());
	}

	[[nodiscard]] Id till() const {
		return _till;
	}

	void add(Id till) {
		if (_till < till) {
			_till = till;
		}
	}

	[[nodiscard]] Id with(Id till) const {
		return (_till < till) ? till : _till;
	}

	void sent(Id till) {
		if (_till <= till) {
			_till = Id();
		}
	}

private:
	Id _till = Id();

};

} // namespace Data
