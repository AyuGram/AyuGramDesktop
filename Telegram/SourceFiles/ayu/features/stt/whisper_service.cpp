// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/whisper_service.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/stt/ggml_backend_detector.h"
#include "ayu/features/stt/ggml_backend_manager.h"
#include "base/debug_log.h"
#include "core/application.h"
#include "crl/crl_on_main.h"

#include "whisper.h"
#include "ggml-backend.h"
#include "gsl/util"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace Ayu::STT {

namespace {

constexpr int kTargetSampleRate = 16000;
constexpr crl::time kCtxIdleTimeoutMs = 3 * 60000;

} // namespace

WhisperService &WhisperService::instance() {
	static WhisperService self;
	return self;
}

WhisperService::WhisperService() {
	_idleTimer.setCallback([this] { freeContext(); });
}

void WhisperService::scheduleFreeContext() {
	// Timer lives on the main thread. restart it from there.
	crl::on_main([this] { _idleTimer.callOnce(kCtxIdleTimeoutMs); });
}

void WhisperService::freeContext(bool unloadBackend) {
	if (!_ctxMutex.tryLock()) {
		scheduleFreeContext();
		return;
	}

	const auto ctx = _cachedCtx;
	_cachedCtx = nullptr;
	_cachedModelPath.clear();
	void *backendReg = nullptr;
	if (unloadBackend) {
		backendReg = _loadedGgmlBackendReg;
		_loadedGgmlBackendReg = nullptr;
		_loadedGgmlBackendPath.clear();
	}
	_ctxMutex.unlock();
	if (ctx || backendReg) {
		const auto thread = QThread::create([ctx, backendReg] {
			if (ctx) {
				whisper_free(ctx);
			}
			if (backendReg) {
				LOG(("WhisperService: unloading GPU backend"));
				ggml_backend_unload(static_cast<ggml_backend_reg_t>(backendReg));
			}
		});
		QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);
		thread->start();
	}
}

bool WhisperService::isQuitPrevent() {
	if (_freeInFlight) {
		return true;
	}

	QMutexLocker lock(&_ctxMutex);
	const auto ctx = _cachedCtx;
	_cachedCtx = nullptr;
	_cachedModelPath.clear();
	lock.unlock();

	if (!ctx) {
		return false;
	}

	_idleTimer.cancel();
	_freeInFlight = true;

	// whisper_free releases Metal buffers off the main thread; block quit
	// until it's done, otherwise ggml's static residency-set destructor
	// aborts on a still-alive context.
	const auto thread = QThread::create([ctx] { whisper_free(ctx); });
	QObject::connect(thread, &QThread::finished, thread, [=] {
		thread->deleteLater();
		_freeInFlight = false;
		if (Core::Quitting()) {
			LOG(("WhisperService doesn't prevent quit any more."));
			Core::App().quitPreventFinished();
		}
	});
	thread->start();
	return true;
}

