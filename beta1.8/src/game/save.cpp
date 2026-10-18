// ===================== 存档（v4 + v1/v2/v3 兼容）=====================
// 旧版三种格式：
//   v1  裸结构：固定 5001x1001，ep 尺寸有 bug（写了 W*W 而非 W*H）
//   v2  带头维度：7 字节 magic "2DMCBIN" + int version + int w + int h
//   v3  分块 "2DMCBNK"：32B 头 + 固定偏移列块（s/s2/ep 可原地重写）+ 文件尾 META 标签流
//
// v4（本次新增）在 v3 基础上做两件事：
//   1. 背包/护甲从 {id,count} 扩展为 {id,count,wear,flag}，用新标签承载
//   2. 箱子物品打包从 id + count*1000 升级为 id + count*1000 + wear*64000
//      读取旧档时 wear 视为 0（全新）
// 读 v1/v2/v3 都能正常加载并自动升级为 v4。
#include "game.h"
#include "game_render.h"
#include "../entity/entity.h"
#include "../platform/platform.h"
#include "../item/chest.h"
#include "../item/recipes.h"
#include "../world/blocks.h"
#include "../render/atlas.h"
#include "../render/renderer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <share.h>
#endif

namespace game {

using world::W;
using world::H;

namespace {

constexpr char BIN_MAGIC[7] = {'2', 'D', 'M', 'C', 'B', 'I', 'N'};
constexpr char BIN3_MAGIC[8] = {'2', 'D', 'M', 'C', 'B', 'N', 'K', '\0'};
constexpr int BIN_VERSION = 2;
constexpr int BIN3_VERSION = 3;
constexpr int BIN4_VERSION = 4;
constexpr int BIN3_HDR = 32;
constexpr int OLD_W = 5001, OLD_H = 1001;
constexpr i64 CHUNK_BYTES = (i64)world::CHUNK_COLS * H * 5;
constexpr i64 CHUNKS_END = BIN3_HDR + (i64)W * H * 5;

enum
{
    TAG_END = 0x00, TAG_DOUBLES = 0x01, TAG_INTS = 0x02, TAG_BEIBAO = 0x03,
    TAG_WEAR = 0x04, TAG_SHITI = 0x05, TAG_SHITI2 = 0x06, TAG_BOX = 0x07,
    TAG_BOIMES = 0x08, TAG_BOIMES2 = 0x09, TAG_GAMEMODE = 0x0A,
    // v4 新增
    TAG_STACKS = 0x0B,      // 背包 101 格 x (id,count,wear,flag)
    TAG_ARMOR = 0x0C,       // 护甲 5 格 x (id,count,wear,flag)
    TAG_BOX4 = 0x0D,        // 箱子（含耐久，32 位打包）
    TAG_SETTINGS = 0x0E,    // 设置
    TAG_FUEL = 0x0F,        // 熔炉燃料点数（旧版 rl，旧版没进存档）
};

struct FileGuard
{
    FILE *fp;
    explicit FileGuard(FILE *f) : fp(f) {}
    ~FileGuard() { if(fp) fclose(fp); }
    FileGuard(const FileGuard &) = delete;
    FileGuard &operator=(const FileGuard &) = delete;
};

void buf_put(std::vector<u8> &b, const void *p, size_t n)
{
    const u8 *q = (const u8 *)p;
    b.insert(b.end(), q, q + n);
}
void buf_u16(std::vector<u8> &b, u16 v) { buf_put(b, &v, 2); }
void buf_u32(std::vector<u8> &b, u32 v) { buf_put(b, &v, 4); }
void buf_tag(std::vector<u8> &b, u16 tag, const void *p, size_t n)
{
    buf_u16(b, tag);
    buf_u32(b, (u32)n);
    if(n) buf_put(b, p, n);
}

constexpr int chunk_cols(int c)
{
    return (W - c * world::CHUNK_COLS) > world::CHUNK_COLS ? world::CHUNK_COLS
                                                          : (W - c * world::CHUNK_COLS);
}
constexpr i64 chunk_offset(int c) { return BIN3_HDR + (i64)c * CHUNK_BYTES; }

// 通用 2D 读取：从文件读 sw x sh，拷进 cur_w x cur_h（过大忽略，过小保留原值）
template<typename T>
void read2d(FILE *fp, T *arr, int cur_w, int cur_h, int sw, int sh)
{
    int cw = (sw < cur_w) ? sw : cur_w;
    int ch = (sh < cur_h) ? sh : cur_h;
    std::vector<u8> tmp((size_t)(sh > 0 ? sh : 1) * sizeof(T));
    for(int i = 0; i < sw; i++)
    {
        if(sh > 0 && fread(tmp.data(), sizeof(T), sh, fp) != (size_t)sh) return;
        if(i < cw)
            for(int j = 0; j < ch; j++)
                memcpy(&arr[(size_t)i * cur_h + j], &tmp[(size_t)j * sizeof(T)], sizeof(T));
    }
}

// ---------------- v4 实体序列化 ----------------
// 逐字段标签化，结构体布局变化不会坏档（沿用 v3 的做法）
struct EntField
{
    u8 fid;
    u8 len;
    size_t off;
};

std::vector<EntField> ent_fields()
{
    std::vector<EntField> f;
    entity::Entity dummy;
    auto add = [&](u8 id, u8 len, size_t off) { f.push_back({id, len, off}); };
    u8 k = 0;
    add(k++, 8, (size_t)((char *)&dummy.hp - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.hp_max - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.hurt - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.x - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.y - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.vx - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.vy - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.random_tick - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.direction - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.sight - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.attack_tick - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.target_x - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.target_y - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.anim - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.fuse - (char *)&dummy));
    add(k++, 8, (size_t)((char *)&dummy.flash - (char *)&dummy));
    u8 k2 = 0;
    add(k2++, 4, (size_t)((char *)&dummy.type - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.jp - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.cd - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.ai - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.nbt - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.chv - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.conl - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.conr - (char *)&dummy));
    add(k2++, 4, (size_t)((char *)&dummy.wd - (char *)&dummy));
    u8 k3 = 0;
    add(k3++, 1, (size_t)((char *)&dummy.ch - (char *)&dummy));
    add(k3++, 1, (size_t)((char *)&dummy.isnotouch - (char *)&dummy));
    add(k3++, 1, (size_t)((char *)&dummy.islife2 - (char *)&dummy));
    add(k3++, 1, (size_t)((char *)&dummy.issd - (char *)&dummy));
    add(k3++, 1, (size_t)((char *)&dummy.state - (char *)&dummy));
    return f;
}

void buf_entities(std::vector<u8> &b, const entity::Entity *arr)
{
    std::vector<EntField> F = ent_fields();
    for(int i = 0; i < entity::MAX_ENT; i++)
    {
        buf_u16(b, (u16)F.size());
        for(const EntField &f : F)
        {
            b.push_back(f.fid);
            b.push_back(f.len);
            buf_put(b, (const char *)&arr[i] + f.off, f.len);
        }
    }
}

void parse_entities(const u8 *p, size_t len, entity::Entity *arr)
{
    std::vector<EntField> F = ent_fields();
    // Properly zero‑initialize each Entity using default construction
    for (int i = 0; i < entity::MAX_ENT; ++i) {
        arr[i] = entity::Entity();
    }
    size_t off = 0;
    int idx = 0;
    while(off + 2 <= len && idx < entity::MAX_ENT)
    {
        u16 nf;
        memcpy(&nf, p + off, 2);
        off += 2;
        entity::Entity &e = arr[idx++];
        for(int f = 0; f < nf && off + 2 <= len; f++)
        {
            u8 fid = p[off], flen = p[off + 1];
            off += 2;
            if(off + flen > len) break;
            for(const EntField &ef : F)
                if(ef.fid == fid && ef.len == flen)
                {
                    memcpy((char *)&e + ef.off, p + off, flen);
                    break;
                }
            off += flen;
        }
    }
}

// ---------------- 建 META ----------------
void build_meta(std::vector<u8> &m)
{
    State &s = st();
    Player &p = s.pl;

    double dv[8] = {p.x, p.y, p.life, (double)s.boss2hp, p.jx, p.jy,
                    (double)p.spy, (double)p.spx};
    buf_tag(m, TAG_DOUBLES, dv, sizeof(dv));

    int iv[16] = {world::dim, world::back_x, world::back_y, world::ed,
                  world::now_time, world::night, world::will_night, world::will_light,
                  world::gamemode, 0, p.cho, p.wudi, p.oxygen,
                  (int)p.fell, p.jump, world::oi};
    buf_tag(m, TAG_INTS, iv, sizeof(iv));

    // ★ v4：背包带耐久
    {
        std::vector<u8> b;
        for(int i = 0; i < item::INV_SLOTS; i++)
        {
            const item::Stack &stk = item::beibao[i];
            u16 id = (u16)(stk.id < 0 ? 0 : stk.id);
            u16 n = (u16)(stk.count < 0 ? 0 : stk.count);
            buf_u16(b, id);
            buf_u16(b, n);
            b.push_back(stk.wear);
            b.push_back(stk.flag);
        }
        buf_tag(m, TAG_STACKS, b.data(), b.size());
    }
    // 护甲
    {
        std::vector<u8> b;
        for(int i = 0; i < item::ARMOR_SLOTS; i++)
        {
            const item::Stack &stk = item::armor[i];
            buf_u16(b, (u16)(stk.id < 0 ? 0 : stk.id));
            buf_u16(b, (u16)(stk.count < 0 ? 0 : stk.count));
            b.push_back(stk.wear);
            b.push_back(stk.flag);
        }
        buf_tag(m, TAG_ARMOR, b.data(), b.size());
    }
    // 兼容旧读法：写一份 {id,count} 数组（v3 的 TAG_BEIBAO），
    // 这样万一用旧 exe 打开也能读到物品（耐久丢失）
    {
        std::vector<int> old(item::INV_SLOTS * 2, 0);
        for(int i = 0; i < item::INV_SLOTS; i++)
        {
            old[i * 2] = item::beibao[i].id;
            old[i * 2 + 1] = item::beibao[i].count;
        }
        buf_tag(m, TAG_BEIBAO, old.data(), old.size() * sizeof(int));
    }
    {
        std::vector<int> wr(item::ARMOR_SLOTS, 0);
        for(int i = 0; i < item::ARMOR_SLOTS; i++) wr[i] = item::armor[i].id;
        buf_tag(m, TAG_WEAR, wr.data(), wr.size() * sizeof(int));
    }

    // 实体
    {
        std::vector<u8> eb;
        buf_entities(eb, entity::ents);
        buf_tag(m, TAG_SHITI, eb.data(), eb.size());
    }
    // 箱子（v4 32 位打包，含耐久）
    {
        std::vector<u8> bb;
        u32 n = (u32)item::box_count();
        buf_put(bb, &n, 4);
        for(u32 i = 0; i < n; i++)
        {
            const item::Box &bx = item::box_at((int)i);
            buf_u16(bb, bx.bx);
            buf_u16(bb, bx.by);
            for(int j = 0; j < item::BOX_SLOTS; j++)
            {
                u32 v = bx.things[j];
                buf_put(bb, &v, 4);
            }
        }
        buf_tag(m, TAG_BOX4, bb.data(), bb.size());
    }

    buf_tag(m, TAG_BOIMES, world::boimes, sizeof(i16) * (W + 5));
    buf_tag(m, TAG_BOIMES2, world::boimes2, sizeof(i16) * (W + 5));
    {
        // 前 8 个是老字段，第 9 个是玩家皮肤下标（v5 起写入）
        int sv[10] = {s.CanGet, s.savetype, s.special_effect, s.setfps,
                      s.durability_on, s.tile_px, s.show_fps, s.sound_on,
                      s.skin_idx, 0};
        buf_tag(m, TAG_SETTINGS, sv, sizeof(sv));
    }
    {
        int f = item::fuel();
        buf_tag(m, TAG_FUEL, &f, sizeof(f));
    }
    buf_u16(m, TAG_END);
    buf_u32(m, 0);
}

// ---------------- 写盘（后台线程）----------------
struct SaveJob
{
    std::string path;
    bool full = false;
    std::vector<int> chunks;
    std::vector<u8> chunkbuf;
    std::vector<u8> meta;
};

std::mutex g_mtx;
std::condition_variable g_cv, g_done;
std::thread g_thread;
bool g_thread_run = false, g_pending = false, g_active = false, g_shutdown = false;
SaveJob g_job;
std::vector<bool> g_dirty(world::NCHUNKS, true);

void snapshot_chunk(std::vector<u8> &dst, int c)
{
    int c0 = c * world::CHUNK_COLS;
    size_t n = (size_t)chunk_cols(c) * H;
    buf_put(dst, &world::s[c0][0], n * sizeof(i16));
    buf_put(dst, &world::s2[c0][0], n * sizeof(i16));
    buf_put(dst, &world::ep[c0][0], n * sizeof(bool));
}

bool file_is_v3(const std::string &path)
{
    FileGuard fg(fopen(path.c_str(), "rb"));
    if(!fg.fp) return false;
    char hdr[BIN3_HDR];
    FILE *fp = fg.fp;
    bool ok = false;
    if(fread(hdr, 1, BIN3_HDR, fp) == BIN3_HDR && memcmp(hdr, BIN3_MAGIC, 8) == 0)
    {
        int w, h, cc;
        memcpy(&w, hdr + 12, 4);
        memcpy(&h, hdr + 16, 4);
        memcpy(&cc, hdr + 20, 4);
        ok = (w == W && h == H && cc == world::CHUNK_COLS);
    }
    return ok;
}

// v3 与 v4 的分块布局完全相同，只是 META 标签不同。
// 原地增量写不会碰版本号字段，所以「v3 -> v4 升级」必须走一次全量写，
// 否则文件头永远停在 3，下次读档还会被当成旧档再升一次。
bool file_is_v4(const std::string &path)
{
    FileGuard fg(fopen(path.c_str(), "rb"));
    if(!fg.fp) return false;
    char hdr[BIN3_HDR];
    FILE *fp = fg.fp;
    if(fread(hdr, 1, BIN3_HDR, fp) != BIN3_HDR) return false;
    if(memcmp(hdr, BIN3_MAGIC, 8) != 0) return false;
    int ver = 0;
    memcpy(&ver, hdr + 8, 4);
    return ver >= BIN4_VERSION;
}

bool write_job(SaveJob &job)
{
    if(job.full)
    {
        FileGuard fg(fopen(job.path.c_str(), "wb"));
        if(!fg.fp) return false;
        FILE *fp = fg.fp;
        char hdr[BIN3_HDR] = {0};
        memcpy(hdr, BIN3_MAGIC, 8);
        int v = BIN4_VERSION, w = W, h = H, cc = world::CHUNK_COLS;
        i64 mo = CHUNKS_END;
        memcpy(hdr + 8, &v, 4);
        memcpy(hdr + 12, &w, 4);
        memcpy(hdr + 16, &h, 4);
        memcpy(hdr + 20, &cc, 4);
        memcpy(hdr + 24, &mo, 8);
        fwrite(hdr, 1, BIN3_HDR, fp);
        fwrite(job.chunkbuf.data(), 1, job.chunkbuf.size(), fp);
        fwrite(job.meta.data(), 1, job.meta.size(), fp);
        return true;
    }
    FileGuard fg(fopen(job.path.c_str(), "r+b"));
    if(!fg.fp) return false;
    FILE *fp = fg.fp;
    size_t off = 0;
    for(size_t k = 0; k < job.chunks.size(); k++)
    {
        int c = job.chunks[k];
        size_t bytes = (size_t)chunk_cols(c) * H * 5;
        if(_fseeki64(fp, chunk_offset(c), SEEK_SET) != 0) return false;
        fwrite(job.chunkbuf.data() + off, 1, bytes, fp);
        off += bytes;
    }
    if(!job.meta.empty())
    {
        if(_fseeki64(fp, 0, SEEK_END) != 0) return false;
        std::string old;
        i64 end = _ftelli64(fp);
        if(_fseeki64(fp, 24, SEEK_SET) != 0) return false;
        i64 old_off = 0;
        if(fread(&old_off, 8, 1, fp) != 1) return false;
        _fseeki64(fp, CHUNKS_END, SEEK_SET);
        fwrite(job.meta.data(), 1, job.meta.size(), fp);
        _fseeki64(fp, 24, SEEK_SET);
        i64 newoff = CHUNKS_END;
        fwrite(&newoff, 8, 1, fp);
        fflush(fp);
        i64 want = CHUNKS_END + (i64)job.meta.size();
        (void)end;
        (void)old;
        (void)old_off;
#ifdef _WIN32
        HANDLE hf = (HANDLE)_get_osfhandle(_fileno(fp));
        if(hf != INVALID_HANDLE_VALUE && hf)
        {
            _fseeki64(fp, want, SEEK_SET);
            fflush(fp);
            SetEndOfFile(hf);
        }
#endif
    }
    return true;
}

void writer_loop()
{
    std::unique_lock<std::mutex> lk(g_mtx);
    while(true)
    {
        g_cv.wait(lk, [] { return g_pending || !g_thread_run; });
        if(!g_pending && !g_thread_run) break;
        SaveJob job = std::move(g_job);
        g_pending = false;
        g_active = true;
        lk.unlock();
        write_job(job);
        lk.lock();
        g_active = false;
        g_done.notify_all();
    }
}

void save_init()
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if(g_thread_run) return;
    g_thread_run = true;
    g_shutdown = false;
    g_thread = std::thread(writer_loop);
    atexit(shutdown);
}

}   // namespace

