#include "bank.h"

#include <stdlib.h>
#include <string.h>

static const char MAGIC[8] = {'T', 'I', 'L', 'B', 'A', 'N', 'K', 0};

static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void bank_default_palette(Bank *b)
{
    int i;

    for (i = 0; i < HW_COLORS; i++) {
        int r = (i % 4) * 85;
        int g = ((i / 4) % 4) * 85;
        int bch = ((i / 16) % 4) * 85;

        b->pal[i] = HW_RGB(r, g, bch);
    }
}

static void stamp(Bank *b, int id, const char *rows, uint8_t fg, uint8_t bg)
{
    int i;

    if ((unsigned)id >= (unsigned)HW_TILES)
        return;
    for (i = 0; i < BANK_TILE_PIX; i++)
        b->tiles[id][i] = (rows[i] == '#') ? fg : bg;
}

void bank_seed_charset(Bank *b)
{
    stamp(b, 0,
          "        "
          "        "
          "        "
          "        "
          "        "
          "        "
          "        "
          "        ",
          HW_PAL(1, 2, 3), HW_PAL(1, 2, 3));
    stamp(b, 1,
          "        "
          "  ####  "
          " ###### "
          "########"
          " ###### "
          "        "
          "        "
          "        ",
          HW_PAL(3, 3, 3), HW_PAL(1, 2, 3));
    stamp(b, 2,
          "        "
          "   #    "
          "  ###   "
          " #####  "
          "  ###   "
          "   #    "
          "        "
          "        ",
          HW_PAL(3, 3, 1), HW_PAL(0, 0, 1));
    stamp(b, 3,
          "# # # # "
          "########"
          "########"
          "########"
          "########"
          "########"
          "########"
          "########",
          HW_PAL(1, 3, 0), HW_PAL(0, 2, 0));
    stamp(b, 4,
          "########"
          "## # ###"
          "########"
          "### ## #"
          "########"
          "# ## ###"
          "########"
          "########",
          HW_PAL(2, 1, 0), HW_PAL(1, 0, 0));
    stamp(b, 5,
          "########"
          "#  ##  #"
          "#  ##  #"
          "########"
          "########"
          "#  ##  #"
          "#  ##  #"
          "########",
          HW_PAL(2, 2, 2), HW_PAL(1, 1, 1));
    stamp(b, 6,
          "  #  #  "
          "#  #  # "
          "  #  #  "
          "#  #  # "
          "  #  #  "
          "#  #  # "
          "  #  #  "
          "#  #  # ",
          HW_PAL(1, 2, 3), HW_PAL(0, 1, 3));
    stamp(b, 7,
          "########"
          "#      #"
          "########"
          "   ##   "
          "########"
          "#      #"
          "########"
          "########",
          HW_PAL(3, 1, 0), HW_PAL(2, 0, 0));
}

void bank_init(Bank *b)
{
    memset(b, 0, sizeof(*b));
    bank_default_palette(b);
    b->map_w = 64;
    b->map_h = 32;
    b->map = calloc((size_t)b->map_w * (size_t)b->map_h, 1);
    bank_seed_charset(b);
    bank_guess_solid(b);
    b->solid[1] = 0; /* cloud */
    b->solid[2] = 0; /* star */
    if (b->map) {
        int x, y, ground = b->map_h - 12;

        for (y = 0; y < b->map_h; y++) {
            for (x = 0; x < b->map_w; x++) {
                uint8_t t = 0;
                if (y > ground + 3)
                    t = 5;
                else if (y > ground + 1)
                    t = 4;
                else if (y == ground + 1)
                    t = 3;
                b->map[y * b->map_w + x] = t;
            }
        }
    }
}

void bank_free(Bank *b)
{
    if (!b)
        return;
    free(b->map);
    b->map = NULL;
    b->map_w = b->map_h = 0;
}

int bank_copy(Bank *dst, const Bank *src)
{
    uint8_t *m;
    size_t n;

    if (!dst || !src || !src->map)
        return 0;
    n = (size_t)src->map_w * (size_t)src->map_h;
    m = malloc(n);
    if (!m)
        return 0;
    memcpy(m, src->map, n);
    free(dst->map);
    memcpy(dst->pal, src->pal, sizeof(src->pal));
    memcpy(dst->tiles, src->tiles, sizeof(src->tiles));
    memcpy(dst->sprites, src->sprites, sizeof(src->sprites));
    memcpy(dst->solid, src->solid, sizeof(src->solid));
    dst->map_w = src->map_w;
    dst->map_h = src->map_h;
    dst->map = m;
    return 1;
}

