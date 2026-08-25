// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/ggml_backend_detector.h"

#include <QtCore/QLibrary>

namespace Ayu::STT {

namespace {

#if defined(Q_OS_WIN)
constexpr auto kCudaDriverLib = "nvcuda";
constexpr auto kVulkanLoaderLib = "vulkan-1";
constexpr auto kLibraryVersion = -1;
#else
constexpr auto kCudaDriverLib = "cuda";
constexpr auto kVulkanLoaderLib = "vulkan";
constexpr auto kLibraryVersion = 1;
#endif

bool ProbeLibraryPresent(const char *name, const int version) {
	QLibrary lib;

	if (version == -1) {
		lib.setFileName(QString::fromUtf8(name));
	} else {
		lib.setFileNameAndVersion(QString::fromUtf8(name), version);
	}
	const auto loaded = lib.load();
	if (loaded) {
		lib.unload();
	}
	return loaded;
}

} // namespace

bool IsGpuDriverPresent(const GgmlBackendKind kind) {
	switch (kind) {
	case GgmlBackendKind::CUDA:
		return ProbeLibraryPresent(kCudaDriverLib, kLibraryVersion);
	case GgmlBackendKind::Vulkan:
		return ProbeLibraryPresent(kVulkanLoaderLib, kLibraryVersion);
	case GgmlBackendKind::Metal:
#if defined(Q_OS_MAC)
		return true;
#else
		return false;
#endif
	}
	return false;
}

std::optional<GgmlBackendKind> BestAvailableGgmlBackendKind() {
#if defined(Q_OS_MAC)
	if (IsGpuDriverPresent(GgmlBackendKind::Metal)) {
		return GgmlBackendKind::Metal;
	}
#else
	if (IsGpuDriverPresent(GgmlBackendKind::CUDA)) {
		return GgmlBackendKind::CUDA;
	}
	if (IsGpuDriverPresent(GgmlBackendKind::Vulkan)) {
		return GgmlBackendKind::Vulkan;
	}
#endif
	return std::nullopt;
}

} // namespace Ayu::STT