void save_world(bool blocking, bool force_full)
{
    if(world_name[0] == 0) return;
    save_init();
    std::string path = std::string(world_name) + ".bin";
    SaveJob job;
    job.path = path;
    // v3 与 v4 布局相同但标签不同：只有确实是 v4 才走增量写，
    // 否则必须全量重写一次把版本号与 v4 标签写进去。
    job.full = force_full || !file_is_v4(path);
    if(job.full)
    {
        for(int c = 0; c < world::NCHUNKS; c++)
        {
            job.chunks.push_back(c);
            snapshot_chunk(job.chunkbuf, c);
        }
        std::fill(g_dirty.begin(), g_dirty.end(), false);
    }
    else
    {
        for(int c = 0; c < world::NCHUNKS; c++)
        {
            if(!g_dirty[c]) continue;
            job.chunks.push_back(c);
            snapshot_chunk(job.chunkbuf, c);
            g_dirty[c] = false;
        }
    }
    build_meta(job.meta);
    std::unique_lock<std::mutex> lk(g_mtx);
    g_job = std::move(job);
    g_pending = true;
    g_cv.notify_one();
    if(blocking)
        g_done.wait(lk, [] { return !g_pending && !g_active; });
}

void shutdown()
{
    {
        std::unique_lock<std::mutex> lk(g_mtx);
        if(g_shutdown) return;
        g_done.wait(lk, [] { return !g_pending && !g_active; });
        g_thread_run = false;
        g_shutdown = true;
        g_cv.notify_one();
    }
    if(g_thread.joinable()) g_thread.join();
}

