/*
name: Chaoqin Li
cnetID: chaoqin
*/
#include "cachelab.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "getopt.h"
#include "unistd.h"
#include <string.h>
typedef struct cache_block
{
  bool valid;
  unsigned long tag;
  unsigned ref_t;
} block;
unsigned s, E, b, B;
char *file;
// Trace file.
FILE *fp;
// Cache data structure.
block ** cache;
unsigned long set_mask;
unsigned long tag_mask;
// A global clock.
unsigned clock = 0;
// Keep track of hit, miss and eviction.
unsigned hit_cnt = 0, miss_cnt = 0, evict_cnt = 0;
unsigned long get_tag(unsigned long addr) {
    return addr >> (s + b);
}
// Get the set number of a given address.
unsigned get_set(unsigned long addr) {
    return (addr >> b) & set_mask;
}
// Update a block, set valid bit, tag number, and time stamp.
void update_block(block* b, unsigned long tag) {
    b->valid = true;
    b->tag = tag;
    b->ref_t = clock;
}
// Find a block to evict and return ptr to that block.
block *evict_block(block *set) {
    evict_cnt++;
    unsigned t_min = 0xffffffff;
    int idx = -1;
    for (int i = 0; i < E; i++) {
            if (set[i].ref_t < t_min) {
                    t_min = set[i].ref_t;
                    idx = i;
            }
    }
    return &set[idx];
}
// Find a block that corresponds to a tag in a given set.
block *find_block(block* set, unsigned long tag) {
    for (int i = 0; i < E; i++) {
            if (set[i].valid == false) continue;
            if (set[i].tag == tag) return &set[i];
    }
    return NULL;
}
// Find a block to bring something into cache.
block *get_block(block *set) {
    for (int i = 0; i < E; i++) {
            if (set[i].valid == false) return &set[i];
    }
    return evict_block(set);
}
// return true when there is a cache hit, false if there is a cache miss.
bool access_cache(block *set, unsigned long tag) {
    clock++;
    block *b = find_block(set, tag);
    if (b != NULL)  {
            hit_cnt++;
            update_block(b, tag);
            return true;
    }
    miss_cnt++;
    b = get_block(set);
    update_block(b, tag);
    return false;
}
// A data load operation.
void data_load(unsigned long addr) {
    unsigned long tag = get_tag(addr);
    unsigned set_id = get_set(addr);
    block *set = cache[set_id];
    access_cache(set, tag);
}
// A data store operation.
void data_store(unsigned long addr) {
    unsigned tag = get_tag(addr);
    unsigned set_id = get_set(addr);
    block *set = cache[set_id];
    access_cache(set, tag);
}
// Go through the trace file to 
void process_file (const char* file) {
    fp = fopen(file, "r");
    int bufferLength = 25;
    char buffer[bufferLength];
    char temp[bufferLength];
    while(fgets(buffer, bufferLength, fp)) {
            if (buffer[0] == 'I') continue;
            for (int i = 0; i < 25; i++) if (buffer[i] == ',') buffer[i] = ' ';
            strcpy(temp, buffer);
            char * type = strtok(temp, " ");
    unsigned long addr;
    addr = strtol(strtok(NULL, " "), NULL, 16);
    //sscanf(strtok(NULL, " "), "%x", &addr);
    unsigned sz;
    sscanf(strtok(NULL, " "), "%d", &sz);
    switch (type[0]) {
            case 'S':
                    data_store(addr);
                    break;
            case 'L':
                    data_load(addr);
                    break;
            case 'M':
                    data_load(addr);
                    hit_cnt++;
                    break;
            default:
                    break;
    }
    }
    fclose(fp);
}
// Initialize the cache data structure.
void cache_init() {
    int S = (1 << s);
    cache = (block **)malloc(S * sizeof(int *));
    for (int i = 0; i < S; i++) cache[i] = (block *)malloc(E * sizeof(block));
    for (int i = 0; i < S; i++)
            for (int j = 0; j < E; j++) cache[i][j].valid = false;
}
// Free the cache data structure.
void cache_free() {
    int S = (1 << s);
    for (int i = 0; i < S; i++) free(cache[i]);
    free(cache);
}
// Initialize the mask used to get tag number and set number.
void mask_init() {
    tag_mask = 1;
    int tag_bits = sizeof(unsigned long) * 8 - (b + s);
    for (int i = 1; i < tag_bits; i++) {
            tag_mask |= (tag_mask << 1);
    }
    set_mask = 1;
    for (int i = 1; i < s; i++) {
            set_mask |= (set_mask << 1);
    }
}
void parse_param(int argc, char *argv[]) {
    static const char *optString = "hvs:E:b:t:";
    int opt = 0;
    opt = getopt(argc, argv, optString);
    while (opt != -1) {
        switch (opt) {
            case 'h':
                break;
            case 'v':
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
                        case 'b':
                                b = atoi(optarg);
                                B = 1 << b;
                break;
            case 't':
                file = optarg;
            default:
                break;
        }
        opt = getopt(argc, argv, optString);
    }
}
int main(int argc, char *argv[])
{
    parse_param(argc, argv);
    mask_init();
    cache_init(s, E);
    process_file(file);
    cache_free();
    printSummary(hit_cnt, miss_cnt, evict_cnt);
    return 0;
}