int bank_resize_map(Bank *b, int w, int h)
{
    uint8_t *n;
    int x, y, nw, nh;

    nw = clampi(w, BANK_MAP_MIN, BANK_MAP_MAX);
    nh = clampi(h, BANK_MAP_MIN, BANK_MAP_MAX);
    n = calloc((size_t)nw * (size_t)nh, 1);
    if (!n)
        return 0;
    if (b->map) {
        int cw = nw < b->map_w ? nw : b->map_w;
        int ch = nh < b->map_h ? nh : b->map_h;

        for (y = 0; y < ch; y++)
            for (x = 0; x < cw; x++)
                n[y * nw + x] = b->map[y * b->map_w + x];
        free(b->map);
    }
    b->map = n;
    b->map_w = nw;
    b->map_h = nh;
    return 1;
}

void bank_guess_solid(Bank *b)
{
    int i;

    if (!b)
        return;
    b->solid[0] = 0;
    for (i = 1; i < HW_TILES; i++)
        b->solid[i] = 1;
}

int bank_is_solid(const Bank *b, int id)
{
    if (!b || (unsigned)id >= (unsigned)HW_TILES)
        return 0;
    return b->solid[id] ? 1 : 0;
}

int bank_blocked(const Bank *b, int tx, int ty)
{
    return bank_is_solid(b, bank_at(b, tx, ty));
}

int bank_mask_hit(const Bank *b, int wx, int wy, int pat)
{
    const uint8_t *pix;
    int sx, sy, mw, mh;

    if (!b || !b->map || (unsigned)pat >= (unsigned)HW_SP_PATS)
        return 0;
    pix = b->sprites[pat];
    mw = b->map_w * HW_TILE;
    mh = b->map_h * HW_TILE;
    for (sy = 0; sy < HW_SP_H; sy++) {
        int py = wy + sy;

        for (sx = 0; sx < HW_SP_W; sx++) {
            int px;

            if (!pix[sy * HW_SP_W + sx])
                continue;
            px = wx + sx;
            if (px < 0 || px >= mw || py >= mh)
                return 1;
            if (py < 0)
                continue;
            if (bank_blocked(b, px >> 3, py >> 3))
                return 1;
        }
    }
    return 0;
}

int bank_mask_bounds(const Bank *b, int pat, int *x0, int *y0, int *x1, int *y1)
{
    const uint8_t *pix;
    int sx, sy, found = 0;
    int ax = HW_SP_W, ay = HW_SP_H, bx = -1, by = -1;

    if (!b || (unsigned)pat >= (unsigned)HW_SP_PATS)
        return 0;
    pix = b->sprites[pat];
    for (sy = 0; sy < HW_SP_H; sy++) {
        for (sx = 0; sx < HW_SP_W; sx++) {
            if (!pix[sy * HW_SP_W + sx])
                continue;
            if (sx < ax)
                ax = sx;
            if (sy < ay)
                ay = sy;
            if (sx > bx)
                bx = sx;
            if (sy > by)
                by = sy;
            found = 1;
        }
    }
    if (!found)
        return 0;
    if (x0)
        *x0 = ax;
    if (y0)
        *y0 = ay;
    if (x1)
        *x1 = bx;
    if (y1)
        *y1 = by;
    return 1;
}

int bank_at(const Bank *b, int tx, int ty)
{
    if (!b || !b->map || tx < 0 || ty < 0 || tx >= b->map_w || ty >= b->map_h)
        return 0;
    return b->map[ty * b->map_w + tx];
}

void bank_put(Bank *b, int tx, int ty, int id)
{
    if (!b || !b->map || tx < 0 || ty < 0 || tx >= b->map_w || ty >= b->map_h)
        return;
    if ((unsigned)id >= (unsigned)HW_TILES)
        id = 0;
    b->map[ty * b->map_w + tx] = (uint8_t)id;
}

