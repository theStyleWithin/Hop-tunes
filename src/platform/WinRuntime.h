// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Windows start-up helpers (compiled on Windows only).
namespace hop::winruntime {

// Points GStreamer at the plugins that ship next to hop-tunes.exe (lib/gstreamer-1.0), so the
// installed or portable build never picks up another GStreamer on the machine. Does nothing when
// that folder is absent (a developer build). Call before gst_init().
void setupBundledRuntime();

// hop-tunes.exe is a GUI program, which has no console, so --help and --version would print
// nowhere. When asked for text output, attach to the console it was started from (cmd, PowerShell)
// unless stdout is already redirected to a file or pipe.
void attachConsoleForTextOutput();

} // namespace hop::winruntime
