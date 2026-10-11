/*
 * The card as both menus see it (card.h; spec/card.md, shared/chgame_card.h):
 * the folder being shown, its entries and the keys. The game table lives in
 * RAM; the folder is read again from the card whenever it changes.
 */
#include "card.h"
#include "sd.h"
#include "fat.h"
#include "chg.h"
#include "install.h"
#include "appmeta.h"
#include "hal.h"
#include "sys.h"
#include "chgame_card.h"

game_t games[MENU_MAX_GAMES];
uint32_t ngames, sel, depth, lit;
uint32_t here;
uint8_t buf[512] __attribute__((aligned(4)));
static file_t idx;                      /* the folder's MENU.IDX */
#if MENU_UI == MENU_UI_LIST
uint32_t top;
file_t bg;
#else
file_t sys;
uint32_t stray;
#endif

_Static_assert(sizeof(game_t) == 32, "keep game_t at 32 bytes");

/* The menu's own colours when there is no picture (black behind; the rest
   by shared/chgame_card.h's roles; colour 15 the picture's magenta, which
   the static visual menu shows as painted; the static list menu draws
   colour 15 in the text colour instead: menu.c's background()). */
const uint16_t pal0[16] = {
    [CARD_C_TEXT] = RGB565(255, 244, 214), [CARD_C_DIM] = RGB565(128, 128, 128),
    [CARD_C_MARK] = RGB565(214, 32, 32), [CARD_C_RAINBOW] = RGB565(255, 0, 255),
};

/* ---- the card --------------------------------------------------------------- */

__attribute__((noinline)) int same(const uint8_t *a, const char *b)
{
    for (uint32_t i = 0; i < 11; i++) if (a[i] != (uint8_t)b[i]) return 0;
    return 1;
}

int find_games(const uint8_t *d, void *ctx)
{
    (void)ctx;
    if (!same(d, "GAMES      ") || (d[11] & (FAT_ATTR_DIR | FAT_ATTR_LABEL)) != FAT_ATTR_DIR) return 0;
    here = fat_entry_cluster(d);
    return 1;
}

static int add_entry(const uint8_t *d, void *ctx)
{
    game_t *g = &games[ngames];
    file_t *f;
    (void)ctx;
    /* labels, hidden and system files, "." and "..", macOS "._NAME" forks */
    if (d[11] & (FAT_ATTR_LABEL | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM) || d[0] == '.' || d[0] == '_') return 0;
    if (!(d[11] & FAT_ATTR_DIR)) {
#if MENU_UI == MENU_UI_LIST
        f = same(d, "MENU    IDX") ? &idx : same(d, "MENU    BG ") ? &bg : 0;
#else
        f = same(d, "MENU    IDX") ? &idx : !depth && same(d, "SYSTEM  PIC") ? &sys : 0;
#endif
        if (f) {
            f->clus = fat_entry_cluster(d);
            f->size = fat_entry_size(d);
            return 0;
        }
        if (d[8] != 'C' || d[9] != 'H' || d[10] != 'G') return 0;
    }
    if (ngames == MENU_MAX_GAMES) return 0;     /* (still looking for MENU.*) */
    g->clus = fat_entry_cluster(d);
    g->size = fat_entry_size(d);
    g->flags = d[11] & FAT_ATTR_DIR ? G_DIR : 0;
    g->key = 0xFF;
#if MENU_UI == MENU_UI_VISUAL
    g->pic = 0;
#endif
    for (uint32_t i = 0; i <= TITLE_COLS; i++) g->title[i] = i < 11 ? (char)d[i] : ' ';
    ngames++;
    return 0;
}

/* Up to TITLE_COLS characters of s into a title: a NUL ends it, lower case is
   folded (the font has capitals only), anything else unprintable is '?'. */
static void set_title(game_t *g, const uint8_t *s)
{
    for (uint32_t k = 0, end = 0; k < TITLE_COLS; k++) {
        char ch = end ? 0 : (char)s[k];
        if (!ch) end = 1;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        g->title[k] = end ? ' ' : (ch < 32 || ch > '_' ? '?' : ch);
    }
}

static void read_index(void)
{
    fat_stream_t s;
    uint32_t lba, k = 0;
    fat_open(&s, idx.clus, idx.size);
    while ((lba = fat_next_lba(&s, buf)) && !sd_read(lba, buf))
        for (const uint8_t *e = buf; e < buf + sizeof buf; e += CARD_IDX_RECORD, k++) {
            if (!k) {
                if (w32(e) != CARD_IDX_MAGIC) return;
                continue;
            }
            for (uint32_t i = 0; i < ngames; i++) {
                game_t *g = &games[i];
                if (g->key != 0xFF || !same(e, g->title)) continue;
                g->key = (uint8_t)(k < 0xFF ? k : 0xFE);
                if (e[CARD_IDX_OFF_FLAGS] & CARD_IDX_LAUNCH) g->flags |= G_LAUNCH;
#if MENU_UI == MENU_UI_LIST                 /* (the visual menu shows no titles; an indexed entry's never sorts) */
                if (g->flags & G_DIR && e[CARD_IDX_OFF_TITLE]) set_title(g, e + CARD_IDX_OFF_TITLE);
#endif
            }
        }
}

/* Struct assignment would call newlib's memcpy (a byte loop in flash, and
   178 B of it): entries are 8 words, copied as words. */
