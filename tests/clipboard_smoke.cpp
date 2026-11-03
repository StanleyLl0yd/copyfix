// Win32-only regression against the real clipboard and production helpers.
// Runs with disposable synthetic data on an ephemeral Windows CI runner.
// Never packaged with CopyFix.exe.
#define wWinMain CopyFixUnusedWinMainForClipboardSmoke
#include "../src/winmain.cpp"
#undef wWinMain

#include <iostream>
#include <string>

namespace {
bool PutText(const std::wstring& text, bool add_rich = false) {
    HGLOBAL data = AllocateClipboardString(text);
    if (!data) return false;
    if (!OpenClipboardBounded(g_message)) {
        GlobalFree(data);
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        GlobalFree(data);
        return false;
    }
    if (!SetClipboardData(CF_UNICODETEXT, data)) {
        CloseClipboard();
        GlobalFree(data);
        return false;
    }
    if (add_rich) {
        const UINT format = RegisterClipboardFormatW(L"CopyFix.Test.SyntheticRich");
        HGLOBAL extra = GlobalAlloc(GMEM_MOVEABLE, 16);
        if (!format || !extra) {
            if (extra) GlobalFree(extra);
            CloseClipboard();
            return false;
        }
        if (!SetClipboardData(format, extra)) {
            GlobalFree(extra);
            CloseClipboard();
            return false;
        }
    }
    return CloseClipboard() != FALSE;
}

// An independent Win32 thread holds the clipboard open to exercise retries.
struct ClipboardHold {
    HANDLE entered = nullptr;
    HANDLE release = nullptr;
    volatile LONG opened = 0;
};
DWORD WINAPI HoldClipboard(LPVOID parameter) {
    auto& hold = *static_cast<ClipboardHold*>(parameter);
    const BOOL opened = OpenClipboard(nullptr);
    InterlockedExchange(&hold.opened, opened ? 1 : 0);
    SetEvent(hold.entered);
    if (opened) {
        WaitForSingleObject(hold.release, 10000);
        CloseClipboard();
    }
    return 0;
}
bool RefusesContendedClipboard(const ClipSnapshot& expected) {
    ClipboardHold hold;
    hold.entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    hold.release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hold.entered || !hold.release) {
        if (hold.entered) CloseHandle(hold.entered);
        if (hold.release) CloseHandle(hold.release);
        return false;
    }
    HANDLE thread = CreateThread(nullptr, 0, HoldClipboard, &hold, 0, nullptr);
    if (!thread) {
        CloseHandle(hold.entered);
        CloseHandle(hold.release);
        return false;
    }
    const bool ready = WaitForSingleObject(hold.entered, 5000) == WAIT_OBJECT_0 &&
                       InterlockedCompareExchange(&hold.opened, 0, 0) == 1;
    bool refused = false;
    if (ready) {
        ClipSnapshot captured;
        refused = !Snapshot(captured) &&
                  !WriteIfUnchanged(expected, L"Нельзя записать при блокировке", nullptr);
    }
    SetEvent(hold.release);
    const bool released = WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0;
    CloseHandle(thread);
    CloseHandle(hold.entered);
    CloseHandle(hold.release);
    return ready && refused && released;
}
}  // namespace

