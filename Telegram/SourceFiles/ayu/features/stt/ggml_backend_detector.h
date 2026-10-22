// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include <optional>

namespace Ayu::STT {

enum class GgmlBackendKind {
	CUDA,
	Vulkan,
	Metal,
};

[[nodiscard]] bool IsGpuDriverPresent(GgmlBackendKind kind);
[[nodiscard]] std::optional<GgmlBackendKind> BestAvailableGgmlBackendKind();

} // namespace Ayu::STT
