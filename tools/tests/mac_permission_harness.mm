#import <Foundation/Foundation.h>

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

template <typename Signature>
using Fn = std::function<Signature>;

enum class PermissionType { Microphone, Camera };
enum class PermissionStatus { Granted, Denied };

namespace {

const auto AVMediaTypeAudio = @"audio";
const auto AVMediaTypeVideo = @"video";
auto ApiAvailable = true;
auto NativeRequests = 0;
NSString *RequestedMedia = nil;
void (^PendingCompletion)(BOOL) = nil;
auto MainQueue = std::vector<Fn<void()>>();

} // namespace

@interface AVCaptureDevice : NSObject
+ (void)requestAccessForMediaType:(NSString *)media
	completionHandler:(void (^)(BOOL))completion;
@end

@implementation AVCaptureDevice

+ (BOOL)respondsToSelector:(SEL)selector {
	return ApiAvailable;
}

+ (void)requestAccessForMediaType:(NSString *)media
	completionHandler:(void (^)(BOOL))completion {
	++NativeRequests;
	RequestedMedia = media;
	PendingCompletion = [completion copy];
}

@end

namespace crl {

void on_main(Fn<void()> callback) {
	MainQueue.push_back(std::move(callback));
}

} // namespace crl

/* REQUEST_PERMISSION */

namespace {

void DrainMainQueue() {
	const auto callbacks = std::exchange(MainQueue, {});
	for (const auto &callback : callbacks) {
		callback();
	}
}

int RunScenario(PermissionType type, bool granted, bool available) {
	ApiAvailable = available;
	auto results = std::vector<PermissionStatus>();
	auto failures = 0;
	const auto check = [&](bool success, const char *message) {
		if (!success) {
			std::cerr << message << '\n';
			++failures;
		}
	};
	RequestPermission(type, [&](PermissionStatus status) {
		results.push_back(status);
	});
	if (!available) {
		check(NativeRequests == 0, "Unavailable API must not be called");
		check(results == std::vector{PermissionStatus::Granted},
			"Legacy fallback must deliver exactly one Granted result");
		check(MainQueue.empty(), "Legacy fallback must not enqueue work");
		return failures;
	}
	check(NativeRequests == 1, "Permission request must reach the native API once");
	check([RequestedMedia isEqualToString:(type == PermissionType::Microphone
		? AVMediaTypeAudio : AVMediaTypeVideo)], "Wrong native media type");
	check(results.empty(), "Callback ran before the user answered permission");
	check(MainQueue.empty(), "Main callback queued before the user answered");
	if (!PendingCompletion) {
		check(false, "Native completion handler was not registered");
		return failures;
	}
	PendingCompletion(granted);
	PendingCompletion = nil;
	check(results.empty(), "Callback ran outside the main dispatch queue");
	check(MainQueue.size() == 1, "Native answer must enqueue one main callback");
	DrainMainQueue();
	check(results == std::vector{granted
		? PermissionStatus::Granted : PermissionStatus::Denied},
		"Callback must deliver exactly one result matching the native answer");
	DrainMainQueue();
	check(results.size() == 1, "Permission continuation ran more than once");
	return failures;
}

} // namespace

int main(int argc, char *argv[]) {
	@autoreleasepool {
		if (argc != 4) {
			return 2;
		}
		const auto media = std::string(argv[1]);
		const auto type = media == "microphone"
			? PermissionType::Microphone : PermissionType::Camera;
		return RunScenario(type, std::string(argv[2]) == "grant",
			std::string(argv[3]) == "available") ? 1 : 0;
	}
}