std::vector<float> WhisperService::decodeAudioToPcm(const QString &filePath) {
	const auto pathUtf8 = filePath.toUtf8();
	const char *path = pathUtf8.constData();

	AVFormatContext *formatCtx = nullptr;
	if (avformat_open_input(&formatCtx, path, nullptr, nullptr) < 0) {
		LOG(("WhisperService: failed to open file: %1").arg(filePath));
		return {};
	}
	const auto closeFormat = gsl::finally([&] { avformat_close_input(&formatCtx); });

	if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
		return {};
	}

	int audioStreamIndex = -1;
	for (unsigned i = 0; i < formatCtx->nb_streams; ++i) {
		if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStreamIndex = static_cast<int>(i);
			break;
		}
	}
	if (audioStreamIndex < 0) {
		LOG(("WhisperService: no audio stream in: %1").arg(filePath));
		return {};
	}

	const AVCodecParameters *codecParams = formatCtx->streams[audioStreamIndex]->codecpar;
	const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
	if (!codec) {
		return {};
	}

	AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
	const auto closeCodec = gsl::finally([&] { avcodec_free_context(&codecCtx); });

	avcodec_parameters_to_context(codecCtx, codecParams);
	if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
		return {};
	}

	constexpr AVChannelLayout targetLayout = AV_CHANNEL_LAYOUT_MONO;
	SwrContext *swrCtx = nullptr;
	swr_alloc_set_opts2(
		&swrCtx,
		&targetLayout,
		AV_SAMPLE_FMT_FLT,
		kTargetSampleRate,
		&codecCtx->ch_layout,
		codecCtx->sample_fmt,
		codecCtx->sample_rate,
		0,
		nullptr);
	swr_init(swrCtx);
	const auto closeSwr = gsl::finally([&] { swr_free(&swrCtx); });

	std::vector<float> pcmData;
	AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	const auto freePacket = gsl::finally([&] { av_packet_free(&packet); });
	const auto freeFrame = gsl::finally([&] { av_frame_free(&frame); });

	while (av_read_frame(formatCtx, packet) >= 0) {
		if (packet->stream_index != audioStreamIndex) {
			av_packet_unref(packet);
			continue;
		}
		if (avcodec_send_packet(codecCtx, packet) < 0) {
			av_packet_unref(packet);
			continue;
		}
		av_packet_unref(packet);

		while (avcodec_receive_frame(codecCtx, frame) == 0) {
			const int outputSamples = av_rescale_rnd(
				swr_get_delay(swrCtx, codecCtx->sample_rate) + frame->nb_samples,
				kTargetSampleRate,
				codecCtx->sample_rate,
				AV_ROUND_UP);

			const auto prevSize = pcmData.size();
			pcmData.resize(prevSize + outputSamples);
			uint8_t *outPtr = reinterpret_cast<uint8_t*>(pcmData.data() + prevSize);
			const int converted = swr_convert(
				swrCtx,
				&outPtr,
				outputSamples,
				const_cast<const uint8_t**>(frame->data),
				frame->nb_samples);
			pcmData.resize(prevSize + std::max(0, converted));
			av_frame_unref(frame);
		}
	}

	(void)swr_convert(swrCtx, nullptr, 0, nullptr, 0);

	return pcmData;
}