static int wr16(FILE *f, uint16_t v)
{
    return fputc(v & 255, f) != EOF && fputc((v >> 8) & 255, f) != EOF;
}

static int wr32(FILE *f, uint32_t v)
{
    return wr16(f, (uint16_t)(v & 0xffffu)) && wr16(f, (uint16_t)(v >> 16));
}

static int rd16(FILE *f, uint16_t *out)
{
    int a = fgetc(f), b = fgetc(f);

    if (a == EOF || b == EOF)
        return 0;
    *out = (uint16_t)(a | (b << 8));
    return 1;
}

static int rd32(FILE *f, uint32_t *out)
{
    uint16_t lo, hi;

    if (!rd16(f, &lo) || !rd16(f, &hi))
        return 0;
    *out = (uint32_t)lo | ((uint32_t)hi << 16);
    return 1;
}

int bank_load(const char *path, Bank *b)
{
    FILE *f;
    char mag[8];
    uint16_t ver, mw, mh, res;
    Bank tmp;
    int i;
    size_t n;

    if (!path || !b)
        return 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    memset(&tmp, 0, sizeof(tmp));
    if (fread(mag, 1, 8, f) != 8 || memcmp(mag, MAGIC, 8) != 0)
        goto fail;
    if (!rd16(f, &ver) || (ver != 1 && ver != 2))
        goto fail;
    if (!rd16(f, &mw) || !rd16(f, &mh) || !rd16(f, &res))
        goto fail;
    if (mw < BANK_MAP_MIN || mh < BANK_MAP_MIN || mw > BANK_MAP_MAX || mh > BANK_MAP_MAX)
        goto fail;
    for (i = 0; i < HW_COLORS; i++)
        if (!rd32(f, &tmp.pal[i]))
            goto fail;
    if (fread(tmp.tiles, 1, sizeof(tmp.tiles), f) != sizeof(tmp.tiles))
        goto fail;
    if (fread(tmp.sprites, 1, sizeof(tmp.sprites), f) != sizeof(tmp.sprites))
        goto fail;
    n = (size_t)mw * (size_t)mh;
    tmp.map = malloc(n);
    if (!tmp.map)
        goto fail;
    if (fread(tmp.map, 1, n, f) != n)
        goto fail;
    tmp.map_w = mw;
    tmp.map_h = mh;
    if (ver >= 2) {
        if (fread(tmp.solid, 1, sizeof(tmp.solid), f) != sizeof(tmp.solid))
            goto fail;
    } else {
        bank_guess_solid(&tmp);
    }
    fclose(f);
    bank_free(b);
    *b = tmp;
    return 1;
fail:
    free(tmp.map);
    fclose(f);
    return 0;
}

int bank_save(const char *path, const Bank *b)
{
    FILE *f;
    int i;
    size_t n;

    if (!path || !b || !b->map)
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    if (fwrite(MAGIC, 1, 8, f) != 8)
        goto fail;
    if (!wr16(f, BANK_VERSION) || !wr16(f, (uint16_t)b->map_w) || !wr16(f, (uint16_t)b->map_h) ||
        !wr16(f, 0))
        goto fail;
    for (i = 0; i < HW_COLORS; i++)
        if (!wr32(f, b->pal[i]))
            goto fail;
    if (fwrite(b->tiles, 1, sizeof(b->tiles), f) != sizeof(b->tiles))
        goto fail;
    if (fwrite(b->sprites, 1, sizeof(b->sprites), f) != sizeof(b->sprites))
        goto fail;
    n = (size_t)b->map_w * (size_t)b->map_h;
    if (fwrite(b->map, 1, n, f) != n)
        goto fail;
    if (fwrite(b->solid, 1, sizeof(b->solid), f) != sizeof(b->solid))
        goto fail;
    fclose(f);
    return 1;
fail:
    fclose(f);
    return 0;
}

