// 诊断旧版 v1/v2 存档的字节布局
// 用法: g++ -o probe.exe tools/probe_save.cpp && probe.exe <file.bin>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>

static constexpr int W = 10000, H = 1001;
static constexpr int OLD_W = 5001, OLD_H = 1001;

int probe_save_main(int argc, char **argv)
{
    if(argc < 2) { printf("用法: probe <file.bin>\n"); return 1; }
    FILE *fp = fopen(argv[1], "rb");
    if(!fp) { printf("打不开 %s\n", argv[1]); return 1; }
    fseek(fp, 0, SEEK_END);
    long long sz = _ftelli64(fp);
    fseek(fp, 0, SEEK_SET);
    printf("文件 %s  大小 %lld 字节\n\n", argv[1], sz);

    unsigned char hdr[64] = {0};
    fread(hdr, 1, 64, fp);
    printf("前 64 字节:\n");
    for(int i = 0; i < 64; i += 16)
    {
        printf("  %3d: ", i);
        for(int j = 0; j < 16; j++) printf("%02X ", hdr[i + j]);
        printf("  |");
        for(int j = 0; j < 16; j++)
        {
            unsigned char c = hdr[i + j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("|\n");
    }

    printf("\nmagic[0..6] = '%.7s'  (2DMCBIN=旧版裸结构)\n", hdr);
    int v_at7 = 0, v_at8 = 0;
    memcpy(&v_at7, hdr + 7, 4);
    memcpy(&v_at8, hdr + 8, 4);
    printf("version 若在偏移 7  = %d\n", v_at7);
    printf("version 若在偏移 8  = %d\n", v_at8);
    int w_at11 = 0, h_at15 = 0, w_at12 = 0, h_at16 = 0;
    memcpy(&w_at11, hdr + 11, 4);
    memcpy(&h_at15, hdr + 15, 4);
    memcpy(&w_at12, hdr + 12, 4);
    memcpy(&h_at16, hdr + 16, 4);
    printf("dim 若 v2 (w@11,h@15) = %d x %d\n", w_at11, h_at15);
    printf("dim 若 v2 (w@12,h@16) = %d x %d\n", w_at12, h_at16);

    // 按 v1 布局推算各段大小，看哪一组能对上文件总大小
    printf("\n=== 按 v1 布局（固定 5001x1001，version@7）推算 ===\n");
    struct Layout
    {
        const char *name;
        long long hdr_bytes;      // magic+version（v1 无维度字段）
        int w, h;
        bool ep_bug;
    };
    const Layout lays[] = {
        {"v1: 7+4 头, ep 写 W*W（bug）", 7 + 4, OLD_W, OLD_H, true},
        {"v2: 7+4+4+4 头, ep 正常",     7 + 4 + 4 + 4, 0, 0, false},
    };
    for(const Layout &L : lays)
    {
        int w = L.w ? L.w : w_at11;
        int h = L.h ? L.h : h_at15;
        if(w <= 0 || w > 100000 || h <= 0 || h > 100000)
        {
            printf("%s -> 维度读不出来，跳过\n", L.name);
            continue;
        }
        long long terr = (long long)w * h * 2 * 2;      // s + s2
        long long beibao = 101 * 2 * 4;
        long long scalars = 4 + 8 + 8 + 4 + 4 + 8 + 4 * 4;   // weidu,x,y,bx,by,life,now_time,night,wn,wl
        long long ents = (long long)500 * 152 * 2;
        long long wear_ed = 6 * 4 + 4;
        long long ep = L.ep_bug ? (long long)w * w : (long long)w * h;
        long long boxhdr = 4;
        long long boss2 = 8;
        long long boimes = (long long)w * 2 * 2;
        long long sum = L.hdr_bytes + terr + beibao + scalars + ents + wear_ed + ep +
                        boxhdr + boss2 + boimes;
        printf("%s\n  w=%d h=%d 头=%lld 地形=%lld 背包=%lld 标量=%lld 实体=%lld "
               "ep=%lld 箱头=%lld boss2=%lld boimes=%lld\n  合计(不含箱体) = %lld   与文件差 %lld\n",
               L.name, w, h, L.hdr_bytes, terr, beibao, scalars, ents, ep, boxhdr,
               boss2, boimes, sum, sz - sum);
    }
    fclose(fp);
    return 0;
}
