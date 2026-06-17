// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/platform/stt_mac.h"
#include "ayu/features/stt/whisper_service.h"

#include "base/debug_log.h"
#include "base/invoke_queued.h"
#include "crl/crl_on_main.h"

#import <Speech/Speech.h>
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

namespace {

constexpr int kTargetSampleRate = 16000;

void RunRecognitionPass(
		SFSpeechRecognizer *recognizer,
		std::shared_ptr<std::vector<float>> pcm,
		bool onDevice,
		std::shared_ptr<std::function<void(QString)>> callback) {
	AVAudioFormat *format = [[AVAudioFormat alloc]
		initWithCommonFormat:AVAudioPCMFormatFloat32
		sampleRate:kTargetSampleRate
		channels:1
		interleaved:NO];

	const AVAudioFrameCount frameCount = (AVAudioFrameCount)pcm->size();
	AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc]
		initWithPCMFormat:format
		frameCapacity:frameCount];
	buffer.frameLength = frameCount;
	memcpy(buffer.floatChannelData[0],
		pcm->data(),
		frameCount * sizeof(float));

	SFSpeechAudioBufferRecognitionRequest *request =
		[[SFSpeechAudioBufferRecognitionRequest alloc] init];
	request.shouldReportPartialResults = NO;
	if (@available(macOS 10.15, *)) {
		request.requiresOnDeviceRecognition = onDevice ? YES : NO;
	}

	// __block keeps task alive past endAudio, otherwise the XPC channel drops early.
	__block SFSpeechRecognitionTask *task = nil;
	task = [recognizer recognitionTaskWithRequest:request
		resultHandler:^(SFSpeechRecognitionResult *result, NSError *error) {
			(void)request;
			(void)task;

			if (error) {
				if (onDevice) {
					LOG(("SFSpeech on-device failed (%1), retrying via server").arg(
						QString::fromNSString(error.localizedDescription)));
					crl::on_main([=] {
						RunRecognitionPass(recognizer, pcm, false, callback);
					});
					return;
				}
				LOG(("SFSpeech error: %1").arg(
					QString::fromNSString(error.localizedDescription)));
				crl::on_main([callback] { (*callback)(QString()); });
				return;
			}

			if (!result || !result.isFinal) {
				return;
			}

			QString qText = QString::fromNSString(
				result.bestTranscription.formattedString);
			crl::on_main([callback, qText] { (*callback)(qText); });
		}];

	[request appendAudioPCMBuffer:buffer];
	[request endAudio];
}

} // namespace

namespace Ayu::STT::Mac {

void transcribeFile(const QString &filePath, const QString &language, std::function<void(QString)> callback) {
	auto sharedCallback = std::make_shared<std::function<void(QString)>>(std::move(callback));
	const auto sharedPath = std::make_shared<QString>(filePath);

	const auto doRecognize = [=](std::vector<float> pcm) {
		if (pcm.empty()) {
			crl::on_main([sharedCallback] { (*sharedCallback)(QString()); });
			return;
		}
		auto sharedPcm = std::make_shared<std::vector<float>>(std::move(pcm));

		NSString *localeId;
		if (language.isEmpty() || language == u"auto"_q) {
			localeId = [[NSLocale preferredLanguages] firstObject] ?: @"en-US";
		} else {
			localeId = language.toNSString();
		}
		NSLocale *locale = [NSLocale localeWithLocaleIdentifier:localeId];

		SFSpeechRecognizer *recognizer = [[SFSpeechRecognizer alloc]
			initWithLocale:locale];
		if (!recognizer || !recognizer.available) {
			crl::on_main([sharedCallback] { (*sharedCallback)(QString()); });
			return;
		}

		auto startOnDevice = false;
		if (@available(macOS 10.15, *)) {
			startOnDevice = recognizer.supportsOnDeviceRecognition;
		}
		RunRecognitionPass(recognizer, sharedPcm, startOnDevice, sharedCallback);
	};

	[SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus status) {
		if (status != SFSpeechRecognizerAuthorizationStatusAuthorized) {
			crl::on_main([sharedCallback] { (*sharedCallback)(QString()); });
			return;
		}
		dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
			auto pcm = Ayu::STT::WhisperService::decodeAudioToPcm(*sharedPath);
			doRecognize(std::move(pcm));
		});
	}];
}

void requestSpeechPermission() {
	[SFSpeechRecognizer requestAuthorization:^(SFSpeechRecognizerAuthorizationStatus) {}];
}

} // namespace Ayu::STT::Mac