int bank_from_embed(Bank *b, const uint32_t *pal, const uint8_t *tiles, const uint8_t *sprites,
                    int map_w, int map_h, const uint8_t *map)
{
    size_t n;

    if (!b || !pal || !tiles || !sprites || !map)
        return 0;
    if (map_w < BANK_MAP_MIN || map_h < BANK_MAP_MIN || map_w > BANK_MAP_MAX || map_h > BANK_MAP_MAX)
        return 0;
    n = (size_t)map_w * (size_t)map_h;
    bank_free(b);
    memset(b, 0, sizeof(*b));
    memcpy(b->pal, pal, sizeof(b->pal));
    memcpy(b->tiles, tiles, sizeof(b->tiles));
    memcpy(b->sprites, sprites, sizeof(b->sprites));
    b->map_w = map_w;
    b->map_h = map_h;
    b->map = malloc(n);
    if (!b->map)
        return 0;
    memcpy(b->map, map, n);
    bank_guess_solid(b);
    return 1;
}

static void dump_bytes(FILE *f, const uint8_t *p, size_t n, int width)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (i % (size_t)width == 0)
            fputs("\n    ", f);
        fprintf(f, "%u%s", p[i], i + 1 < n ? "," : "");
    }
}

int bank_export_c(const char *path, const Bank *b)
{
    FILE *f;
    int i;
    size_t n;

    if (!path || !b || !b->map)
        return 0;
    f = fopen(path, "w");
    if (!f)
        return 0;
    n = (size_t)b->map_w * (size_t)b->map_h;
    fprintf(f, "/* Generated by xcb-tile-edit. Include after bank.h, then:\n");
    fprintf(f, " *   Bank b = {0}; BANK_LOAD_EMBED(&b); bank_upload(&b); bank_paint_view(&b);\n");
    fprintf(f, " */\n#ifndef BANK_EMBED_H\n#define BANK_EMBED_H\n\n");
    fprintf(f, "#include <stdint.h>\n#include <string.h>\n\n");
    fprintf(f, "#define BANK_EMBED_MAP_W %d\n#define BANK_EMBED_MAP_H %d\n\n", b->map_w, b->map_h);
    fprintf(f, "static const uint32_t bank_embed_pal[%d] = {", HW_COLORS);
    for (i = 0; i < HW_COLORS; i++) {
        if (i % 8 == 0)
            fputs("\n    ", f);
        fprintf(f, "0x%06Xu%s", b->pal[i] & 0xffffffu, i + 1 < HW_COLORS ? "," : "");
    }
    fprintf(f, "\n};\n");
    fprintf(f, "static const uint8_t bank_embed_tiles[%d][%d] = {", HW_TILES, BANK_TILE_PIX);
    for (i = 0; i < HW_TILES; i++) {
        int j;

        fprintf(f, "\n  {");
        for (j = 0; j < BANK_TILE_PIX; j++)
            fprintf(f, "%u%s", b->tiles[i][j], j + 1 < BANK_TILE_PIX ? "," : "");
        fprintf(f, "}%s", i + 1 < HW_TILES ? "," : "");
    }
    fprintf(f, "\n};\n");
    fprintf(f, "static const uint8_t bank_embed_sprites[%d][%d] = {", HW_SP_PATS, BANK_SP_PIX);
    for (i = 0; i < HW_SP_PATS; i++) {
        fprintf(f, "\n  {");
        dump_bytes(f, b->sprites[i], BANK_SP_PIX, 32);
        fprintf(f, "}%s", i + 1 < HW_SP_PATS ? "," : "");
    }
    fprintf(f, "\n};\n");
    fprintf(f, "static const uint8_t bank_embed_map[] = {");
    dump_bytes(f, b->map, n, b->map_w);
    fprintf(f, "\n};\n");
    fprintf(f, "static const uint8_t bank_embed_solid[%d] = {", HW_TILES);
    dump_bytes(f, b->solid, HW_TILES, 16);
    fprintf(f, "\n};\n\n");
    fprintf(f, "#define BANK_LOAD_EMBED(b) \\\n");
    fprintf(f, "    (bank_from_embed((b), bank_embed_pal, (const uint8_t *)bank_embed_tiles, \\\n");
    fprintf(f, "                     (const uint8_t *)bank_embed_sprites, BANK_EMBED_MAP_W, \\\n");
    fprintf(f, "                     BANK_EMBED_MAP_H, bank_embed_map) && \\\n");
    fprintf(f, "     (memcpy((b)->solid, bank_embed_solid, sizeof((b)->solid)), 1))\n\n");
    fprintf(f, "#endif\n");
    fclose(f);
    return 1;
}
