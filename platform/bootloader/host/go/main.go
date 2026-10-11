// Command chgame-upload is the CHGame board uploader.
//
// It owns the ENTIRE app -> bootloader -> app transition: find the port, do the
// 1200-baud touch, wait for the bootloader, erase, write, verify, launch, and
// confirm the sketch came back. Arduino's own 1200bps touch and port-waiting are
// disabled in boards.txt precisely so there is only one actor on the port -
// two racing for it is the classic source of flaky native-USB uploads.
//
// A single static binary with no runtime dependency, shipped as an Arduino tool
// dependency. That is what the ecosystem does (avrdude, bossac, openocd) and it
// is why this is Go rather than the Python prototype it replaces: Arduino ships
// no Python interpreter, and freezing one would need a build machine per OS.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"
)

var version = "0.2.1-weact.1"

type opts struct {
	port    string
	timeout time.Duration
	debug   bool
	verbose bool
	quiet   bool
}

func die(format string, a ...any) {
	fmt.Fprintf(os.Stderr, format+"\n", a...)
	os.Exit(1)
}

func resolvePort(o *opts) string {
	if o.port != "" {
		return o.port
	}
	ports, err := findPorts()
	if err != nil {
		die("could not enumerate serial ports: %v", err)
	}
	switch len(ports) {
	case 0:
		die("no CHGame device found (looked for %s:%s). Is it plugged in?", chgameVID, chgamePID)
	case 1:
		return ports[0]
	default:
		die("several CHGame devices found: %s - pass -port", strings.Join(ports, ", "))
	}
	return ""
}

func progressBar(quiet bool) func(done, total int) {
	if quiet {
		return nil
	}
	const width = 32
	return func(done, total int) {
		filled, pct := 0, 100
		if total > 0 {
			filled = width * done / total
			pct = 100 * done / total
		}
		fmt.Printf("\r  [%s%s] %3d%%  %d/%d B",
			strings.Repeat("#", filled), strings.Repeat(".", width-filled),
			pct, done, total)
		if done >= total {
			fmt.Println()
		}
	}
}

