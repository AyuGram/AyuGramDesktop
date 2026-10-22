def qt_options(options):
    architectures = [name for name in ("arm64", "x86_64") if "mac-" + name in options]
    if len(architectures) > 1:
        raise ValueError("Choose one macOS architecture, or omit both for universal.")
    if "qt-release-only" in options and "skip-release" in options:
        raise ValueError("qt-release-only conflicts with skip-release.")
    configuration = (
        "-debug" if "skip-release" in options else
        "-release" if "qt-release-only" in options else
        "-debug-and-release"
    )
    return configuration, ";".join(architectures or ["x86_64", "arm64"])
