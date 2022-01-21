/*
 * Name: Zhichun Zhao
 * AndrewID: zhichun2
 * File Overview:
 * This file contains a cache simulator that takes in traces of instruction
 * in the format of "Operand address, bit", and returns the number of hits,
 * misses, evictions, dirty bytes in cache at the end, and evictions of
 * dirty lines.
 * Design desicions:
 * The cache is simulated using a 2D array, with each row represent a set
 * and the columns represent the block in the cache lines in a set.
 * The instructions are obtained line by line, stored in global variables, and
 * simulated with either load_update or store_upload. Within the update
 * functions, the return values such as hits and misses are modified. A time
 * stamp variable is used to implement the LRU policy.
 */
#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    long dirty;
    long tag;
    long usedTime;
    long valid;
} block;

int opt, s, E, b;
long stamp = 0;
long hit = 0, miss = 0, eviction = 0, dirty_eviction = 0, dirty_bytes = 0;
block ***cache = NULL;

void initialize() {
    // build a 2D array to simulate the set
    cache = calloc((1 << s), sizeof(block **));
    for (int i = 0; i < (1 << s); i++) {
        cache[i] = calloc(E, sizeof(block *));
        for (int j = 0; j < E; j++) {
            cache[i][j] = calloc(1, sizeof(block));
        }
    }
    // fill the initial data in each cell
    for (int i = 0; i < (1 << s); i++) {
        for (int j = 0; j < E; j++) {
            cache[i][j]->valid = 0;
            cache[i][j]->dirty = 0;
            cache[i][j]->tag = -1;
            cache[i][j]->usedTime = 0;
        }
    }
    return;
}

void load_update(unsigned long addr) {
    int s_index = (addr >> b) & ((1 << s) - 1);
    int min_i = -1;
    int min_time = 2147483647;
    unsigned long t_index = addr >> (s + b);
    int location = -1;

    for (int i = 0; i < E; i++) {
        // when data is already in a block
        if ((cache[s_index][i]->valid == 1) &&
            (cache[s_index][i]->tag == t_index)) {
            hit++;
            cache[s_index][i]->usedTime = stamp;
            return;
        }
        if (cache[s_index][i]->valid == 0) {
            location = i;
        }
        // LRU
        if (cache[s_index][i]->usedTime <= min_time) {
            min_time = cache[s_index][i]->usedTime;
            min_i = i;
        }
    }
    // when data is not yet in the block and at least one block is empty

    if (location != -1) {
        miss++;
        cache[s_index][location]->tag = t_index;
        cache[s_index][location]->valid = 1;
        cache[s_index][location]->usedTime = stamp;
        return;
    }

    // when data is not yet in the block and all the blocks are full
    eviction++;
    miss++;
    if (cache[s_index][min_i]->dirty == 1) {
        dirty_eviction++;
    }
    cache[s_index][min_i]->tag = t_index;
    cache[s_index][min_i]->usedTime = stamp;
    cache[s_index][min_i]->valid = 1;
    cache[s_index][min_i]->dirty = 0;
    return;
}

void store_update(unsigned long addr) {
    int s_index = (addr >> b) & ((1 << s) - 1);
    int min_i = -1;
    int min_time = 2147483647;
    unsigned long t_index = addr >> (s + b);
    int location = -1;
    // when data is already in a block
    for (int i = 0; i < E; i++) {
        if ((cache[s_index][i]->valid == 1) &&
            (cache[s_index][i]->tag == t_index)) {
            hit++;
            cache[s_index][i]->usedTime = 0;
            cache[s_index][i]->dirty = 1;
            return;
        }
        if (cache[s_index][i]->valid == 0) {
            location = i;
        }
        // lru
        if (cache[s_index][i]->usedTime <= min_time) {
            min_time = cache[s_index][i]->usedTime;
            min_i = i;
        }
    }
    // when data is not yet in the block and at least one block is empty
    if (location != -1) {
        miss++;
        cache[s_index][location]->tag = t_index;
        cache[s_index][location]->valid = 1;
        cache[s_index][location]->dirty = 1;
        cache[s_index][location]->usedTime = 0;
        return;
    }

    // when data is not yet in the block and all the blocks are full
    eviction++;
    miss++;
    if (cache[s_index][min_i]->dirty == 1) {
        dirty_eviction++;
    }
    cache[s_index][min_i]->tag = t_index;
    cache[s_index][min_i]->usedTime = stamp;
    cache[s_index][min_i]->valid = 1;
    cache[s_index][min_i]->dirty = 1;
    return;
}

// at the end of simulation, count the number of dirty bits that is set
long calc_dirty_bytes() {
    long res = 0;
    for (int i = 0; i < (1 << s); i++) {
        for (int j = 0; j < E; j++) {
            if (cache[i][j]->dirty == 1) {
                res++;
            }
        }
    }
    return res;
}

int main(int argc, char *argv[]) {
    char op;
    FILE *file = NULL;
    unsigned long addr;
    int size;
    char *buffer = malloc(sizeof(char) * 50);
    while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (opt) {
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':

            file = fopen(optarg, "r");
            break;
        default:
            break;
        }
    }
    initialize();
    if (file == NULL) {
        printf("file is NULL");
        exit(-1);
    }

    while (fgets(buffer, 50, file)) // get a whole line
    {
        int x = sscanf(buffer, "%c %lx,%d", &op, &addr, &size);
        if ((x) != 3) {
            printf("%d\n", x);
            printf("%s\n", buffer);
            printf("%lx\n", addr);
            printf("scanning failed\n");
            exit(-1);
        }
        switch (op) {
        case 'L':
            load_update(addr);
            break;
        case 'S':
            store_update(addr);
            break;
        }
        stamp++;
    }
    long num_dirty_blocks = calc_dirty_bytes();
    dirty_bytes = (1 << b) * num_dirty_blocks;
    csim_stats_t *print = (csim_stats_t *)malloc(sizeof(csim_stats_t));

    memset(print, 0, sizeof(csim_stats_t));

    print->hits = hit;

    print->misses = miss;

    print->evictions = eviction;

    print->dirty_evictions = dirty_eviction * (1 << b);

    print->dirty_bytes = dirty_bytes;

    printSummary(print);
    for (int i = 0; i < (1 << s); i++) {
        free(cache[i]);
    }
    free(cache);
    free(buffer);
    fclose(file);
    return 0;
}