func main() {
	o := &opts{}
	bind := func(fs *flag.FlagSet) {
		fs.StringVar(&o.port, "port", "", "serial port (auto-detected if omitted)")
		fs.DurationVar(&o.timeout, "timeout", 10*time.Second, "per-command timeout")
		fs.BoolVar(&o.debug, "debug", false, "dump frames in both directions")
		// Arduino's platform.txt expands {upload.verbose} to one of these.
		fs.BoolVar(&o.verbose, "verbose", false, "more detail")
		fs.BoolVar(&o.quiet, "quiet", false, "less detail")
	}

	global := flag.NewFlagSet("chgame-upload", flag.ExitOnError)
	bind(global)
	global.Usage = func() {
		fmt.Fprintf(os.Stderr, `chgame-upload %s

  chgame-upload [flags] probe
  chgame-upload [flags] info
  chgame-upload [flags] touch
  chgame-upload [flags] run
  chgame-upload [flags] flash <file.bin> [-verify] [-run] [-device rev0]
  chgame-upload [flags] selfupdate <boot.bin>
  chgame-upload [flags] provision -bootloader <boot.bin> [-app <app.bin>] [-wchisp <path>]
  chgame-upload [flags] burn -method usb|isp -bootloader <boot.bin> [-app <app.bin>] [-wchisp <path>]
  chgame-upload [flags] pack <file.bin> [-out <file.chg>] [-title T] [-author A] [-gameversion V] [-device rev0]

  flash       upload a sketch through the bootloader (what Upload does)
  selfupdate  replace the bootloader over USB, through the one installed
  provision   write the bootloader through the chip's factory ISP (hold BOOT, power cycle)
  burn        what Arduino's Burn Bootloader and Upload Using Programmer run:
              selfupdate (usb) or provision (isp), then the sketch if one is given
  pack        wrap a sketch image in a .chg package for the SD game menu (every
              build runs it, so Export Compiled Binary leaves one by the sketch)
  chgame-upload [flags] noop

-device names the board the image was built for (rev0; spec/chgame.md's
device table): pack writes its target id, and flash refuses a board whose
bootloader reports another. selfupdate and burn -method usb refuse a
bootloader image built for another board than the one running.

Flags may appear before or after the subcommand.
`, version)
		global.PrintDefaults()
	}

	if err := global.Parse(os.Args[1:]); err != nil {
		os.Exit(1)
	}
	rest := global.Args()
	if len(rest) == 0 {
		global.Usage()
		os.Exit(1)
	}
	cmd := rest[0]
	rest = rest[1:]

	sub := flag.NewFlagSet(cmd, flag.ExitOnError)
	bind(sub)
	var (
		flagVerify = sub.Bool("verify", false, "read back and compare, independently of the device CRC")
		flagRun    = sub.Bool("run", false, "launch the application afterwards")
		bootFile   = sub.String("bootloader", "", "bootloader image (provision)")
		appFile    = sub.String("app", "", "application image (provision)")
		wchispPath = sub.String("wchisp", "", "path to wchisp (provision)")
		method     = sub.String("method", "usb", "burn: usb (through the installed bootloader) or isp (factory ISP)")
		packOut    = sub.String("out", "", "pack: the package (default: the image's name with .chg for .bin)")
		packTitle  = sub.String("title", "", "pack: what the menu shows (default: the sketch's name in capitals)")
		packAuthor = sub.String("author", "", "pack: the author")
		packVer    = sub.String("gameversion", "", "pack: a short version string, e.g. 1.2")
		device     = sub.String("device", "", "flash, pack: the board the image is built for (rev0); flash refuses any other board")
	)

	// Go's flag package stops parsing at the first non-flag argument, so a plain
	// sub.Parse would silently ignore the trailing flags in
	//     flash firmware.bin -run
	// which is exactly the form platform.txt uses. Parse in a loop instead,
	// peeling off one positional at a time, so flags work on either side.
	var positional []string
	for {
		if err := sub.Parse(rest); err != nil {
			os.Exit(1)
		}
		rest = sub.Args()
		if len(rest) == 0 {
			break
		}
		positional = append(positional, rest[0])
		rest = rest[1:]
	}

	switch cmd {
	case "probe":
		doProbe(o)
	case "info":
		doInfo(o)
	case "touch":
		doTouch(o)
	case "run":
		doRun(o)
	case "flash":
		if len(positional) < 1 {
			die("flash needs an image path")
		}
		doFlash(o, positional[0], *flagVerify, *flagRun, *device)
	case "provision":
		if *bootFile == "" {
			die("provision needs -bootloader")
		}
		if err := provision(*bootFile, *appFile, *wchispPath); err != nil {
			die("%v", err)
		}
	case "selfupdate":
		if len(positional) < 1 {
			die("selfupdate needs a bootloader image")
		}
		doSelfUpdate(o, positional[0], "")
	case "burn":
		if *bootFile == "" {
			die("burn needs -bootloader")
		}
		switch *method {
		case "usb":
			doSelfUpdate(o, *bootFile, *appFile)
		case "isp":
			if err := provision(*bootFile, *appFile, *wchispPath); err != nil {
				die("%v", err)
			}
		default:
			die("unknown method %q (usb or isp)", *method)
		}
	case "pack":
		if len(positional) < 1 {
			die("pack needs an image path")
		}
		doPack(o, positional[0], *packOut, *packTitle, *packAuthor, *packVer, *device)
	case "noop":
		// Arduino runs a separate chip-erase step before Burn Bootloader. The
		// erase itself is done inside provision(), so that EVERY provisioning
		// path gets it - including Upload Using Programmer, which Arduino does
		// not precede with an erase step at all.
		fmt.Println("erase: performed by the provisioning step itself")
	default:
		die("unknown command %q", cmd)
	}
}

func doPack(o *opts, image, out, title, author, ver, device string) {
	if device == "" {
		device = "rev0"
	}
	target, err := targetOf(device)
	if err != nil {
		die("%v", err)
	}
	data, err := os.ReadFile(image)
	if err != nil {
		die("cannot read %s: %v", image, err)
	}
	if out == "" {
		out = chgDefaultOutput(image)
	}
	if title == "" {
		title = chgDefaultTitle(image)
	}
	pkg, err := chgPackFor(data, title, author, ver, 0, target)
	if err != nil {
		die("%s: %v", image, err)
	}
	if err := os.WriteFile(out, pkg, 0o644); err != nil {
		die("cannot write %s: %v", out, err)
	}
	if !o.quiet {
		fmt.Printf("SD menu package: %s (%s, %d B)\n", out, title, len(pkg)-chgHeaderBytes)
	}
}