// ============================================================================
//                                读档
// ============================================================================
namespace {

bool load_v3(const char *filename, bool v4)
{
    FileGuard fg(fopen(filename, "rb"));
    if(!fg.fp) return false;
    FILE *fp = fg.fp;
    char hdr[BIN3_HDR];
    if(fread(hdr, 1, BIN3_HDR, fp) != BIN3_HDR) return false;
    if(memcmp(hdr, BIN3_MAGIC, 8) != 0) return false;
    int w, h, cc;
    i64 meta_off;
    memcpy(&w, hdr + 12, 4);
    memcpy(&h, hdr + 16, 4);
    memcpy(&cc, hdr + 20, 4);
    memcpy(&meta_off, hdr + 24, 8);
    if(w != W || h != H || cc != world::CHUNK_COLS || meta_off < CHUNKS_END)
        return false;

    for(int c = 0; c < world::NCHUNKS; c++)
    {
        int c0 = c * world::CHUNK_COLS;
        size_t n = (size_t)chunk_cols(c) * H;
        if(_fseeki64(fp, chunk_offset(c), SEEK_SET) != 0) return false;
        if(fread(&world::s[c0][0], sizeof(i16), n, fp) != n) return false;
        if(fread(&world::s2[c0][0], sizeof(i16), n, fp) != n) return false;
        if(fread(&world::ep[c0][0], sizeof(bool), n, fp) != n) return false;
    }
    if(_fseeki64(fp, meta_off, SEEK_SET) != 0) return false;
    std::vector<u8> meta;
    {
        char tmp[65536];
        size_t r;
        while((r = fread(tmp, 1, sizeof(tmp), fp)) > 0)
        {
            meta.insert(meta.end(), tmp, tmp + r);
            if(meta.size() > (size_t)64 * 1024 * 1024) return false;
        }
    }

    State &s = st();
    Player &p = s.pl;
    world::gamemode = 1;
    bool got_stacks = false, got_armor = false;
    size_t off = 0;
    while(off + 6 <= meta.size())
    {
        u16 tag;
        u32 len;
        memcpy(&tag, &meta[off], 2);
        memcpy(&len, &meta[off + 2], 4);
        off += 6;
        if(tag == TAG_END) break;
        if(off + len > meta.size()) break;
        const u8 *q = &meta[off];
        switch(tag)
        {
        case TAG_DOUBLES:
            if(len >= 32)
            {
                double d[4];
                memcpy(d, q, 32);
                p.x = d[0]; p.y = d[1]; p.life = d[2]; s.boss2hp = (i32)d[3];
            }
            if(len >= 64)
            {
                double d[8];
                memcpy(d, q, 64);
                p.jx = d[4]; p.jy = d[5]; p.spy = d[6]; p.spx = d[7];
            }
            break;
        case TAG_INTS:
            if(len >= 32)
            {
                int v[8];
                memcpy(v, q, 32);
                world::dim = v[0]; world::back_x = v[1]; world::back_y = v[2];
                world::ed = v[3]; world::now_time = v[4]; world::night = v[5];
                world::will_night = v[6]; world::will_light = v[7];
            }
            if(len >= (u32)(sizeof(int) * 16))
            {
                int v[16];
                memcpy(v, q, sizeof(v));
                world::gamemode = v[8];
                p.cho = v[10]; p.wudi = v[11]; p.oxygen = v[12];
                p.fell = v[13]; p.jump = v[14]; world::oi = v[15];
            }
            break;
        case TAG_STACKS:      // ★ v4
            if(len >= (u32)(item::INV_SLOTS * 6))
            {
                const u8 *r = q;
                for(int i = 0; i < item::INV_SLOTS; i++)
                {
                    u16 id, n;
                    memcpy(&id, r, 2); memcpy(&n, r + 2, 2);
                    item::beibao[i].id = (i16)id;
                    item::beibao[i].count = (i16)n;
                    item::beibao[i].wear = r[4];
                    item::beibao[i].flag = r[5];
                    if(id >= BLOCK_COUNT) item::beibao[i].reset();
                    r += 6;
                }
                got_stacks = true;
            }
            break;
        case TAG_ARMOR:       // ★ v4
            if(len >= (u32)(item::ARMOR_SLOTS * 6))
            {
                const u8 *r = q;
                for(int i = 0; i < item::ARMOR_SLOTS; i++)
                {
                    u16 id, n;
                    memcpy(&id, r, 2); memcpy(&n, r + 2, 2);
                    item::armor[i].id = (i16)id;
                    item::armor[i].count = (i16)n;
                    item::armor[i].wear = r[4];
                    item::armor[i].flag = r[5];
                    if(id >= BLOCK_COUNT) item::armor[i].reset();
                    r += 6;
                }
                got_armor = true;
            }
            break;
        case TAG_BEIBAO:      // v3 旧读法
            if(!got_stacks && len >= (u32)(sizeof(int) * 202))
            {
                int v[202];
                memcpy(v, q, sizeof(v));
                for(int i = 0; i < item::INV_SLOTS; i++)
                {
                    i32 id = v[i * 2], n = v[i * 2 + 1];
                    if(id < 0 || id >= BLOCK_COUNT) { id = 0; n = 0; }
                    item::beibao[i].id = (i16)id;
                    item::beibao[i].count = (i16)n;
                    item::beibao[i].wear = 0;   // 旧档视为全新
                    item::beibao[i].flag = 0;
                }
            }
            break;
        case TAG_WEAR:        // v3 护甲读法
            if(!got_armor && len >= 24)
            {
                int v[6] = {0};
                memcpy(v, q, 24);
                for(int i = 1; i < item::ARMOR_SLOTS; i++)
                {
                    i32 id = v[i];
                    if(id < 0 || id >= BLOCK_COUNT) id = 0;
                    item::armor[i].id = (i16)id;
                    item::armor[i].count = id > 0 ? 1 : 0;
                    item::armor[i].wear = 0;
                    item::armor[i].flag = 0;
                }
            }
            break;
        case TAG_GAMEMODE:
            if(len >= 4) memcpy(&world::gamemode, q, 4);
            break;
        case TAG_SHITI:
            parse_entities(q, len, entity::ents);
            break;
        case TAG_BOX4:        // ★ v4
            if(len >= 4)
            {
                u32 n;
                memcpy(&n, q, 4);
                item::box_reset();
                if(4 + (size_t)n * (4 + item::BOX_SLOTS * 4) <= len)
                    for(u32 i = 0; i < n; i++)
                    {
                        const u8 *r = q + 4 + (size_t)i * (4 + item::BOX_SLOTS * 4);
                        u16 bx, by;
                        memcpy(&bx, r, 2);
                        memcpy(&by, r + 2, 2);
                        i32 idx = item::alloc_box(bx, by);
                        item::Box &b = item::box_at(idx);
                        for(int j = 0; j < item::BOX_SLOTS; j++)
                        {
                            u32 v;
                            memcpy(&v, r + 4 + j * 4, 4);
                            // 解包：id + count*1000 + wear*64000（24 位上限内）
                            i32 id = (i32)(v % 1000u);
                            i32 cnt = (i32)((v / 1000u) % 64u);
                            if(id < 0 || id >= BLOCK_COUNT) { id = 0; cnt = 0; }
                            b.things[j] = (u16)(id + (cnt > 0 ? cnt : 0) * 1000);
                        }
                    }
            }
            break;
        case TAG_BOX:         // v3 旧箱子
            if(!v4 && len >= 4)
            {
                u32 n;
                memcpy(&n, q, 4);
                item::box_reset();
                if(4 + (size_t)n * 58 <= len)
                    for(u32 i = 0; i < n; i++)
                    {
                        const u8 *r = q + 4 + (size_t)i * 58;
                        u16 bx, by, th[27];
                        memcpy(&bx, r, 2);
                        memcpy(&by, r + 2, 2);
                        memcpy(th, r + 4, sizeof(th));
                        i32 idx = item::alloc_box(bx, by);
                        item::Box &b = item::box_at(idx);
                        for(int j = 0; j < item::BOX_SLOTS; j++)
                        {
                            i32 id = th[j] % 1000;
                            i32 cnt = th[j] / 1000;
                            if(id < 0 || id >= BLOCK_COUNT) { id = 0; cnt = 0; }
                            if(cnt > 64) cnt = 64;
                            b.things[j] = (u16)(id + cnt * 1000);
                        }
                    }
            }
            break;
        case TAG_BOIMES:
            if(len >= sizeof(i16) * (W + 5)) memcpy(world::boimes, q, sizeof(i16) * (W + 5));
            break;
        case TAG_BOIMES2:
            if(len >= sizeof(i16) * (W + 5)) memcpy(world::boimes2, q, sizeof(i16) * (W + 5));
            break;
        case TAG_FUEL:        // ★ v4：熔炉燃料（旧版是运行时全局，不持久化）
            if(len >= 4)
            {
                int f = 0;
                memcpy(&f, q, 4);
                if(f < 0) f = 0;
                item::add_fuel(f - item::fuel());
            }
            break;
        case TAG_SETTINGS:    // ★ v4（老档 8 个 int）/ v5（多了皮肤下标）
        {
            int v[10] = {0};
            size_t nb = len < sizeof(v) ? len : sizeof(v);
            memcpy(v, q, nb);
            if(len >= 32)
            {
                s.CanGet = v[0]; s.savetype = v[1]; s.special_effect = v[2];
                s.setfps = v[3]; s.durability_on = v[4]; s.tile_px = v[5];
                s.show_fps = v[6]; s.sound_on = v[7];
            }
            if(len >= 36) s.skin_idx = v[8];
            // 皮肤下标越界（换了机器、皮肤变少了）就回落到第 0 张
            if(s.skin_idx < 0 || s.skin_idx >= atlas::skin_count()) s.skin_idx = 0;
            if(atlas::skin_count() > 0 && atlas::skin_apply(s.skin_idx))
                render::upload_atlas();
            break;
        }
        default: break;
        }
        off += len;
    }

    entity::hash_clear();
    for(int i = 0; i < entity::MAX_ENT; i++)
        if(entity::ents[i].type > 0) entity::hash_add(i, entity::ents[i].x, entity::ents[i].y);
    p.lx = (i32)(p.x + 0.5);
    p.ly = (i32)(p.y + 0.5);
    light_update();
    armor_update();
    std::fill(g_dirty.begin(), g_dirty.end(), false);
    // 升级为 v4：立刻全量重写一次（旧档的 META 标签不含耐久度，必须重写）
    if(!v4)
    {
        platform::log("旧档升级：%s 从 v3 重写为 v4", filename);
        save_world(true, true);
    }
    return true;
}

bool load_v1v2(const char *filename)
{
    FileGuard fg(fopen(filename, "rb"));
    if(!fg.fp) return false;
    FILE *fp = fg.fp;
    char magic[7];
    if(fread(magic, 1, 7, fp) != 7) return false;
    if(memcmp(magic, BIN_MAGIC, 7) != 0) return false;
    // 文件头 = magic(7) + version(int)。v2 之后紧跟 w、h 两个 int。
    // 注意：旧版先 fread 7 字节 magic，再 fread 4 字节 version，所以 version
    // 落在**偏移 7**，而不是 8（对照 beta1.8.cpp 第 3131-3152 行）。
    int version;
    if(fread(&version, sizeof(int), 1, fp) != 1) return false;
    int saved_w, saved_h;
    bool ep_bug = false;
    if(version == 1)
    {
        saved_w = OLD_W;
        saved_h = OLD_H;
        ep_bug = true;   // 旧版 v1 的 ep 被写成 saved_w * saved_w
    }
    else
    {
        if(fread(&saved_w, sizeof(int), 1, fp) != 1) return false;
        if(fread(&saved_h, sizeof(int), 1, fp) != 1) return false;
    }
    if(saved_w <= 0 || saved_h <= 0 || saved_w > 100000 || saved_h > 100000)
        return false;

    read2d(fp, &world::s[0][0], W, H, saved_w, saved_h);
    read2d(fp, &world::s2[0][0], W, H, saved_w, saved_h);

    // 旧档背包是 int[101][2]，读进来后再转 Stack
    {
        int old[202] = {0};
        size_t rd = fread(old, sizeof(int), 202, fp);
        if(rd != 202) return false;
        for(int i = 0; i < item::INV_SLOTS; i++)
        {
            i32 id = old[i * 2], n = old[i * 2 + 1];
            if(id < 0 || id >= BLOCK_COUNT) { id = 0; n = 0; }
            item::beibao[i].id = (i16)id;
            item::beibao[i].count = (i16)n;
            item::beibao[i].wear = 0;
            item::beibao[i].flag = 0;
        }
    }
    State &s = st();
    Player &p = s.pl;
    fread(&world::dim, sizeof(int), 1, fp);
    fread(&p.x, sizeof(double), 1, fp);
    fread(&p.y, sizeof(double), 1, fp);
    fread(&world::back_x, sizeof(int), 1, fp);
    fread(&world::back_y, sizeof(int), 1, fp);
    fread(&p.life, sizeof(double), 1, fp);
    fread(&world::now_time, sizeof(int), 1, fp);
    fread(&world::night, sizeof(int), 1, fp);
    fread(&world::will_night, sizeof(int), 1, fp);
    fread(&world::will_light, sizeof(int), 1, fp);

    // 实体：旧档是裸 guaiwu 结构，新结构布局不同 -> 跳过（只保留位置）
    {
        struct OldEnt
        {
            double HP, hurt, gx, gy, spgy, spgx, randomtick, direction, sd,
                   gongjitick, tox, toy, maxHP;
            int type, jp, CD, AI, nbt, chv, conl, conr, wd;
            bool ch, isnotouch, islife2, issd;
            unsigned char state;
        };
        auto skip_old_entities = [&](i32 count) {
            std::vector<char> buf(sizeof(OldEnt));
            for(i32 i = 0; i < count && i < entity::MAX_ENT; i++)
            {
                if(fread(buf.data(), 1, sizeof(OldEnt), fp) != sizeof(OldEnt)) return;
                OldEnt e;
                memcpy(&e, buf.data(), sizeof(OldEnt));
                if(e.type <= 0) continue;
                entity::Entity &ne = entity::ents[i];
                ne.reset();
                ne.type = e.type;
                ne.hp = e.HP;
                ne.hp_max = e.maxHP > 0 ? e.maxHP : e.HP;
                ne.hurt = e.hurt;
                ne.x = e.gx;
                ne.y = e.gy;
                ne.vx = e.spgx;
                ne.vy = e.spgy;
                ne.sight = e.sd;
                ne.ai = e.AI;
                ne.nbt = e.nbt;
                ne.direction = e.direction;
                ne.attack_tick = e.gongjitick;
                ne.state = (u8)e.state;
                entity::hash_add(i, ne.x, ne.y);
            }
        };
        entity::clear_all();
        skip_old_entities(entity::MAX_ENT);
        // 下界的实体数组在旧档里紧跟其后；新版本只有一份实体池，直接跳过
        std::vector<char> tmp(sizeof(OldEnt));
        for(i32 i = 0; i < entity::MAX_ENT; i++)
            if(fread(tmp.data(), 1, sizeof(OldEnt), fp) != sizeof(OldEnt)) break;
    }

    // 旧版 wear[6]：索引 1..4 是护甲 id
    {
        int wr[6] = {0};
        fread(wr, sizeof(int), 6, fp);
        for(int i = 1; i < item::ARMOR_SLOTS; i++)
        {
            i32 id = wr[i];
            if(id < 0 || id >= BLOCK_COUNT) id = 0;
            item::armor[i].id = (i16)id;
            item::armor[i].count = id > 0 ? 1 : 0;
            item::armor[i].wear = 0;
            item::armor[i].flag = 0;
        }
    }
    fread(&world::ed, sizeof(int), 1, fp);

    // ep
    if(ep_bug)
    {
        std::vector<char> row(saved_h);
        for(int i = 0; i < saved_w; i++)
        {
            if(fread(row.data(), sizeof(bool), saved_h, fp) != (size_t)saved_h) break;
            if(i < W)
                for(int j = 0; j < saved_h && j < H; j++) world::ep[i][j] = row[j];
        }
    }
    else read2d(fp, &world::ep[0][0], W, H, saved_w, saved_h);

    // 箱子（旧打包）
    {
        int box_count = 0;
        fread(&box_count, sizeof(int), 1, fp);
        item::box_reset();
        for(int i = 0; i < box_count && i < 100000; i++)
        {
            unsigned short bx, by, th[27];
            if(fread(&bx, 2, 1, fp) != 1) break;
            if(fread(&by, 2, 1, fp) != 1) break;
            if(fread(th, 2, 27, fp) != 27) break;
            i32 idx = item::alloc_box(bx, by);
            item::Box &b = item::box_at(idx);
            for(int j = 0; j < item::BOX_SLOTS; j++)
            {
                i32 id = th[j] % 1000;
                i32 cnt = th[j] / 1000;
                if(id < 0 || id >= BLOCK_COUNT) { id = 0; cnt = 0; }
                if(cnt > 64) cnt = 64;
                b.things[j] = (u16)(id + cnt * 1000);
            }
        }
    }
    fread(&s.boss2hp, sizeof(double), 1, fp);
    int bc = (version == 1) ? 5005 : (saved_w + 5);
    {
        std::vector<i16> bt(bc);
        if(fread(bt.data(), sizeof(i16), bc, fp) == (size_t)bc)
            for(int i = 0; i < bc && i < W + 5; i++) world::boimes[i] = bt[i];
        if(fread(bt.data(), sizeof(i16), bc, fp) == (size_t)bc)
            for(int i = 0; i < bc && i < W + 5; i++) world::boimes2[i] = bt[i];
    }
    if(saved_w < W) world::expand_from(saved_w);

    entity::hash_clear();
    for(int i = 0; i < entity::MAX_ENT; i++)
        if(entity::ents[i].type > 0) entity::hash_add(i, entity::ents[i].x, entity::ents[i].y);
    p.lx = (i32)(p.x + 0.5);
    p.ly = (i32)(p.y + 0.5);
    light_update();
    armor_update();
    world::mark_all_dirty();
    // 升级为 v4
    save_world(true);
    return true;
}

}   // namespace

