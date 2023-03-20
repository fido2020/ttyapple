#include <uefi/UefiBaseType.h>
#include <uefi/UefiSpec.h>

void play_frames();

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* console;
EFI_TEXT_STRING output_string;
EFI_TEXT_SET_CURSOR_POSITION set_cursor_position;

EFI_STALL stall;

void __chkstk() {}

void us_sleep(long us) {
    stall(us);
}

void reset_cursor() {
    set_cursor_position(console, 0, 0);
}

void print_text(CHAR16* text) {
    output_string(console, text);
}

EFI_STATUS efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE* systemTbl) {
    console = systemTbl->ConOut;
    output_string = console->OutputString;
    set_cursor_position = console->SetCursorPosition;

    stall = systemTbl->BootServices->Stall;

    play_frames();

    return 0;
}
