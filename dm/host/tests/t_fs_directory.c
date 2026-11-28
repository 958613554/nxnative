// Real folder-backed OpenDirectory: allocation failure must return an error,
// leave the output untouched, and close the DIR already opened by the handler.
// A folder whose reading fails partway (readdir's NULL with EIO) is a failure everywhere, never a short listing taken for the whole.
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

static void *directory_calloc(size_t count, size_t size);
static DIR *directory_opendir(const char *path);
static int directory_closedir(DIR *d);
static struct dirent *directory_readdir(DIR *d);
#define calloc directory_calloc
#define opendir directory_opendir
#define closedir directory_closedir
#define readdir directory_readdir
#include "../hle_fs.c"
#undef readdir
#undef closedir
#undef opendir
#undef calloc

struct dm_flags g_flag;
uint64_t g_t0_ns;
uint64_t now_ns(void) { return 0; }
void dm_guest_write_prepare(void *a, size_t n) { (void)a; (void)n; }

static int fail_handle, failed_allocations, opened_fd = -1;
static int calloc_calls;
static size_t calloc_bytes;
static void *directory_calloc(size_t count, size_t size) {
    calloc_calls++;
    calloc_bytes += count * size;
    if (fail_handle && count == 1 && size == sizeof(dirh_t)) {
        failed_allocations++;
        errno = ENOMEM;
        return NULL;
    }
    return calloc(count, size);
}
// One injected read failure: the next folder opened at fail_dir (trailing slashes aside) gives fail_after entries, then NULL with EIO; then injection is off
static char fail_dir[1100]; static DIR *fail_d; static int fail_after, read_failures;
static int same_dir(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    while (la > 1 && a[la - 1] == '/') la--;
    while (lb > 1 && b[lb - 1] == '/') lb--;
    return la == lb && !strncmp(a, b, la);
}
static void fail_reading(const char *dir, int after) { strlcpy(fail_dir, dir, sizeof fail_dir); fail_after = after; fail_d = NULL; }
static DIR *directory_opendir(const char *path) {
    DIR *d = opendir(path);
    if (d) opened_fd = dirfd(d);
    if (d && fail_dir[0] && same_dir(path, fail_dir)) fail_d = d;
    return d;
}
static struct dirent *directory_readdir(DIR *d) {
    if (d && d == fail_d) {
        struct dirent *e;
        while ((e = readdir(d)) && (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))) {}
        if (e && fail_after-- > 0) return e;
        fail_dir[0] = 0; fail_d = NULL; read_failures++; errno = EIO; return NULL;
    }
    return readdir(d);
}

static int directory_closedir(DIR *d) {
    if (d == fail_d) fail_d = NULL;
    int r = closedir(d);
    // Successful cleanup may change errno; it must not hide the allocation error.
    if (fail_handle) errno = ENOENT;
    return r;
}

static int checks, fails;
static void drop_indexes(void) {   // the read-only folders' name indexes, built again at the next lookup
    for (dir_index **slot = g_dir_index; slot < g_dir_index + DIR_INDEX_SLOTS; slot++)
        while (*slot) { dir_index *x = *slot; *slot = x->next; free(x->e); free(x->pool); free(x->path); free(x); }
}
#define CHECK(c, text) do { checks++; int ok = !!(c); fails += !ok; printf("%s %s\n", ok ? "PASS" : "FAIL", text); } while (0)