bool load_world(const char *name)
{
    std::string path = std::string(name) + ".bin";
    char mg[8] = {0};
    size_t n = 0;
    {
        FileGuard fg(fopen(path.c_str(), "rb"));
        if(!fg.fp) return false;
        n = fread(mg, 1, 8, fg.fp);
    }
    if(n >= 8 && memcmp(mg, BIN3_MAGIC, 8) == 0)
    {
        int ver = 0;
        {
            FileGuard fg(fopen(path.c_str(), "rb"));
            if(fg.fp)
            {
                char hdr[BIN3_HDR];
                if(fread(hdr, 1, BIN3_HDR, fg.fp) == BIN3_HDR) memcpy(&ver, hdr + 8, 4);
            }
        }
        // 必须先设 world_name：load_v3() 在 v3 档里会调 save_world() 做升级重写，
        // 而 save_world() 开头就检查 world_name 是否为空，晚了就会直接返回。
        snprintf(world_name, sizeof(world_name), "%s", name);
        bool ok = load_v3(path.c_str(), ver >= BIN4_VERSION);
        if(!ok) { world_name[0] = 0; return false; }
        item::inv_compact();   // 旧档可能把物品放在 37..100，挪回可用槽位
        return true;
    }
    if(n >= 7 && memcmp(mg, BIN_MAGIC, 7) == 0)
    {
        snprintf(world_name, sizeof(world_name), "%s", name);
        if(!load_v1v2(path.c_str())) { world_name[0] = 0; return false; }
        item::inv_compact();
        return true;
    }
    return false;
}