static void copy(game_t *d, const game_t *s)
{
    for (uint32_t i = 0; i < sizeof(game_t) / 4; i++)
        ((uint32_t *)(void *)d)[i] = ((const uint32_t *)(const void *)s)[i];
}

static int less(const game_t *a, const game_t *b)
{
    if (a->key != b->key) return a->key < b->key;
    for (uint32_t i = 0; i < TITLE_COLS; i++)
        if (a->title[i] != b->title[i]) return a->title[i] < b->title[i];
    return 0;
}

uint32_t scan(int app)
{
    const chgame_meta_t *m = appmeta();
    uint32_t i, installed = 0;

    ngames = idx.clus = 0;
    fat_dir(here, buf, add_entry, 0);
    if (idx.clus) read_index();
    for (i = 0; i < ngames; i++) {
        game_t *g = &games[i];
        fat_stream_t s;
        uint32_t lba, n, crc;
        int rc;
        if (g->flags & G_DIR) continue;
        fat_open(&s, g->clus, g->size);
        lba = fat_next_lba(&s, buf);
        rc = lba && !sd_read(lba, buf) ? chg_check(buf, g->size, &n, &crc) : -1;
        if (rc) {
            g->flags |= G_BAD;
            g->err = (uint8_t)(rc < 0 ? INST_E_READ : INST_E_PKG);
            continue;                       /* (its 8.3 name stands in: "BADFILE CHG") */
        }
        set_title(g, buf + CHG_OFF_TITLE);
#if MENU_UI == MENU_UI_VISUAL
        /* Its picture: CARD_BG_BYTES at a sector of its own, under 128 KiB (spec/chg.md) */
        lba = w32(buf + CHG_OFF_IMAGE);
        g->pic = (uint8_t)(w32(buf + CHG_OFF_IMAGE + 4) == CARD_BG_BYTES && !(lba & 511u) && lba < 0x20000u ? lba >> 9 : 0);
#endif
        if (app == APP_VALID && m->length == n && m->crc32 == crc) {
            g->flags |= G_INSTALLED;
            installed = 1;
        }
    }
    /* insertion sort: MENU.IDX's order, then by title */
    for (i = 1; i < ngames; i++) {
        game_t t;
        uint32_t j = i;
        copy(&t, &games[i]);
        for (; j && less(&t, &games[j - 1]); j--) copy(&games[j], &games[j - 1]);
        copy(&games[j], &t);
    }
#if MENU_UI == MENU_UI_LIST
    if (ngames && !depth && app == APP_VALID && !installed && ngames < MENU_MAX_GAMES) {
        for (i = ngames; i; i--) copy(&games[i], &games[i - 1]);
        games[0].flags = G_INSTALLED;      /* (its clus is never used) */
        set_title(&games[0], (const uint8_t *)"INSTALLED PROGRAM");
        ngames++;
    }
#else
    (void)installed;
    if (stray && !depth && ngames < MENU_MAX_GAMES) {
        for (i = ngames; i; i--) copy(&games[i], &games[i - 1]);
        games[0].flags = G_INSTALLED;       /* clus 0: SYSTEM.PIC's picture of it */
        games[0].clus = 0;
        ngames++;
    }
#endif
    for (sel = 0; sel < ngames && !(games[sel].flags & G_INSTALLED); sel++) { }
    if (sel == ngames) sel = 0;
#if MENU_UI == MENU_UI_LIST
    top = 0;                                /* (draw_list scrolls to sel) */
#endif
    return ngames;
}

/* ---- the panel ------------------------------------------------------------------- */

void flush(uint32_t y0, uint32_t y1)
{
    if (lit) lcd_flush(y0, y1);             /* before lcd_on(), the picture waits for it */
}

void light(void)
{
    if (lit) return;
    lcd_wake();
    lcd_on();
    lit = 1;
}

/* ---- keys ------------------------------------------------------------------------- */

static uint32_t k_prev = BTN_ALL, k_last, k_rep;

#if MENU_UI == MENU_UI_LIST
#define K_REPEAT (BTN_UP | BTN_DOWN)
#else
#define K_REPEAT (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)
#endif

/* Newly pressed keys, sampled every 15 ms (debounce), with UP/DOWN (visual:
   and LEFT/RIGHT) repeating after 400 ms every 80 ms. Keys held when the menu
   starts (START at power-on) count only after a release: k_prev starts with
   every key down. */
uint32_t keys(void)
{
    uint32_t t = sys_ticks(), now, out;
    if (t - k_last < 15u * SYS_TICKS_PER_MS) return 0;
    k_last = t;
    now = hal_buttons();
    out = now & ~k_prev;
    if (out) k_rep = t + 400u * SYS_TICKS_PER_MS;
    else if ((now & K_REPEAT) && (int32_t)(t - k_rep) >= 0) {
        out = now & K_REPEAT;
        k_rep = t + 80u * SYS_TICKS_PER_MS;
    }
    k_prev = now;
    return out;
}

void release(void)
{
    uint32_t t = sys_ticks();
    while (sys_ticks() - t < 30u * SYS_TICKS_PER_MS)
        if (hal_buttons()) t = sys_ticks();
}

void wait_key(void)
{
    while (!(keys() & (BTN_A | BTN_B | BTN_START))) { }
}
