// SPDX-License-Identifier: GPL-3.0-or-later
//
// The single-file portable Hop-tunes.exe.
//
// This program is a launcher with the whole app appended to it as a zip ("payload"), followed by
// a 16-byte footer: the 8 bytes "HOPTPAY1" and the payload size as a little-endian 64-bit number.
// On first run it unpacks the payload into %LOCALAPPDATA%\Hop-tunes\portable\rt-<id> (the id comes
// from the payload, so a new version unpacks beside the old one), then starts hop-tunes.exe from
// there with HOPTUNES_DATA_DIR pointing at a "Hop-tunes-data" folder next to this exe, so the user's
// profiles, playlists and settings travel with the file. Later runs skip the unpacking.

#include <windows.h>
#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr char kMagic[8] = {'H', 'O', 'P', 'T', 'P', 'A', 'Y', '1'};
constexpr uint64_t kFooterSize = 16;

// ---- the little "unpacking" window -------------------------------------------------------------

HWND g_window = nullptr;
HWND g_label = nullptr;

LRESULT CALLBACK windowProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CLOSE) return 0;   // let the unpack finish rather than leave a half-written folder
    return DefWindowProcW(h, m, w, l);
}

void pumpMessages();

void showWindow()
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512) /* IDC_ARROW */);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"HopTunesUnpack";
    RegisterClassW(&wc);
    const int w = 380, h = 110;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    g_window = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"Hop-tunes",
                               WS_CAPTION | WS_VISIBLE, x, y, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    g_label = CreateWindowExW(0, L"STATIC", L"Getting Hop-tunes ready (first launch only)...",
                              WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 28, w - 36, 40, g_window, nullptr, wc.hInstance, nullptr);
    pumpMessages();
}

void pumpMessages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}

void setProgress(int percent)
{
    if (!g_label) return;
    const std::wstring text = L"Getting Hop-tunes ready (first launch only)... " + std::to_wstring(percent) + L"%";
    SetWindowTextW(g_label, text.c_str());
    pumpMessages();
}

void fail(const std::wstring &what)
{
    MessageBoxW(nullptr, what.c_str(), L"Hop-tunes", MB_OK | MB_ICONERROR);
}

// ---- zip reading ---------------------------------------------------------------------------------

uint16_t u16(const unsigned char *p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t u32(const unsigned char *p) { return uint32_t(u16(p)) | uint32_t(u16(p + 2)) << 16; }

struct Entry {
    std::string name;
    uint16_t method = 0;
    uint64_t compSize = 0, size = 0, localOffset = 0;
};

struct Payload {
    FILE *file = nullptr;
    uint64_t start = 0, size = 0;
    std::vector<Entry> entries;
    uint32_t id = 0;   // crc of the central directory: names this payload's unpack folder
};

bool seekTo(FILE *f, uint64_t pos) { return _fseeki64(f, int64_t(pos), SEEK_SET) == 0; }

bool readAt(FILE *f, uint64_t pos, void *buf, size_t n) { return seekTo(f, pos) && fread(buf, 1, n, f) == n; }

bool openPayload(const std::wstring &self, Payload &p)
{
    p.file = _wfopen(self.c_str(), L"rb");
    if (!p.file) return false;
    if (_fseeki64(p.file, 0, SEEK_END) != 0) return false;
    const int64_t total = _ftelli64(p.file);
    if (total < int64_t(kFooterSize)) return false;
    unsigned char footer[kFooterSize];
    if (!readAt(p.file, uint64_t(total) - kFooterSize, footer, kFooterSize)) return false;
    if (std::memcmp(footer, kMagic, 8) != 0) return false;
    uint64_t size = 0;
    for (int i = 7; i >= 0; --i) size = size << 8 | footer[8 + i];
    if (size == 0 || size + kFooterSize > uint64_t(total)) return false;
    p.size = size;
    p.start = uint64_t(total) - kFooterSize - size;

    // End of central directory, within the last 64 KB of the payload.
    const size_t tail = size_t(std::min<uint64_t>(size, 65557));
    std::vector<unsigned char> buf(tail);
    if (!readAt(p.file, p.start + size - tail, buf.data(), tail)) return false;
    size_t eocd = std::string::npos;
    for (size_t i = tail >= 22 ? tail - 22 : 0; ; --i) {
        if (i + 22 <= tail && u32(&buf[i]) == 0x06054b50) { eocd = i; break; }
        if (i == 0) break;
    }
    if (eocd == std::string::npos) return false;
    const uint32_t count = u16(&buf[eocd + 10]), cdSize = u32(&buf[eocd + 12]), cdOffset = u32(&buf[eocd + 16]);
    std::vector<unsigned char> cd(cdSize);
    if (!readAt(p.file, p.start + cdOffset, cd.data(), cdSize)) return false;
    p.id = uint32_t(crc32(0, cd.data(), uInt(cdSize)));

    size_t o = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (o + 46 > cd.size() || u32(&cd[o]) != 0x02014b50) return false;
        Entry e;
        e.method = u16(&cd[o + 10]);
        e.compSize = u32(&cd[o + 20]);
        e.size = u32(&cd[o + 24]);
        e.localOffset = u32(&cd[o + 42]);
        const uint16_t nameLen = u16(&cd[o + 28]), extraLen = u16(&cd[o + 30]), commentLen = u16(&cd[o + 32]);
        if (o + 46 + nameLen > cd.size()) return false;
        e.name.assign(reinterpret_cast<const char *>(&cd[o + 46]), nameLen);
        o += 46 + size_t(nameLen) + extraLen + commentLen;
        p.entries.push_back(std::move(e));
    }
    return !p.entries.empty();
}

