
struct bbinfoloc {
        u_int64_t       magic1;
        u_int64_t       start;
        u_int64_t       end;
        u_int64_t       magic2;
};

struct bbinfo {
        int32_t         cksum;
        int32_t         nblocks;
        int32_t         bsize;
        int32_t         blocks[1];
};