int main(void) {
    char root[1024], file[1100];
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/t_fs_directory.XXXXXX", tmp ? tmp : "/private/tmp");
    if (!mkdtemp(root)) { perror("mkdtemp"); return 2; }
    snprintf(file, sizeof file, "%s/entry", root);
    FILE *f = fopen(file, "w");
    if (!f) { perror("fopen"); rmdir(root); return 2; }
    fputs("hello", f); fclose(f);
    CHECK(mount("test", root, 0) == RES_OK, "mount the synthetic folder");

    const uint64_t initial[] = {0, 0x1234};
    for (size_t i = 0; i < sizeof initial / sizeof initial[0]; i++) {
        uint64_t h = initial[i];
        opened_fd = -1; failed_allocations = 0; fail_handle = 1;
        Result r = h_fs_OpenDirectory(&h, "test:/", DIRMODE_DIR | DIRMODE_FILE);
        int error = errno;
        fail_handle = 0;
        CHECK(failed_allocations == 1, "inject failure into the directory-handle allocation");
        CHECK(r == RES_FAIL, "failed allocation returns the filesystem resource failure");
        CHECK(error == ENOMEM, "allocation error survives directory cleanup");
        CHECK(h == initial[i], "failed open leaves the caller's handle untouched");
        CHECK(opened_fd >= 0, "the handler opened a real directory before allocation failed");
        errno = 0;
        CHECK(fcntl(opened_fd, F_GETFD) == -1 && errno == EBADF, "failed open closes the real directory descriptor");
    }
    h_fs_CloseDirectory(0);

    uint64_t h = 0;
    Result r = h_fs_OpenDirectory(&h, "test:/", DIRMODE_DIR | DIRMODE_FILE);
    int fd = opened_fd;
    CHECK(r == RES_OK && h != 0, "a later successful open publishes a handle");
    if (r == RES_OK && h) {
        CHECK(fcntl(fd, F_GETFD) >= 0, "successful open keeps its directory descriptor alive");
        int64_t count = -1;
        CHECK(h_fs_DirCount(&count, h) == RES_OK && count == 1, "the successful handle counts the one synthetic file");
        DirectoryEntry entry[2] = {0};
        CHECK(h_fs_ReadDirectory(&count, entry, h, 2) == RES_OK && count == 1 &&
              !strcmp(entry[0].name, "entry") && entry[0].type == ENT_FILE && entry[0].size == 5,
              "the successful handle reads the file name, type and size");
        h_fs_CloseDirectory(h);
        errno = 0;
        CHECK(fcntl(fd, F_GETFD) == -1 && errno == EBADF, "CloseDirectory releases the successful handle's descriptor");
    }

    // Reading fails partway: three names in a read-only folder (name index), a writable folder with a subfolder (path walk, listing, count, clean) and an overlay over a base
    char ro[1100], w[1100], sub[1200], ov[1100], q[1300];
    snprintf(ro, sizeof ro, "%s.ro", root); snprintf(w, sizeof w, "%s.w", root); snprintf(sub, sizeof sub, "%s/sub", w); snprintf(ov, sizeof ov, "%s.ov", root);
    mkdir(ro, 0755); mkdir(w, 0755); mkdir(sub, 0755); mkdir(ov, 0755);
    const char *three[] = {"a", "b", "c"};
    for (int i = 0; i < 3; i++) {
        snprintf(q, sizeof q, "%s/%s", ro, three[i]); fclose(fopen(q, "w"));
        snprintf(q, sizeof q, "%s/%s", sub, three[i]); fclose(fopen(q, "w"));
        snprintf(q, sizeof q, "%s/o%s", ov, three[i]); fclose(fopen(q, "w"));
    }
    CHECK(mount("ro", ro, 1) == RES_OK && mount("w", w, 0) == RES_OK && mount("ov", root, 1) == RES_OK, "mount a read-only folder, a writable one and one with an overlay");
    strlcpy(g_mnt[g_nmnt - 1].overlay, ov, sizeof g_mnt[0].overlay);

    for (int after = 0; after <= 1; after++) {
        fail_reading(ro, after); int opened = 0, before = read_failures;
        for (int i = 0; i < 3; i++) {
            uint64_t fh = 0; char path[16]; snprintf(path, sizeof path, "ro:/%s", three[i]);
            if (h_fs_OpenFile(&fh, path, MODE_READ) == RES_OK) { opened++; h_fs_CloseFile(fh); }
        }
        CHECK(read_failures == before + 1 && opened == 3, after ? "a name index whose folder fails after one entry is not kept: every name still opens" : "a name index whose folder fails at once is not kept: every name still opens");
        drop_indexes();   // the next round builds it again
    }

    uint64_t fh = 0x1234; fail_reading(sub, 1);
    Result rr = h_fs_OpenFile(&fh, "w:/sub/zz", MODE_READ);
    CHECK(read_failures && rr != RES_OK && rr != RES_PATH_NOT_FOUND && fh == 0x1234, "a path walk whose folder cannot be read fails, not with not found");

    fail_reading(sub, 1); int64_t count = -1; DirectoryEntry ents[8];
    uint64_t dh = 0;
    CHECK(h_fs_OpenDirectory(&dh, "w:/sub", DIRMODE_FILE) == RES_OK && h_fs_ReadDirectory(&count, ents, dh, 8) != RES_OK, "a listing that fails after one entry fails");
    if (dh) h_fs_CloseDirectory(dh);
    dh = 0; CHECK(h_fs_OpenDirectory(&dh, "w:/sub", DIRMODE_FILE) == RES_OK, "open the folder again");
    fail_reading(sub, 1); CHECK(dh && h_fs_DirCount(&count, dh) != RES_OK, "a count that fails after one entry fails");
    CHECK(dh && h_fs_DirCount(&count, dh) == RES_OK && count == 3 && h_fs_ReadDirectory(&count, ents, dh, 8) == RES_OK && count == 3, "then the folder counts and lists its three files");
    if (dh) h_fs_CloseDirectory(dh);

    dh = 0x1234; fail_reading(ov, 1);
    CHECK(h_fs_OpenDirectory(&dh, "ov:/", DIRMODE_FILE) != RES_OK && dh == 0x1234, "an overlay folder that cannot be read fails the open and leaves the handle");
    dh = 0; count = 0;
    CHECK(h_fs_OpenDirectory(&dh, "ov:/", DIRMODE_FILE) == RES_OK && h_fs_ReadDirectory(&count, ents, dh, 8) == RES_OK && count == 4, "then the overlay lists its three files and the base's one");
    if (dh) h_fs_CloseDirectory(dh);

    // A name in several layers is listed once, as the first layer has it; the layers come in their order (the last overlay folder first), and names differing in case are two
    char ov2[1100]; snprintf(ov2, sizeof ov2, "%s.ov2", root); mkdir(ov2, 0755);
    snprintf(q, sizeof q, "%s/entry", ov); FILE *g = fopen(q, "w"); fputs("overlay!", g); fclose(g);
    snprintf(q, sizeof q, "%s/ENTRY", ov2); fclose(fopen(q, "w"));
    snprintf(q, sizeof q, "%s/oa", ov2); fclose(fopen(q, "w"));
    snprintf(g_mnt[g_nmnt - 1].overlay, sizeof g_mnt[0].overlay, "%s:%s", ov, ov2);
    drop_indexes();   // a read-only folder's index is kept for the run: the folder just changed, which a game's never does
    dh = 0; count = 0; int once = 0, upper = 0, big = 0;
    CHECK(h_fs_OpenDirectory(&dh, "ov:/", DIRMODE_FILE) == RES_OK && h_fs_ReadDirectory(&count, ents, dh, 8) == RES_OK && count == 5,
          "the layers list oa, ob, oc, entry and ENTRY, each once");
    for (int64_t i = 0; i < count; i++) { once += !strcmp(ents[i].name, "entry"); upper += !strcmp(ents[i].name, "ENTRY"); big += !strcmp(ents[i].name, "entry") && ents[i].size == 8; }
    CHECK(once == 1 && upper == 1 && big == 1, "entry is the overlay's (8 bytes, not the base's 5), and ENTRY is a name of its own");
    CHECK(count == 5 && (!strcmp(ents[0].name, "ENTRY") || !strcmp(ents[0].name, "oa")) && (!strcmp(ents[1].name, "ENTRY") || !strcmp(ents[1].name, "oa")), "the last overlay folder's names come first");
    if (dh) h_fs_CloseDirectory(dh);
    snprintf(q, sizeof q, "%s/entry", ov); unlink(q); snprintf(q, sizeof q, "%s/ENTRY", ov2); unlink(q); snprintf(q, sizeof q, "%s/oa", ov2); unlink(q); rmdir(ov2);

    // RomFS image directory: OpenDirectory allocates only dirh_t (no per-child DirectoryEntry snapshot) and fails cleanly on OOM
    {   enum { NFILES = 64 };
        char ip[1200]; snprintf(ip, sizeof ip, "%s.img", root);
        char names[4 + NFILES * 5]; memcpy(names, "big", 3); size_t noff = 3;
        img_ent ent[2 + NFILES];
        ent[0] = (img_ent){1, 1, 0, 0, 0, 0};
        ent[1] = (img_ent){2, NFILES, 0, 3, 0, 0};
        for (int i = 0; i < NFILES; i++) {
            char nm[8]; int len = snprintf(nm, sizeof nm, "f%03d", i);
            memcpy(names + noff, nm, (size_t)len);
            ent[2 + i] = (img_ent){32, (uint64_t)(i + 1), (uint32_t)noff, (uint16_t)len, 1, 0};
            noff += (size_t)len;
        }
        uint64_t hd[3] = {32 + NFILES, (2 + NFILES) * sizeof(img_ent) + noff, 2 + NFILES};
        uint8_t data[NFILES] = {0};
        FILE *im = fopen(ip, "wb");
        fwrite("DMROMFS1", 1, 8, im); fwrite(hd, 1, sizeof hd, im); fwrite(data, 1, sizeof data, im);
        fwrite(ent, 1, sizeof ent, im); fwrite(names, 1, noff, im); fclose(im);
        CHECK(mount("img", ip, 1) == RES_OK, "mount a synthetic RomFS image");
        uint64_t ih = 0x5678; failed_allocations = 0; fail_handle = 1;
        Result ir = h_fs_OpenDirectory(&ih, "img:/big", DIRMODE_FILE);
        int ierr = errno; fail_handle = 0;
        CHECK(failed_allocations == 1 && ir == RES_FAIL && ierr == ENOMEM && ih == 0x5678, "image directory allocation failure returns RES_FAIL with ENOMEM and leaves handle untouched");
        calloc_calls = 0; calloc_bytes = 0; ih = 0;
        CHECK(h_fs_OpenDirectory(&ih, "img:/big", DIRMODE_FILE) == RES_OK && ih != 0, "image directory opens");
        CHECK(calloc_calls == 1 && calloc_bytes == sizeof(dirh_t), "image directory OpenDirectory allocates only dirh_t, not a per-child snapshot");
        DirectoryEntry one[2]; int64_t icnt = -1;
        CHECK(h_fs_ReadDirectory(&icnt, one, ih, 1) == RES_OK && icnt == 1 && !strcmp(one[0].name, "f000") && one[0].size == 1, "first 1-entry read returns f000");
        CHECK(h_fs_DirCount(&icnt, ih) == RES_OK && icnt == NFILES, "DirCount returns all 64 files without moving the cursor");
        CHECK(h_fs_ReadDirectory(&icnt, one, ih, 1) == RES_OK && icnt == 1 && !strcmp(one[0].name, "f001") && one[0].size == 2, "second 1-entry read returns f001");
        h_fs_CloseDirectory(ih);
        unlink(ip);
    }

    // Normalized overlay directory paths (folder base and RomFS image base): .., backslash spelling, above-root rejection, and image-only cursor path
    {
        char nbase[1100], nbase_a[1200], nov[1100], nov_a[1200], nimg[1200], np[1300];
        snprintf(nbase, sizeof nbase, "%s.nbase", root); snprintf(nbase_a, sizeof nbase_a, "%s/assets", nbase);
        snprintf(nov, sizeof nov, "%s.nov", root); snprintf(nov_a, sizeof nov_a, "%s/assets", nov);
        mkdir(nbase, 0755); mkdir(nbase_a, 0755); mkdir(nov, 0755); mkdir(nov_a, 0755);
        snprintf(np, sizeof np, "%s/base.txt", nbase_a); FILE *bf = fopen(np, "w"); fputs("base", bf); fclose(bf);
        snprintf(np, sizeof np, "%s/over.txt", nov_a); FILE *of = fopen(np, "w"); fputs("overlay", of); fclose(of);

        // Synthetic image with /assets/base.txt (4 bytes) and image-only /plain/only.txt (3 bytes)
        snprintf(nimg, sizeof nimg, "%s.nimg", root);
        const char names[] = "assetsplainbase.txtonly.txt";
        img_ent ent[5] = {
            {1, 2, 0, 0, 0, 0},     // 0: root -> children 1..2 (assets, plain)
            {3, 1, 0, 6, 0, 0},     // 1: assets -> child 3 (base.txt)
            {4, 1, 6, 5, 0, 0},     // 2: plain -> child 4 (only.txt)
            {32, 4, 11, 8, 1, 0},   // 3: base.txt ("base")
            {36, 3, 19, 8, 1, 0},   // 4: only.txt ("img")
        };
        const char payload[] = "baseimg";
        uint64_t hd[3] = {32 + 7, sizeof ent + (sizeof names - 1), 5};
        FILE *im = fopen(nimg, "wb");
        fwrite("DMROMFS1", 1, 8, im); fwrite(hd, 1, sizeof hd, im); fwrite(payload, 1, 7, im);
        fwrite(ent, 1, sizeof ent, im); fwrite(names, 1, sizeof names - 1, im); fclose(im);

        setenv("DM_ROMFS_OVERLAY", nov, 1);
        setenv("DM_ROMFS", nbase, 1);
        CHECK(h_fs_MountRom("rom_dir", NULL, 0) == RES_OK, "MountRom folder base with DM_ROMFS_OVERLAY");
        setenv("DM_ROMFS", nimg, 1);
        CHECK(h_fs_MountRom("rom_img", NULL, 0) == RES_OK, "MountRom image base with DM_ROMFS_OVERLAY");
        unsetenv("DM_ROMFS"); unsetenv("DM_ROMFS_OVERLAY");

        const char *mnts[] = {"rom_dir", "rom_img"};
        for (int m = 0; m < 2; m++) {
            char p0[64], p1[64], p2[64], pbad[64];
            snprintf(p0, sizeof p0, "%s:/assets", mnts[m]);
            snprintf(p1, sizeof p1, "%s:/assets/../assets", mnts[m]);
            snprintf(p2, sizeof p2, "%s:\\assets\\..\\assets", mnts[m]);
            snprintf(pbad, sizeof pbad, "%s:/assets/../..", mnts[m]);

            DirectoryEntry ref[4], alt[4]; int64_t rcnt = -1, rdc = -1, acnt = -1, adc = -1;
            uint64_t rh = 0;
            CHECK(h_fs_OpenDirectory(&rh, p0, DIRMODE_DIR | DIRMODE_FILE) == RES_OK &&
                  h_fs_DirCount(&rdc, rh) == RES_OK && h_fs_ReadDirectory(&rcnt, ref, rh, 4) == RES_OK &&
                  rdc == 2 && rcnt == 2, m ? "image overlay /assets enumerates base and overlay entries" : "folder overlay /assets enumerates base and overlay entries");
            if (rh) h_fs_CloseDirectory(rh);

            const char *spellings[] = {p1, p2};
            for (int s = 0; s < 2; s++) {
                uint64_t ah = 0; acnt = -1; adc = -1;
                CHECK(h_fs_OpenDirectory(&ah, spellings[s], DIRMODE_DIR | DIRMODE_FILE) == RES_OK &&
                      h_fs_DirCount(&adc, ah) == RES_OK && h_fs_ReadDirectory(&acnt, alt, ah, 4) == RES_OK,
                      s ? "backslash parent-normalized overlay path opens, counts, and reads" : "parent-normalized overlay path opens, counts, and reads");
                int same = adc == rdc && acnt == rcnt;
                for (int64_t i = 0; same && i < rcnt; i++)
                    same = !strcmp(ref[i].name, alt[i].name) && ref[i].type == alt[i].type && ref[i].size == alt[i].size;
                CHECK(same, s ? "backslash parent-normalized overlay matches /assets in names, types, sizes, and count" : "parent-normalized overlay matches /assets in names, types, sizes, and count");
                if (ah) h_fs_CloseDirectory(ah);
            }
            uint64_t badh = 0x4321;
            CHECK(h_fs_OpenDirectory(&badh, pbad, DIRMODE_DIR | DIRMODE_FILE) == RES_PATH_NOT_FOUND && badh == 0x4321,
                  m ? "image overlay path above mount root is rejected" : "folder overlay path above mount root is rejected");
        }

        // Image-only directory on an overlay-mounted image still uses the direct cursor path (no snapshot allocation)
        calloc_calls = 0; calloc_bytes = 0;
        uint64_t ph = 0; DirectoryEntry pent[2]; int64_t pcnt = -1;
        CHECK(h_fs_OpenDirectory(&ph, "rom_img:/plain/../plain", DIRMODE_FILE) == RES_OK &&
              calloc_calls == 1 && calloc_bytes == sizeof(dirh_t) &&
              h_fs_ReadDirectory(&pcnt, pent, ph, 2) == RES_OK && pcnt == 1 &&
              !strcmp(pent[0].name, "only.txt") && pent[0].size == 3,
              "image-only normalized path keeps cursor path without snapshot allocation");
        if (ph) h_fs_CloseDirectory(ph);

        unlink(nimg);
        snprintf(np, sizeof np, "%s/base.txt", nbase_a); unlink(np); rmdir(nbase_a); rmdir(nbase);
        snprintf(np, sizeof np, "%s/over.txt", nov_a); unlink(np); rmdir(nov_a); rmdir(nov);
    }

    // Large (38,000-entry) RomFS image with multi-layer overlay: streamed image children, no full-size snapshot allocation, mid-read DirCount, modes, order, winning metadata
    {
        enum { BIG_FILES = 37999, BIG_TOTAL = 38000 };
        char bimg[1200], bov1[1100], bov1_b[1200], bov1_d[1250], bov2[1100], bov2_b[1200], bp[1300];
        snprintf(bimg, sizeof bimg, "%s.big.img", root);
        snprintf(bov1, sizeof bov1, "%s.bov1", root); snprintf(bov1_b, sizeof bov1_b, "%s/big", bov1); snprintf(bov1_d, sizeof bov1_d, "%s/extra_dir", bov1_b);
        snprintf(bov2, sizeof bov2, "%s.bov2", root); snprintf(bov2_b, sizeof bov2_b, "%s/big", bov2);
        mkdir(bov1, 0755); mkdir(bov1_b, 0755); mkdir(bov1_d, 0755); mkdir(bov2, 0755); mkdir(bov2_b, 0755);
        snprintf(bp, sizeof bp, "%s/f00017", bov1_b); FILE *f1 = fopen(bp, "w"); fputs("override", f1); fclose(f1);   // 8 bytes
        snprintf(bp, sizeof bp, "%s/f00042", bov1_b); FILE *f2 = fopen(bp, "w"); fputs("first", f2); fclose(f2);      // 5 bytes (wins over bov2)
        snprintf(bp, sizeof bp, "%s/f00042", bov2_b); FILE *f3 = fopen(bp, "w"); fputs("no", f3); fclose(f3);         // 2 bytes (colliding name)
        snprintf(bp, sizeof bp, "%s/f00099", bov2_b); FILE *f4 = fopen(bp, "w"); fputs("last", f4); fclose(f4);       // 4 bytes

        img_ent *ent = calloc(2 + BIG_TOTAL, sizeof *ent);
        char *names = malloc(3 + 6 + BIG_FILES * 8);
        size_t noff = 0;
        memcpy(names + noff, "big", 3); noff += 3;
        ent[0] = (img_ent){1, 1, 0, 0, 0, 0};
        ent[1] = (img_ent){2, BIG_TOTAL, 0, 3, 0, 0};
        memcpy(names + noff, "d00000", 6);
        ent[2] = (img_ent){2 + BIG_TOTAL, 0, (uint32_t)noff, 6, 0, 0};   // empty subdir in image ('d' < 'f')
        noff += 6;
        for (int i = 0; i < BIG_FILES; i++) {
            char nm[16]; int l = snprintf(nm, sizeof nm, "f%05d", i);
            memcpy(names + noff, nm, (size_t)l);
            ent[3 + i] = (img_ent){32, 0, (uint32_t)noff, (uint16_t)l, 1, 0};
            noff += (size_t)l;
        }
        uint64_t hd[3] = {32, (2 + BIG_TOTAL) * sizeof(img_ent) + noff, 2 + BIG_TOTAL};
        FILE *im = fopen(bimg, "wb");
        fwrite("DMROMFS1", 1, 8, im); fwrite(hd, 1, sizeof hd, im);
        fwrite(ent, sizeof *ent, 2 + BIG_TOTAL, im); fwrite(names, 1, noff, im); fclose(im);
        free(ent); free(names);

        char ov_spec[2400]; snprintf(ov_spec, sizeof ov_spec, "%s:%s", bov1, bov2);
        setenv("DM_ROMFS", bimg, 1); setenv("DM_ROMFS_OVERLAY", ov_spec, 1);
        CHECK(h_fs_MountRom("big_ov", NULL, 0) == RES_OK, "mount 38,000-entry image with two overlay layers");
        unsetenv("DM_ROMFS"); unsetenv("DM_ROMFS_OVERLAY");

        // Allocation budget at open: only dirh_t + small overlay snapshot (never a 38,000-entry snapshot)
        calloc_calls = 0; calloc_bytes = 0;
        uint64_t bh = 0;
        CHECK(h_fs_OpenDirectory(&bh, "big_ov:/big", DIRMODE_DIR | DIRMODE_FILE) == RES_OK && bh != 0, "open 38,000-entry overlay image directory");
        CHECK(calloc_bytes < 65536, "overlay image OpenDirectory allocates only O(overlay) snapshot, not 38,000 entries");

        int64_t dc_all = -1, dc_mid = -1, seen = 0, dirs = 0, files = 0;
        int got17 = 0, got42 = 0, got99 = 0, got_extra = 0, got_d0 = 0, dup = 0, sorted_tail = 1;
        char prev_tail[32] = "";
        CHECK(h_fs_DirCount(&dc_all, bh) == RES_OK && dc_all == BIG_TOTAL + 1, "DirCount (all) returns 38,001 (38,000 image + 1 extra overlay dir)");
        DirectoryEntry batch[64]; int64_t n = 0; int reads_ok = 1;
        do {
            if (h_fs_ReadDirectory(&n, batch, bh, 64) != RES_OK) { reads_ok = 0; break; }
            if (seen == 0) {
                // First two names come from the last overlay folder (bov2: f00042, f00099); next two from bov1 (f00017, extra_dir)
                int first_two = n >= 4 &&
                    (!strcmp(batch[0].name, "f00042") || !strcmp(batch[0].name, "f00099")) &&
                    (!strcmp(batch[1].name, "f00042") || !strcmp(batch[1].name, "f00099")) &&
                    (!strcmp(batch[2].name, "f00017") || !strcmp(batch[2].name, "extra_dir")) &&
                    (!strcmp(batch[3].name, "f00017") || !strcmp(batch[3].name, "extra_dir"));
                CHECK(first_two, "last overlay folder names appear first, followed by earlier overlay layer");
                CHECK(h_fs_DirCount(&dc_mid, bh) == RES_OK && dc_mid == BIG_TOTAL + 1, "mid-read DirCount returns 38,001 without disturbing cursor");
            }
            for (int64_t i = 0; i < n; i++) {
                int64_t idx = seen + i;
                if (batch[i].type == ENT_DIR) dirs++; else files++;
                if (!strcmp(batch[i].name, "f00017")) { got17 += (batch[i].type == ENT_FILE && batch[i].size == 8); if (idx >= 4) dup = 1; }
                else if (!strcmp(batch[i].name, "f00042")) { got42 += (batch[i].type == ENT_FILE && batch[i].size == 5); if (idx >= 4) dup = 1; }
                else if (!strcmp(batch[i].name, "f00099")) { got99 += (batch[i].type == ENT_FILE && batch[i].size == 4); if (idx >= 4) dup = 1; }
                else if (!strcmp(batch[i].name, "extra_dir")) { got_extra += (batch[i].type == ENT_DIR && batch[i].size == 0); if (idx >= 4) dup = 1; }
                else if (!strcmp(batch[i].name, "d00000")) { got_d0 += (batch[i].type == ENT_DIR && batch[i].size == 0); }
                if (idx >= 4) {
                    if (prev_tail[0] && strcmp(prev_tail, batch[i].name) >= 0) sorted_tail = 0;
                    strlcpy(prev_tail, batch[i].name, sizeof prev_tail);
                }
            }
            seen += n;
        } while (n > 0);
        h_fs_CloseDirectory(bh);
        CHECK(reads_ok && seen == BIG_TOTAL + 1 && dirs == 2 && files == BIG_FILES, "small-batch streaming visits all 38,001 entries (2 dirs, 37,999 files)");
        CHECK(got17 == 1 && got42 == 1 && got99 == 1 && got_extra == 1 && got_d0 == 1 && !dup && sorted_tail,
              "winning overlay sizes/types appear once, image duplicates are skipped, and streamed tail stays sorted");

        // Mode-filtered counts and reads (DIRMODE_FILE and DIRMODE_DIR)
        uint64_t fh_dir = 0, fh_file = 0; int64_t cdir = -1, cfile = -1;
        CHECK(h_fs_OpenDirectory(&fh_dir, "big_ov:/big", DIRMODE_DIR) == RES_OK && h_fs_DirCount(&cdir, fh_dir) == RES_OK && cdir == 2 &&
              h_fs_ReadDirectory(&n, batch, fh_dir, 4) == RES_OK && n == 2 &&
              !strcmp(batch[0].name, "extra_dir") && !strcmp(batch[1].name, "d00000"),
              "DIRMODE_DIR counts and lists overlay dir then image dir");
        if (fh_dir) h_fs_CloseDirectory(fh_dir);
        CHECK(h_fs_OpenDirectory(&fh_file, "big_ov:/big", DIRMODE_FILE) == RES_OK && h_fs_DirCount(&cfile, fh_file) == RES_OK && cfile == BIG_FILES,
              "DIRMODE_FILE counts all 37,999 files across overlay and image");
        if (fh_file) h_fs_CloseDirectory(fh_file);

        unlink(bimg);
        snprintf(bp, sizeof bp, "%s/f00017", bov1_b); unlink(bp);
        snprintf(bp, sizeof bp, "%s/f00042", bov1_b); unlink(bp);
        rmdir(bov1_d); rmdir(bov1_b); rmdir(bov1);
        snprintf(bp, sizeof bp, "%s/f00042", bov2_b); unlink(bp);
        snprintf(bp, sizeof bp, "%s/f00099", bov2_b); unlink(bp);
        rmdir(bov2_b); rmdir(bov2);
    }

    fail_reading(sub, 1);
    CHECK(h_fs_CleanDirRec("w:/sub") != RES_OK, "a clean whose folder cannot be read fails");
    CHECK(h_fs_CleanDirRec("w:/sub") == RES_OK && rmdir(sub) == 0, "then it empties the folder");
    for (int i = 0; i < 3; i++) { snprintf(q, sizeof q, "%s/%s", ro, three[i]); unlink(q); snprintf(q, sizeof q, "%s/o%s", ov, three[i]); unlink(q); }
    CHECK(!rmdir(w) && !rmdir(ro) && !rmdir(ov), "remove the read-only, writable and overlay folders");
    CHECK(!unlink(file) && !rmdir(root), "remove the synthetic fixture");
    printf("== fs directory: %d checks, %d failed ==\n", checks, fails);
    return fails != 0;
}