int main() {
    const auto fail = [](int code) {
        std::cerr << "CLIPBOARD_SMOKE_FAILURE_" << code << "\n";
        return code;
    };
    g_instance = GetModuleHandleW(nullptr);
    g_message = CreateWindowExW(0, L"STATIC", L"CopyFix clipboard test", WS_OVERLAPPED,
                                0, 0, 1, 1, nullptr, nullptr, g_instance, nullptr);
    if (!g_message) return fail(1);
    // CR, LF and CRLF normalization must compare exactly without allocating.
    for (const auto* value : {L"", L"plain", L"A\nB", L"A\rB",
                              L"A\r\nB", L"A\n\r\nB", L"\r", L"\n", L"\r\n"}) {
        if (!EqualsClipboardLineEndings(value, copyfix::ToWindowsLineEndings(value)))
            return fail(33);
    }
    if (EqualsClipboardLineEndings(L"A\nB", L"A\nB") ||
        EqualsClipboardLineEndings(L"A\nB", L"A\rB") ||
        EqualsClipboardLineEndings(L"A", L"A\r\n")) return fail(34);
    const auto original = std::wstring(L"Текст был скопиро-\r\nван из PDF.");
    if (!PutText(original)) return fail(2);

    ClipSnapshot snapshot;
    if (!Snapshot(snapshot) || snapshot.sequence == 0 ||
        snapshot.text != original || snapshot.rich_formats) return fail(3);
    const auto cleaned = copyfix::Clean(snapshot.text, copyfix::Profile::Deep);
    if (cleaned.protected_structure || !cleaned.requires_confirmation ||
        cleaned.text != L"Текст был скопирован из PDF.") return fail(4);
    DWORD sequence = 0;
    if (!WriteIfUnchanged(snapshot, cleaned.text, &sequence) || sequence == 0) return fail(5);
    ClipSnapshot applied;
    if (!Snapshot(applied) || applied.sequence != sequence ||
        applied.text != cleaned.text) return fail(6);
    if (WriteIfUnchanged(snapshot, L"Нельзя перезаписывать", nullptr)) return fail(7);
    ClipSnapshot after_stale_write;
    if (!Snapshot(after_stale_write) || after_stale_write.text != cleaned.text) return fail(8);

    // Exercise the same pending->AcceptPending->UndoLast pipeline as the tray.
    const auto undo_source = std::wstring(L"  Привет   мир  ");
    if (!PutText(undo_source)) return fail(9);
    ClipSnapshot before_undo;
    if (!Snapshot(before_undo)) return fail(10);
    g_pending = Pending{std::move(before_undo),
                        copyfix::Clean(undo_source, copyfix::Profile::Deep)};
    AcceptPending();
    if (!g_undo.available) return fail(11);
    ClipSnapshot fixed;
    if (!Snapshot(fixed) || fixed.text != L"Привет мир") return fail(12);
    UndoLast();
    ClipSnapshot undone;
    if (!Snapshot(undone) || undone.text != undo_source || g_undo.available) return fail(13);

    // Copying anything else invalidates Undo instead of overwriting new data.
    if (!PutText(undo_source)) return fail(14);
    ClipSnapshot repeat;
    if (!Snapshot(repeat)) return fail(15);
    g_pending = Pending{std::move(repeat),
                        copyfix::Clean(undo_source, copyfix::Profile::Deep)};
    AcceptPending();
    if (!g_undo.available) return fail(16);
    if (!PutText(L"Новый текст из другой программы")) return fail(17);
    UndoLast();
    ClipSnapshot changed;
    if (!Snapshot(changed) || changed.text != L"Новый текст из другой программы" ||
        g_undo.available) return fail(18);

    // Non-plain formats must be flagged so the UI requires confirmation.
    if (!PutText(L"Текст с дополнительным форматом", true)) return fail(19);
    ClipSnapshot with_rich;
    if (!Snapshot(with_rich) || !with_rich.rich_formats) return fail(20);
    // A second thread may hold the clipboard open. Bounded retries must
    // fail without clearing or replacing the previously copied data.
    if (!PutText(L"Защищённый от блокировки буфер")) return fail(21);
    ClipSnapshot before_contention;
    if (!Snapshot(before_contention)) return fail(22);
    if (!RefusesContendedClipboard(before_contention)) return fail(23);
    ClipSnapshot after_contention;
    if (!Snapshot(after_contention) ||
        after_contention.sequence != before_contention.sequence ||
        after_contention.text != before_contention.text) return fail(24);

    // An uninspectable preview must not overwrite a long source payload.
    std::wstring beyond_preview(kMaxPreviewChars + 1, L'ж');
    beyond_preview[0] = L' ';
    beyond_preview[1] = L' ';
    if (!PutText(beyond_preview)) return fail(25);
    ClipSnapshot preview_source;
    if (!Snapshot(preview_source) || preview_source.text != beyond_preview) return fail(26);
    g_settings.profile = copyfix::Profile::Deep;
    FixClipboard(true);
    ClipSnapshot still_unmodified;
    if (!Snapshot(still_unmodified) ||
        still_unmodified.sequence != preview_source.sequence ||
        still_unmodified.text != beyond_preview ||
        g_preview != nullptr) return fail(27);

    // Exercise the exact documented million-character clipboard boundary.
    std::wstring maximum(kMaxChars, L'Я');
    if (!PutText(maximum)) return fail(28);
    ClipSnapshot maximum_snapshot;
    if (!Snapshot(maximum_snapshot) || maximum_snapshot.text != maximum) return fail(29);
    if (!EqualsClipboardLineEndings(maximum, maximum) ||
        EqualsClipboardLineEndings(maximum, maximum + L"extra")) return fail(35);
    if (AllocateClipboardString(std::wstring(kMaxChars + 1, L'Я')) != nullptr)
        return fail(30);

    // Invalid surrogate halves must not reach the text engine.
    std::wstring invalid = L"Текст ";
    invalid.push_back(static_cast<wchar_t>(0xD800));
    if (!PutText(invalid)) return fail(31);
    ClipSnapshot invalid_snapshot;
    if (Snapshot(invalid_snapshot)) return fail(32);

    DestroyWindow(g_message);
    g_message = nullptr;
    std::cout << "PASS clipboard contention, size boundaries, UTF-16, "
                 "stale-write guard, Undo and rich formats\\n";
    return 0;
}