bool safeName(const std::string &n)
{
    if (n.empty() || n[0] == '/' || n[0] == '\\' || n.find(':') != std::string::npos) return false;
    return n.find("..") == std::string::npos;
}

bool extractEntry(Payload &p, const Entry &e, const fs::path &root)
{
    if (!safeName(e.name)) return false;
    const fs::path out = root / fs::u8path(e.name);
    if (!e.name.empty() && e.name.back() == '/') { std::error_code ec; fs::create_directories(out, ec); return true; }
    {
        std::error_code ec;
        fs::create_directories(out.parent_path(), ec);
    }
    unsigned char lh[30];
    if (!readAt(p.file, p.start + e.localOffset, lh, sizeof lh) || u32(lh) != 0x04034b50) return false;
    uint64_t pos = p.start + e.localOffset + 30 + u16(lh + 26) + u16(lh + 28);

    FILE *dst = _wfopen(out.c_str(), L"wb");
    if (!dst) return false;
    bool ok = true;
    std::vector<unsigned char> in(1 << 16), outBuf(1 << 16);
    uint64_t left = e.compSize, written = 0;
    if (e.method == 0) {
        while (left > 0 && ok) {
            const size_t n = size_t(std::min<uint64_t>(left, in.size()));
            ok = readAt(p.file, pos, in.data(), n) && fwrite(in.data(), 1, n, dst) == n;
            pos += n; left -= n; written += n;
        }
    } else if (e.method == 8) {
        z_stream zs{};
        ok = inflateInit2(&zs, -15) == Z_OK;
        int rc = Z_OK;
        while (ok && rc != Z_STREAM_END) {
            if (zs.avail_in == 0) {
                const size_t n = size_t(std::min<uint64_t>(left, in.size()));
                if (n == 0) { ok = false; break; }
                ok = readAt(p.file, pos, in.data(), n);
                pos += n; left -= n;
                zs.next_in = in.data();
                zs.avail_in = uInt(n);
                if (!ok) break;
            }
            zs.next_out = outBuf.data();
            zs.avail_out = uInt(outBuf.size());
            rc = inflate(&zs, Z_NO_FLUSH);
            if (rc != Z_OK && rc != Z_STREAM_END) { ok = false; break; }
            const size_t have = outBuf.size() - zs.avail_out;
            if (have && fwrite(outBuf.data(), 1, have, dst) != have) { ok = false; break; }
            written += have;
        }
        inflateEnd(&zs);
    } else {
        ok = false;
    }
    if (fclose(dst) != 0) ok = false;
    return ok && written == e.size;
}

// ---- paths and process ---------------------------------------------------------------------------

std::wstring envVar(const wchar_t *name)
{
    std::wstring v(32768, L'\0');
    const DWORD n = GetEnvironmentVariableW(name, v.data(), DWORD(v.size()));
    v.resize(n < v.size() ? n : 0);
    return v;
}

bool writableDir(const fs::path &dir)
{
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path probe = dir / L".write-test";
    FILE *f = _wfopen(probe.c_str(), L"wb");
    if (!f) return false;
    fclose(f);
    fs::remove(probe, ec);
    return true;
}

// Everything after the program name in the original command line, passed on unchanged.
std::wstring restOfCommandLine()
{
    const std::wstring cl = GetCommandLineW();
    size_t i = 0;
    if (!cl.empty() && cl[0] == L'"') {
        i = cl.find(L'"', 1);
        i = (i == std::wstring::npos) ? cl.size() : i + 1;
    } else {
        i = cl.find_first_of(L" \t");
        if (i == std::wstring::npos) i = cl.size();
    }
    while (i < cl.size() && (cl[i] == L' ' || cl[i] == L'\t')) ++i;
    return cl.substr(i);
}