func doProbe(o *opts) {
	ports, err := findPorts()
	if err != nil {
		die("could not enumerate serial ports: %v", err)
	}
	if len(ports) == 0 {
		fmt.Printf("no CHGame device found (%s:%s)\n", chgameVID, chgamePID)
		os.Exit(1)
	}
	for _, name := range ports {
		// quickHello, never a bare hello(): writing to a sketch that does not
		// read Serial blocks forever, and probe has to survive being pointed at
		// one.
		h, err := quickHello(name, 2*time.Second, o.debug)
		if err != nil {
			// Silence is how an application identifies itself. Sketches do not
			// implement the bootloader protocol, and must not.
			fmt.Printf("%s: application (did not answer HELLO)\n", name)
			continue
		}
		fmt.Printf("%s: %s, protocol v%d, bootloader v%d, board %s\n",
			name, h.modeName(), h.ProtoVersion, h.BootVersion, boardName(h.Board))
	}
}

func doInfo(o *opts) {
	name := resolvePort(o)
	h, err := quickHello(name, o.timeout, o.debug)
	if err != nil {
		die("no response from %s: %v\n"+
			"(a running sketch does not answer this - use 'touch' first)", name, err)
	}
	fmt.Printf("port          : %s\n%s\n", name, h.describe())
}

func doTouch(o *opts) {
	name := resolvePort(o)
	if err := uploadTouch(name); err != nil {
		die("1200-baud touch failed: %v", err)
	}
	boot, err := waitForBootloader(o.timeout, name, o.debug)
	if err != nil {
		die("%v", err)
	}
	if boot == name {
		fmt.Printf("bootloader is up on %s\n", boot)
	} else {
		fmt.Printf("bootloader is up on %s (was %s)\n", boot, name)
	}
}

func doRun(o *opts) {
	name := resolvePort(o)
	h, err := quickHello(name, o.timeout, o.debug)
	if err != nil {
		die("no response from %s: %v", name, err)
	}
	if h.Mode != modeBootloader {
		die("%s is in %s mode, not the bootloader", name, h.modeName())
	}
	if h.AppState != 0 {
		die("bootloader reports the application image is not valid (%s); refusing to RUN",
			appStateNames[h.AppState])
	}
	c, err := openClient(name, o.timeout, o.debug)
	if err != nil {
		die("could not open %s: %v", name, err)
	}
	// The device vanishes as it launches, so a lost acknowledgement is expected.
	_, _ = c.request(cmdRun, nil, 2*time.Second)
	c.Close()
	fmt.Println("sent RUN - device should now be running the application")
}

// doFlash uploads a sketch. device, the board it was built for ("rev0"),
// refuses a board whose bootloader reports another; "" checks nothing.
func doFlash(o *opts, image string, verify, run bool, device string) {
	data, err := os.ReadFile(image)
	if err != nil {
		die("cannot read %s: %v", image, err)
	}
	var want uint32
	if device != "" {
		if want, err = targetOf(device); err != nil {
			die("%v", err)
		}
	}
	name := resolvePort(o)

	timeout := o.timeout
	if timeout < 10*time.Second {
		timeout = 10 * time.Second
	}

	bootPort, err := ensureBootloader(name, timeout, o.debug)
	if err != nil {
		die("could not put %s into the bootloader: %v", name, err)
	}
	if bootPort != name {
		fmt.Printf("note    : bootloader appeared on %s (was %s)\n", bootPort, name)
	}

	c, err := openClient(bootPort, timeout, o.debug)
	if err != nil {
		die("could not open %s: %v", bootPort, err)
	}
	defer c.Close()

	fmt.Printf("port    : %s\n", bootPort)
	fmt.Printf("image   : %s  (%d bytes)\n", image, len(data))

	h, err := c.hello()
	if err != nil {
		die("no response from %s: %v", bootPort, err)
	}
	fmt.Printf("region  : 0x%04X + %d bytes\n", h.AppStart, h.AppMaxSize)
	if device != "" && h.Board != want {
		die("the image is built for %s, but this board's bootloader is %s's: "+
			"build for that board (Tools > Board)", boardName(want), boardName(h.Board))
	}

	res, err := upload(c, data, progressBar(o.quiet), verify)
	if err != nil {
		die("upload failed: %v", err)
	}

	fmt.Printf("crc32   : 0x%08X\n", res.CRC32)
	fmt.Printf("erase   : %.2f s\n", res.EraseS)
	fmt.Printf("write   : %.2f s  (%.1f KiB/s, %d B chunks)\n", res.WriteS, res.KiBps, res.Chunk)
	if verify && res.Verified {
		fmt.Println("readback: MATCHES")
	} else if verify {
		fmt.Println("readback: not available (bootloader v3 has no READ; END checked the CRC in flash)")
	}
	fmt.Printf("total   : %.2f s  -- image accepted and marked valid\n", res.TotalS)

	if run {
		_, _ = c.request(cmdRun, nil, 2*time.Second)
		c.Close()
		fmt.Println("sent RUN")
		if back, err := waitForApplication(6*time.Second, o.debug); err == nil {
			fmt.Printf("running : application is up on %s\n", back)
		} else {
			fmt.Println("warning : the application did not re-enumerate within 6s")
		}
	}
}

