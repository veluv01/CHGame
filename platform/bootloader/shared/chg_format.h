/* Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. */
/*
 * CHGame package (.CHG), format version 1 - the file the SD game menu
 * installs. spec/chg.md is the normative description; tools/chgpack.py
 * (repository root) writes and checks these files and mirrors every constant
 * here.
 *
 *   0x000  512-byte header (all fields little-endian)
 *   0x200  payload: the program image exactly as chgame-upload would write it
 *          at CHGAME_APP_START (the sketch .bin padded with 0xFF to a
 *          multiple of 4)
 *   ...    optional: the game's picture, which the visual menu shows and the
 *          install ignores
 *   ...    optional: the game's record (JSON), which only the PC and web tools
 *          read, to back the game up from a card
 *
 * The package never names a flash address: the destination is always
 * CHGAME_APP_START, and layout_id says which memory map it was built for.
 */
#ifndef CHG_FORMAT_H
#define CHG_FORMAT_H

#define CHG_MAGIC            0x31474843u   /* "CHG1" */
#define CHG_FORMAT_VERSION   1u
#define CHG_HEADER_BYTES     512u
#define CHG_LAYOUT_ID        0x003000F7u   /* app at 0x3000, metadata page at 0xF700 */

/* Board target ids: which board a package's program was built for, one per
 * device of spec/chgame.md ("Devices and revisions", the naming rule and the
 * registry). Rev0's is named after its MCU and keeps that name for good;
 * every later board is "CGR<n>". A bootloader installs only its own board's.
 * Ids are never reused or reassigned. */
#define CHG_TARGET_REV0      0x35335843u   /* "CX35": CHGame Rev0 (CH32X035G8U6) */
#define CHG_TARGET_WEACT_F8U6 0x36384657u   /* "WF86": WeAct CH32X035F8U6 */
#define CHG_TARGET_REV1      0x31524743u   /* "CGR1": reserved for CHGame Rev1, not yet defined */

/* The board this bootloader is built for. A rev0 build defines nothing, and
 * its binaries are the ones built before boards had ids. Any other board's
 * build passes its id, -DCHGAME_BOARD_TARGET=0x31524743 for rev1 (with that
 * board's pins), and then also reports it: HELLO's board field and the board
 * word at offset 0x14 of the bootloader image (platform/board/docs/protocol.md). */
#ifdef CHGAME_BOARD_TARGET
#define CHG_TARGET_ID        CHGAME_BOARD_TARGET
#else
#define CHG_TARGET_ID        CHG_TARGET_REV0
#endif

#if defined(CHBOOT_WEACT_F8U6) && CHG_TARGET_ID != CHG_TARGET_WEACT_F8U6
#error "WeAct pins require the WF86 target id"
#endif
#if !defined(CHBOOT_WEACT_F8U6) && CHG_TARGET_ID == CHG_TARGET_WEACT_F8U6
#error "WF86 target id requires CHBOOT_WEACT_F8U6"
#endif

/* Field offsets. */
#define CHG_OFF_MAGIC        0x000u   /* u32 */
#define CHG_OFF_VERSION      0x004u   /* u16 format_version */
#define CHG_OFF_HDRBYTES     0x006u   /* u16 header_bytes */
#define CHG_OFF_TARGET       0x008u   /* u32 */
#define CHG_OFF_LAYOUT       0x00Cu   /* u32 */
#define CHG_OFF_PAYLOAD      0x010u   /* u32 payload_bytes: 4..CHGAME_APP_MAX_SIZE, a multiple of 4 */
#define CHG_OFF_PCRC         0x014u   /* u32 payload_crc32: CRC-32/ISO-HDLC of the payload */
#define CHG_OFF_APPVER       0x018u   /* u32 app_version (free for the author) */
#define CHG_OFF_FLAGS        0x01Cu   /* u32, 0 */
#define CHG_OFF_TITLE        0x020u   /* char[32], ASCII, NUL-padded: shown by the menu */
#define CHG_OFF_AUTHOR       0x040u   /* char[16] */
#define CHG_OFF_VERSTR       0x050u   /* char[8], e.g. "1.2" */
#define CHG_OFF_IMAGE        0x060u   /* u32 offset, u32 bytes, u32 crc32: the game's picture, 0 = none. A picture in
                                         MENU.BG's encoding (chgame_card.h): bytes 8,704, offset a multiple of 512
                                         after the payload and under 128 KiB. Readers may ignore the CRC */
#define CHG_OFF_RECORD       0x06Cu   /* u32 offset, u32 bytes, u32 crc32: the game's record, 0 = none. JSON for the
                                         tools that back a card up (spec/chg.md); no menu reads it. Offset a multiple
                                         of 512 after the payload and the picture, bytes at most 1 MiB */
#define CHG_OFF_HCRC         0x1FCu   /* u32 header_crc32: CRC-32/ISO-HDLC of bytes 0x000..0x1FB */

#define CHG_TITLE_LEN        32u

#endif
