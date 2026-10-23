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
    const auto original = std::wstring(L"Текст был скопиро-\r\nван из PDF.");
    if (!PutText(original)) return fail(2);

    ClipSnapshot snapshot;
    if (!Snapshot(snapshot) || snapshot.text != original || snapshot.rich_formats) return fail(3);
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
    DestroyWindow(g_message);
    g_message = nullptr;
    std::cout << "PASS real clipboard Unicode, stale-write protection, Undo and rich-format detection\n";
    return 0;
}
