#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>

#define WDD_VERSION L"1.1"

/* Defaults */
static size_t bs = 512;
static uint64_t count = UINT64_MAX;
static uint64_t skip = 0;
static uint64_t seek = 0;

/* Flags */
static int conv_noerror = 0;
static int conv_sync = 0;
static int conv_notrunc = 0;
static int conv_sparse = 0;

static int iflag_direct = 0;
static int oflag_direct = 0;

static int status_progress = 0;

/* Files */
static const wchar_t *ifile = L"-";
static const wchar_t *ofile = L"-";

/* --------------------------------------------------------- */

static void die_win(const wchar_t *msg)
{
    fwprintf(stderr, L"wdd: %ls (err=%lu)\n", msg, GetLastError());
    ExitProcess(1);
}

static void usage(void)
{
    fwprintf(stdout,
        L"Usage: wdd [OPTIONS]\n"
        L"Options:\n"
        L"  if=FILE       Input file (default: -)\n"
        L"  of=FILE       Output file (default: -)\n"
        L"  bs=SIZE       Block size (default: 512)\n"
        L"  count=N       Copy only N blocks\n"
        L"  skip=N        Skip N blocks from input\n"
        L"  seek=N        Skip N blocks on output\n"
        L"  conv=CONVS    sync,noerror,notrunc,sparse\n"
        L"  iflag=FLAGS   direct\n"
        L"  oflag=FLAGS   direct\n"
        L"  status=MODE   none,progress\n"
        L"  --help        Show this message\n"
        L"  --version     Show version\n"
    );
}

/* --------------------------------------------------------- */

static uint64_t parse_u64(const wchar_t *s)
{
    wchar_t *end;
    return wcstoull(s, &end, 0);
}

static size_t parse_size(const wchar_t *s)
{
    wchar_t *end;
    uint64_t v = wcstoull(s, &end, 0);

    if (*end == L'k' || *end == L'K') v <<= 10;
    else if (*end == L'm' || *end == L'M') v <<= 20;
    else if (*end == L'g' || *end == L'G') v <<= 30;

    return (size_t)v;
}

/* --------------------------------------------------------- */

int wmain(int argc, wchar_t **argv)
{
    for (int i = 1; i < argc; i++) {
        wchar_t *a = argv[i];

        if (wcscmp(a, L"--help") == 0) {
            usage();
            return 0;
        }
        if (wcscmp(a, L"--version") == 0) {
            fwprintf(stdout, L"wdd version %ls\n", WDD_VERSION);
            return 0;
        }
        if (wcsncmp(a, L"if=", 3) == 0) {
            ifile = a + 3;
        } else if (wcsncmp(a, L"of=", 3) == 0) {
            ofile = a + 3;
        } else if (wcsncmp(a, L"bs=", 3) == 0) {
            bs = parse_size(a + 3);
        } else if (wcsncmp(a, L"count=", 6) == 0) {
            count = parse_u64(a + 6);
        } else if (wcsncmp(a, L"skip=", 5) == 0) {
            skip = parse_u64(a + 5);
        } else if (wcsncmp(a, L"seek=", 5) == 0) {
            seek = parse_u64(a + 5);
        } else if (wcsncmp(a, L"conv=", 5) == 0) {
            wchar_t *p = a + 5;
            if (wcsstr(p, L"noerror")) conv_noerror = 1;
            if (wcsstr(p, L"sync")) conv_sync = 1;
            if (wcsstr(p, L"notrunc")) conv_notrunc = 1;
            if (wcsstr(p, L"sparse")) conv_sparse = 1;
        } else if (wcsncmp(a, L"iflag=", 6) == 0) {
            if (wcsstr(a + 6, L"direct")) iflag_direct = 1;
        } else if (wcsncmp(a, L"oflag=", 6) == 0) {
            if (wcsstr(a + 6, L"direct")) oflag_direct = 1;
        } else if (wcsncmp(a, L"status=", 7) == 0) {
            if (wcscmp(a + 7, L"progress") == 0)
                status_progress = 1;
        }
    }

    DWORD iflags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
    DWORD oflags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;

    if (iflag_direct)
        iflags |= FILE_FLAG_NO_BUFFERING;
    if (oflag_direct)
        oflags |= FILE_FLAG_NO_BUFFERING;

    HANDLE hin = (ifile[0] == L'-' && ifile[1] == 0)
        ? GetStdHandle(STD_INPUT_HANDLE)
        : CreateFileW(ifile, GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, iflags, NULL);

    if (hin == INVALID_HANDLE_VALUE)
        die_win(L"CreateFile(input)");

    HANDLE hout = (ofile[0] == L'-' && ofile[1] == 0)
        ? GetStdHandle(STD_OUTPUT_HANDLE)
        : CreateFileW(ofile,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            conv_notrunc ? OPEN_EXISTING : CREATE_ALWAYS,
            oflags,
            NULL);

    if (hout == INVALID_HANDLE_VALUE)
        die_win(L"CreateFile(output)");

    LARGE_INTEGER off;
    off.QuadPart = (LONGLONG)(skip * bs);
    SetFilePointerEx(hin, off, NULL, FILE_BEGIN);

    off.QuadPart = (LONGLONG)(seek * bs);
    SetFilePointerEx(hout, off, NULL, FILE_BEGIN);

    BYTE *buf = (BYTE *)_aligned_malloc(bs, 4096);
    if (!buf)
        die_win(L"alloc");

    uint64_t in = 0, out = 0;
    DWORD r, w;

    for (uint64_t i = 0; i < count; i++) {
        if (!ReadFile(hin, buf, (DWORD)bs, &r, NULL) || r == 0) {
            if (conv_noerror)
                continue;
            break;
        }

        in++;

        if (conv_sync && r < bs)
            ZeroMemory(buf + r, bs - r);

        if (!WriteFile(hout, buf, r, &w, NULL)) {
            if (!conv_noerror)
                die_win(L"WriteFile");
        } else {
            out++;
        }

        if (status_progress) {
            fwprintf(stderr, L"\r%llu bytes copied",
                (unsigned long long)(out * bs));
        }
    }

    fwprintf(stderr,
        L"\n%llu+0 records in\n%llu+0 records out\n%llu bytes copied\n",
        (unsigned long long)in,
        (unsigned long long)out,
        (unsigned long long)(out * bs));

    _aligned_free(buf);
    CloseHandle(hin);
    CloseHandle(hout);
    return 0;
}