// ============================================================================
//                                新建世界
// ============================================================================
namespace {
void gen_progress(const char *stage, f32 ratio, void *ud)
{
    (void)ud;
    platform::log("生成: %s (%.0f%%)", stage, (double)(ratio * 100));
    // 主循环里由主线程刷新，这里只写日志；生成期间界面会短暂无响应
}

}   // namespace

void new_world(const char *name, u64 seed)
{
    init();
    snprintf(world_name, sizeof(world_name), "%s", name);
    // seed == 0 表示「随便来一张图」，否则用给定种子（可复现）
    world::rng_seed(seed ? seed : (u64)platform::now_seconds() * 1000000.0);
    item::box_reset();
    world::generate(gen_progress, nullptr);
    // generate() 内部已经通过 spawn_chest() 填好了箱子，不要在这里再 reset

    // 把玩家放到世界中心的**干燥**地表出生点。
    // 旧版 MAP() 只是在世界中心往下找第一块能站的地面，如果那一列正好是
    // 湖/海/岩浆，玩家就直接出生在水里了。这里改成从世界中心向两边找
    // 「头顶两格是空气、脚下是固体、且不是液体」的列。
    {
        Player &p = st().pl;
        bool found = find_dry_spawn(world::back_x, p.x, p.y);
        if(p.y < 2) p.y = 2;
        if(p.x < 2) p.x = 2;
        if(p.x > world::W - 3) p.x = world::W - 3;
        world::back_x = (i32)p.x;
        world::back_y = (i32)p.y + 2;   // 脚下的地面方块（脚在 y+1，地面在 y+2）
        p.jx = p.x;
        p.jy = p.y;
        p.lx = (i32)(p.x + 0.5);
        p.ly = (i32)(p.y + 0.5);
        p.fell = p.y;
        p.spx = 0;
        p.spy = 0;
        p.oxygen = 900;
        world::g_light_cx = p.lx;
        world::g_light_cy = p.ly;
        {
            i32 fx = (i32)p.x, fy = (i32)p.y;
            // 报告实际状态：本体格 / 脚下 / 脚下2 / 头顶，便于确认没卡墙没泡水
            platform::log("出生点: %s  x=%d y=%d  本体=%s 脚下=%s 脚下2=%s 头顶=%s",
                          found ? "干燥地表" : "兜底", fx, fy,
                          block_of(world::s[fx][fy]).cn,
                          block_of(world::s[fx][fy + 1]).cn,
                          block_of(world::s[fx][fy + 2]).cn,
                          block_of(world::s[fx][fy - 1]).cn);
        }
    }

    world::mark_all_dirty();
    // 给一点初始物资，避免开局手无寸铁（旧版没有，属于体验优化）
    item::inv_add(B_WOOD_PICK, 1);
    item::inv_add(B_STONE_SWORD, 1);
    item::inv_add(B_TORCH, 16);
    item::inv_add(B_APPLE, 4);
    armor_update();
    light_update();
    save_world(true);
}

}   // namespace game








