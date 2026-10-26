// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace AyuMcp {

struct RequestHeaders {
	QByteArray protocolVersion;
	QByteArray method;
	QByteArray name;
};

struct Response {
	int status = 200;
	QByteArray body;
};

void handleMessage(
	const QByteArray &body,
	const RequestHeaders &headers,
	Fn<bool()> alive,
	Fn<void(Response)> done);

} // namespace AyuMcp
