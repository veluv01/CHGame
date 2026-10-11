package main

// CHG packages for the SD game menu: `chgame-upload pack`.
//
// A package is a 512-byte header plus the program image as flash would write
// it (spec/chg.md in the CHGame repository). platform.txt runs pack
// after every build, so Sketch > Export Compiled Binary leaves a .chg beside
// the .bin: copy it into the card's GAMES folder and the menu lists it.
//
// The constants mirror platform/bootloader/shared/chg_format.h; the shared
// vectors (test/protocol/vectors.json) hold this, the Python tool and
// tools/chgpack.py to the same bytes.

import (
	"encoding/binary"
	"fmt"
	"hash/crc32"
	"path/filepath"
	"strings"
)

const (
	chgMagic         = 0x31474843 // "CHG1"
	chgFormatVersion = 1
	chgHeaderBytes   = 512
	chgLayoutID      = 0x003000F7 // app at 0x3000, metadata page at 0xF700
	chgBootSig       = 0x4C424843 // "CHBL" at payload offset 8 marks a bootloader image
	chgTitleLen      = 32
	chgAuthorLen     = 16
	chgVerstrLen     = 8

	// Board target ids (chg_format.h CHG_TARGET_*): which board a program, a
	// package or a bootloader is for. Rev0's is named after its MCU and keeps
	// that name; later boards are "CGR<n>" (spec/chgame.md, "Devices and
	// revisions"). Never reused or reassigned.
	chgTargetRev0 = 0x35335843 // "CX35": CHGame Rev0 (CH32X035G8U6)
	chgTargetWeActF8U6 = 0x36384657 // "WF86": WeAct CH32X035F8U6
	chgTargetRev1 = 0x31524743 // "CGR1": reserved for CHGame Rev1, not yet defined
)

// The boards this tool uploads and packs for, and the names assigned to
// boards not defined yet (refused, by name).
var (
	chgDevices  = map[string]uint32{"rev0": chgTargetRev0, "weact-f8u6": chgTargetWeActF8U6}
	chgReserved = map[string]uint32{"rev1": chgTargetRev1}
)

// fourcc: a 32-bit id as the four characters it spells (little-endian), or "?".
func fourcc(v uint32) string {
	b := []byte{byte(v), byte(v >> 8), byte(v >> 16), byte(v >> 24)}
	for _, c := range b {
		if c <= 32 || c >= 127 {
			return "?"
		}
	}
	return string(b)
}

// boardName: "rev0 (CX35)", "rev1 (CGR1)" for a reserved one, else the id.
func boardName(target uint32) string {
	for _, m := range []map[string]uint32{chgDevices, chgReserved} {
		for name, t := range m {
			if t == target {
				return fmt.Sprintf("%s (%s)", name, fourcc(t))
			}
		}
	}
	return fmt.Sprintf("unknown board 0x%08X (%s)", target, fourcc(target))
}

// targetOf: the target id of a device this tool knows ("rev0").
func targetOf(device string) (uint32, error) {
	if t, ok := chgDevices[device]; ok {
		return t, nil
	}
	if _, ok := chgReserved[device]; ok {
		return 0, fmt.Errorf("%s is reserved for a board that is not defined yet", device)
	}
	return 0, fmt.Errorf("unknown device %q (known: rev0, weact-f8u6)", device)
}

func chgText(dst []byte, s, what string) error {
	if len(s) >= len(dst) {
		return fmt.Errorf("%s must be printable ASCII, at most %d characters", what, len(dst)-1)
	}
	for i := 0; i < len(s); i++ {
		if s[i] < 32 || s[i] > 126 {
			return fmt.Errorf("%s must be printable ASCII, at most %d characters", what, len(dst)-1)
		}
	}
	copy(dst, s)
	return nil
}

// chgDefaultTitle: MyGame.ino.bin -> MYGAME, the sketch's name as the menu's capitals.
func chgDefaultTitle(path string) string {
	base := filepath.Base(path)
	if i := strings.Index(base, "."); i >= 0 {
		base = base[:i]
	}
	t := strings.ToUpper(base)
	if len(t) > chgTitleLen-1 {
		t = t[:chgTitleLen-1]
	}
	if t == "" {
		t = "PROGRAM"
	}
	return t
}

// chgDefaultOutput: MyGame.ino.bin -> MyGame.ino.chg, beside it (Export
// Compiled Binary copies every <project>.* file from the build folder).
func chgDefaultOutput(path string) string {
	ext := filepath.Ext(path)
	if strings.EqualFold(ext, ".bin") {
		return path[:len(path)-len(ext)] + ".chg"
	}
	return path + ".chg"
}

// chgPack packs an image built for rev0.
func chgPack(image []byte, title, author, ver string, appVersion uint32) ([]byte, error) {
	return chgPackFor(image, title, author, ver, appVersion, chgTargetRev0)
}

// chgPackFor packs an image built for the board with this target id.
func chgPackFor(image []byte, title, author, ver string, appVersion, target uint32) ([]byte, error) {
	payload := padToWord(image)
	if len(payload) == 0 || len(payload) > appMaxSize {
		return nil, fmt.Errorf("image is %d B; the limit is %d B", len(payload), appMaxSize)
	}
	if len(payload) >= 12 && binary.LittleEndian.Uint32(payload[8:]) == chgBootSig {
		return nil, fmt.Errorf("this is a bootloader image, not a program")
	}
	h := make([]byte, chgHeaderBytes)
	le := binary.LittleEndian
	le.PutUint32(h[0x00:], chgMagic)
	le.PutUint16(h[0x04:], chgFormatVersion)
	le.PutUint16(h[0x06:], chgHeaderBytes)
	le.PutUint32(h[0x08:], target)
	le.PutUint32(h[0x0C:], chgLayoutID)
	le.PutUint32(h[0x10:], uint32(len(payload)))
	le.PutUint32(h[0x14:], crc32.ChecksumIEEE(payload))
	le.PutUint32(h[0x18:], appVersion)
	if err := chgText(h[0x20:0x20+chgTitleLen], title, "title"); err != nil {
		return nil, err
	}
	if err := chgText(h[0x40:0x40+chgAuthorLen], author, "author"); err != nil {
		return nil, err
	}
	if err := chgText(h[0x50:0x50+chgVerstrLen], ver, "version"); err != nil {
		return nil, err
	}
	le.PutUint32(h[0x1FC:], crc32.ChecksumIEEE(h[:0x1FC]))
	return append(h, payload...), nil
}