wchar_t hex(unsigned v) { return L"0123456789abcdef"[v & 15]; }

std::wstring idString(const Payload &p)
{
    std::wstring s = L"rt-";
    for (int sh = 28; sh >= 0; sh -= 4) s += hex(p.id >> sh);
    s += L"-" + std::to_wstring(p.size);
    return s;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t selfBuf[32768];
    const DWORD n = GetModuleFileNameW(nullptr, selfBuf, DWORD(sizeof selfBuf / sizeof selfBuf[0]));
    if (n == 0 || n >= sizeof selfBuf / sizeof selfBuf[0]) { fail(L"Cannot find out where this program is."); return 1; }
    const fs::path self(selfBuf);

    Payload payload;
    if (!openPayload(self.wstring(), payload)) {
        fail(L"This copy of Hop-tunes is damaged (the program data is missing). Please download it again.");
        return 1;
    }

    std::wstring local = envVar(L"LOCALAPPDATA");
    if (local.empty()) local = fs::temp_directory_path().wstring();
    const fs::path cacheRoot = fs::path(local) / L"Hop-tunes" / L"portable";
    const fs::path runtime = cacheRoot / idString(payload);
    const fs::path ready = runtime / L".ready";

    if (!fs::exists(ready)) {
        showWindow();
        setProgress(0);
        std::error_code ec;
        // Older unpacked versions are no longer needed (one still in use just stays: its files are locked).
        if (fs::exists(cacheRoot, ec))
            for (const auto &d : fs::directory_iterator(cacheRoot, ec))
                if (d.path().filename() != runtime.filename()) fs::remove_all(d.path(), ec);

        const fs::path tmp = cacheRoot / (runtime.filename().wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()));
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp, ec);
        uint64_t totalBytes = 0, doneBytes = 0;
        for (const Entry &e : payload.entries) totalBytes += e.size;
        for (const Entry &e : payload.entries) {
            if (!extractEntry(payload, e, tmp)) {
                fs::remove_all(tmp, ec);
                fail(L"Hop-tunes could not be unpacked. Check that there is enough free disk space and try again.");
                return 1;
            }
            doneBytes += e.size;
            setProgress(totalBytes ? int(doneBytes * 100 / totalBytes) : 100);
        }
        if (FILE *f = _wfopen((tmp / L".ready").c_str(), L"wb")) fclose(f);
        // Another copy may have unpacked the same version meanwhile; whichever is in place wins.
        fs::rename(tmp, runtime, ec);
        if (ec) fs::remove_all(tmp, ec);
        if (!fs::exists(ready)) { fail(L"Hop-tunes could not be unpacked."); return 1; }
        if (g_window) DestroyWindow(g_window);
    }
    fclose(payload.file);

    // Keep the user's data beside this exe; fall back to the user profile if that place is read-only.
    fs::path data = self.parent_path() / L"Hop-tunes-data";
    if (!writableDir(data)) {
        data = fs::path(local) / L"Hop-tunes" / L"portable-data";
        writableDir(data);
    }
    SetEnvironmentVariableW(L"HOPTUNES_DATA_DIR", data.c_str());

    const fs::path app = runtime / L"hop-tunes.exe";
    std::wstring cmd = L"\"" + app.wstring() + L"\"";
    const std::wstring rest = restOfCommandLine();
    if (!rest.empty()) cmd += L" " + rest;

    STARTUPINFOW si{};
    si.cb = sizeof si;
    // Pass redirected standard handles on, so "hop-tunes-portable.exe --version > out.txt" works.
    const HANDLE hin = GetStdHandle(STD_INPUT_HANDLE), hout = GetStdHandle(STD_OUTPUT_HANDLE), herr = GetStdHandle(STD_ERROR_HANDLE);
    const bool haveOut = hout && hout != INVALID_HANDLE_VALUE;
    if (haveOut) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hin;
        si.hStdOutput = hout;
        si.hStdError = (herr && herr != INVALID_HANDLE_VALUE) ? herr : hout;
    }
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');
    if (!CreateProcessW(app.c_str(), mutableCmd.data(), nullptr, nullptr, haveOut ? TRUE : FALSE, 0, nullptr,
                        runtime.c_str(), &si, &pi)) {
        fail(L"Hop-tunes could not be started.");
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return int(code);
}