// doSelfUpdate replaces the bootloader over USB and, given a sketch, uploads
// it afterwards (the update erases the one that was installed).
func doSelfUpdate(o *opts, bootPath, appPath string) {
	boot, err := os.ReadFile(bootPath)
	if err != nil {
		die("cannot read %s: %v", bootPath, err)
	}
	if err := checkBootImage(boot); err != nil {
		die("%s: %v", bootPath, err)
	}
	if appPath != "" {
		if _, err := os.Stat(appPath); err != nil {
			die("cannot read %s: %v", appPath, err)
		}
	}
	name := resolvePort(o)

	timeout := o.timeout
	if timeout < 10*time.Second {
		timeout = 10 * time.Second
	}
	bootPort, err := ensureBootloader(name, timeout, o.debug)
	if err != nil {
		die("could not reach the bootloader on %s: %v\n"+
			"If the board has no working bootloader, use the programmer "+
			"\"WCH factory ISP\" instead.", name, err)
	}
	c, err := openClient(bootPort, timeout, o.debug)
	if err != nil {
		die("could not open %s: %v", bootPort, err)
	}
	h, err := c.hello()
	if err != nil {
		c.Close()
		die("no response from %s: %v", bootPort, err)
	}
	fmt.Printf("port        : %s\n", bootPort)
	fmt.Printf("installed   : bootloader v%d\n", h.BootVersion)
	fmt.Printf("bootloader  : %s  (%d bytes)\n", filepath.Base(bootPath), len(boot))
	if h.AppStart != appStart {
		c.Close()
		die("this board reserves 0x%04X bytes for its bootloader, not 0x%04X: "+
			"the image does not belong on it", h.AppStart, appStart)
	}
	// Another board's bootloader would drive the wrong pins: the panel and
	// the card stay dark and only the factory ISP brings the board back.
	if b := bootImageBoard(boot); b != h.Board {
		c.Close()
		die("this bootloader is built for %s, but the board runs %s's: "+
			"pick that board's bootloader", boardName(b), boardName(h.Board))
	}
	fmt.Println("note        : the installed sketch is erased by the update. Keep the")
	fmt.Println("              board powered until it is done (a few seconds).")

	err = selfUpdate(c, boot, progressBar(o.quiet))
	c.Close()
	if err != nil {
		die("bootloader update failed: %v", err)
	}
	fmt.Println("promoting   : the board copies the new bootloader into place and resets")

	// The board is gone from USB while it copies and restarts.
	time.Sleep(1500 * time.Millisecond)
	newPort, err := waitForBootloader(20*time.Second, bootPort, o.debug)
	if err != nil {
		die("the board did not come back after the update: %v\n"+
			"Unplug it and plug it in again. If it still does not appear, recover it\n"+
			"with the programmer \"WCH factory ISP\" (hold BOOT across a power cycle).", err)
	}
	nh, err := quickHello(newPort, 2*time.Second, o.debug)
	if err != nil {
		die("the board came back on %s but did not identify itself: %v", newPort, err)
	}
	fmt.Printf("done        : bootloader v%d is running on %s\n", nh.BootVersion, newPort)

	if appPath == "" {
		fmt.Println("              no sketch is installed now: use Upload (or, with the SD")
		fmt.Println("              menu bootloader, start a game from the card)")
		return
	}
	o.port = newPort
	doFlash(o, appPath, false, true, "")
}