void WhisperService::transcribeOnDemand(
	const QString &filePath,
	const QString &modelPath,
	const QString &language,
	std::function<void(QString)> callback) {

	auto sharedCallback = std::make_shared<std::function<void(QString)>>(std::move(callback));

	const auto thread = QThread::create([=] {
		const auto langUtf8 = language.toUtf8();

		// Hold the mutex for the whole inference, whisper_context is not
		// thread-safe, and this also guards against freeContext() racing in.
		QMutexLocker lock(&_ctxMutex);

		if (_cachedModelPath != modelPath || !_cachedCtx) {
			if (_cachedCtx) {
				whisper_free(_cachedCtx);
				_cachedCtx = nullptr;
				_cachedModelPath.clear();
			}
			const auto modelUtf8 = modelPath.toUtf8();
			whisper_context_params ctxParams = whisper_context_default_params();
			const auto accelerationEnabled = AyuSettings::getInstance().sttHardwareAcceleration();
			const auto kind = accelerationEnabled
				? BestAvailableGgmlBackendKind()
				: std::nullopt;
			ctxParams.use_gpu = kind.has_value();
			LOG(("WhisperService: hardware acceleration=%1, kind=%2"
				).arg(accelerationEnabled ? 1 : 0
				).arg(kind ? static_cast<int>(*kind) : -1));
			if (kind) {
				const auto modulePath = GgmlBackendManager::loadedModulePath(*kind);
				if (modulePath.isEmpty()) {
					LOG(("WhisperService: GPU backend not downloaded yet, falling back to CPU"));
					ctxParams.use_gpu = false; // not downloaded yet
				} else if (_loadedGgmlBackendPath == modulePath) {
					LOG(("WhisperService: GPU backend already loaded, skipping reload: %1"
						).arg(modulePath));
				} else {
					LOG(("WhisperService: loading GPU backend module: %1").arg(modulePath));
#if defined(Q_OS_WIN)
					// dl_load_library's LoadLibraryW (ggml-backend-dl.cpp) doesn't
					// search the loaded DLL's own directory for transitive deps
					// (ggml-base.dll, cublas64_*.dll) by default - widen the
					// search path first.
					const auto moduleDir = QFileInfo(modulePath).absolutePath();
					SetDllDirectoryW(reinterpret_cast<LPCWSTR>(moduleDir.utf16()));
#endif
					const auto reg = ggml_backend_load(modulePath.toUtf8().constData());
#if defined(Q_OS_WIN)
					SetDllDirectoryW(nullptr);
#endif
					if (!reg) {
						LOG(("WhisperService: failed to load GPU backend module: %1, falling back to CPU"
							).arg(modulePath));
						ctxParams.use_gpu = false;
					} else {
						LOG(("WhisperService: GPU backend loaded ok: %1").arg(modulePath));
						_loadedGgmlBackendPath = modulePath;
						_loadedGgmlBackendReg = reg;
					}
				}
			}
			_cachedCtx = whisper_init_from_file_with_params(
				modelUtf8.constData(),
				ctxParams);
			if (!_cachedCtx) {
				LOG(("WhisperService: failed to load model: %1").arg(modelPath));
				crl::on_main([sharedCallback] {
					(*sharedCallback)(QString());
				});
				return;
			}
			LOG(("WhisperService: model loaded, use_gpu=%1").arg(ctxParams.use_gpu ? 1 : 0));
			_cachedModelPath = modelPath;
		}
		whisper_context *ctx = _cachedCtx;

		// Reset the idle timer on every exit path so the cooldown
		// restarts after each transcription, not after the first one.
		const auto freeOnIdle = gsl::finally([this] { scheduleFreeContext(); });

		auto pcm = decodeAudioToPcm(filePath);
		if (pcm.empty()) {
			LOG(("WhisperService: failed to decode audio: %1").arg(filePath));
			crl::on_main([sharedCallback] {
				(*sharedCallback)(QString());
			});
			return;
		}

		whisper_full_params wFullParams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
		// detect_language=true means "detect only, skip transcription", never set it.
		// Pass language=nullptr to trigger auto-detect + transcribe in one pass.
		wFullParams.detect_language = false;
		if (language.isEmpty() || language == u"auto"_q) {
			wFullParams.language = nullptr;
		} else {
			wFullParams.language = langUtf8.constData();
		}
		wFullParams.translate = false;
		wFullParams.print_progress = false;
		wFullParams.print_realtime = false;
		wFullParams.print_timestamps = false;
		wFullParams.single_segment = true;
		wFullParams.temperature_inc = 0.2f;
		wFullParams.no_speech_thold = 1.0f;
		wFullParams.n_threads = std::max(1, static_cast<int>(QThread::idealThreadCount()) - 1);

		if (whisper_full(ctx, wFullParams, pcm.data(), static_cast<int>(pcm.size())) != 0) {
			LOG(("WhisperService: whisper_full failed"));
			crl::on_main([sharedCallback] {
				(*sharedCallback)(QString());
			});
			return;
		}

		const int segmentCount = whisper_full_n_segments(ctx);
		QString result;
		for (int i = 0; i < segmentCount; ++i) {
			if (const char *text = whisper_full_get_segment_text(ctx, i)) {
				if (!result.isEmpty()) result += u" "_q;
				result += QString::fromUtf8(text).trimmed();
			}
		}

		crl::on_main([sharedCallback, result] {
			(*sharedCallback)(result);
		});
	});
	QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
	thread->start();
}

} // namespace Ayu::STT
