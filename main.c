//
//  main.c
//  main
//
//  Created by Ye Joon Ko on 23/11/2019.
//  Copyright © 2019 Ye Joon Ko. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for getopt()

#define BYTES_PER_WORD 4
// #define DEBUG

/*
 * Cache structures
 */
int time = 0;

typedef struct {
    int age;
    int valid;
    int modified;
    int flag2;
    uint32_t tag;
} cline;

typedef struct {
    cline *lines;
} cset;

typedef struct {
    int s;
    int E;
    int b;
    cset *sets;
} cache;

static int index_bit(int n) {
    int cnt = 0;
    while(n){
        cnt++;
        n = n >> 1;
    }
    return cnt-1;
}

//hex to binary char
char* NumToBits(unsigned int long num, int len) {
    char* bits = (char*)malloc(len + 1);
    for (int k = 0; k < len + 1; k++) {
        bits[k] = 0;
    }
    int idx = len - 1, i;
    
    while (num > 0 && idx >= 0) {
        if (num % 2 == 1) {
            bits[idx--] = '1';
        }
        else {
            bits[idx--] = '0';
        }
        num /= 2;
    }
    
    for (i = idx; i >= 0; i--) {
        bits[i] = '0';
    }
    
    return bits;
}

void build_cache(cache*newcache,int nset, int nway) {
    cset*aSet;
    cline*aWay;
    aSet=(cset*)malloc(sizeof(cset)*nset);
    for(int i=0;i<nset;i++){
        aWay=(cline*)malloc(sizeof(cline)*nway);
        aSet[i].lines=aWay;
        for(int j=0;j<nway;j++){
            aSet[i].lines[j].age=0;
            aSet[i].lines[j].modified=0;
            aSet[i].lines[j].tag=0;
            aSet[i].lines[j].valid=0;
            aSet[i].lines[j].flag2=0;
        }
    }
    newcache->sets=aSet;
    //cache[set][assoc][word per block]
}

void access_cache(cache*accCache,int index, int tag, int type,int*flag2,int*writeback,int nset, int nway) {
    int flag=0;
    int lastNum=0;
    int dupNum=0;
    for(int i=0;i<nway;i++){
        if(accCache->sets[index].lines[i].valid==1){
            flag=1;
            lastNum=i;
            if(accCache->sets[index].lines[i].tag==tag){
                *flag2=1;
                dupNum=i;
            }
        }
    }
    if(flag==1 && *flag2==0){
        if(lastNum==accCache->E-1){
            int max=0;
            int maxind=0;
            for(int j=0;j<nway;j++){
                if(max<accCache->sets[index].lines[j].age) {
                    max=accCache->sets[index].lines[j].age;
                    maxind=j;
                }
            }
            if(accCache->sets[index].lines[maxind].modified==1){
                *writeback=*writeback+1;
            }
            accCache->sets[index].lines[maxind].age=0;
            accCache->sets[index].lines[maxind].tag=tag;
            accCache->sets[index].lines[maxind].valid=1;
            if(type==0){
                accCache->sets[index].lines[maxind].modified=0;
            }
            else{
                accCache->sets[index].lines[maxind].modified=1;
            }
        }
        else{
            accCache->sets[index].lines[lastNum+1].age=0;
            accCache->sets[index].lines[lastNum+1].tag=tag;
            accCache->sets[index].lines[lastNum+1].valid=1;
            if(type==0){
                accCache->sets[index].lines[lastNum+1].modified=0;
            }
            else{
                accCache->sets[index].lines[lastNum+1].modified=1;
            }
        }
    }
    else if(flag==0){
        accCache->sets[index].lines[0].age=0;
        accCache->sets[index].lines[0].tag=tag;
        accCache->sets[index].lines[0].valid=1;
        if(type==0){
            accCache->sets[index].lines[0].modified=0;
        }
        else{
            accCache->sets[index].lines[0].modified=1;
        }
    }
    else if(*flag2==1){
        accCache->sets[index].lines[dupNum].age=0;
        if(type==1){
            accCache->sets[index].lines[dupNum].modified=1;
        }
    }
    
    for(int i=0;i<nset;i++){
        for(int j=0;j<nway;j++){
            if(accCache->sets[i].lines[j].valid==1){
                accCache->sets[i].lines[j].age=accCache->sets[i].lines[j].age+1;
            }
        }
    }
}


/***************************************************************/
/*                                                             */
/* Procedure : cdump                                           */
/*                                                             */
/* Purpose   : Dump cache configuration                        */
/*                                                             */
/***************************************************************/
void cdump(int capacity, int assoc, int blocksize){
    
    printf("Cache Configuration:\n");
    printf("-------------------------------------\n");
    printf("Capacity: %dB\n", capacity);
    printf("Associativity: %dway\n", assoc);
    printf("Block Size: %dB\n", blocksize);
    printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : sdump                                           */
/*                                                             */
/* Purpose   : Dump cache stat                                   */
/*                                                             */
/***************************************************************/
void sdump(int total_reads, int total_writes, int write_backs,
           int reads_hits, int write_hits, int reads_misses, int write_misses) {
    printf("Cache Stat:\n");
    printf("-------------------------------------\n");
    printf("Total reads: %d\n", total_reads);
    printf("Total writes: %d\n", total_writes);
    printf("Write-backs: %d\n", write_backs);
    printf("Read hits: %d\n", reads_hits);
    printf("Write hits: %d\n", write_hits);
    printf("Read misses: %d\n", reads_misses);
    printf("Write misses: %d\n", write_misses);
    printf("\n");
}


/***************************************************************/
/*                                                             */
/* Procedure : xdump                                           */
/*                                                             */
/* Purpose   : Dump current cache state                        */
/*                                                                */
/* Cache Design                                                   */
/*                                                             */
/*         cache[set][assoc][word per block]                       */
/*                                                               */
/*                                                             */
/*       ----------------------------------------               */
/*       I        I  way0  I  way1  I  way2  I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set0  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set1  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*                                                             */
/*                                                             */
/***************************************************************/
void xdump(cache* L)
{
    int i,j,k = 0;
    int b = L->b, s = L->s;
    int way = L->E, set = 1 << s;
    int E = index_bit(way);
    
    uint32_t line;
    
    printf("Cache Content:\n");
    printf("-------------------------------------\n");
    for(i = 0; i < way;i++)
    {
        if(i == 0)
        {
            printf("    ");
        }
        printf("      WAY[%d]",i);
    }
    printf("\n");
    
    for(i = 0 ; i < set;i++)
    {
        printf("SET[%d]:   ",i);
        for(j = 0; j < way;j++)
        {
            if(k != 0 && j == 0)
            {
                printf("          ");
            }
            if(L->sets[i].lines[j].valid){
                line = L->sets[i].lines[j].tag << (s+b);
                line = line|(i << b);
            }
            else{
                line = 0;
            }
            printf("0x%08x  ", line);
        }
        printf("\n");
    }
    printf("\n");
}




int main(int argc, char *argv[]) {
    int i, j, k;
    int capacity=1024;
    int way=8;
    int blocksize=8;
    int set;
    
    /*
     int tag_num=0;
     int index_num=0;
     int offset_num=0;
     */
    //cache
    cache simCache;
    
    // counts
    int read=0, write=0, writeback=0;
    int readhit=0, writehit=0;
    int readmiss=0, writemiss = 0;
    
    // Input option
    int opt = 0;
    char* token;
    int xflag = 0;
    
    // parse file
    char *trace_name = (char*)malloc(32);
    FILE *fp;
    char line[16]={0};
    char *op;
    uint32_t addr;
    
    /* You can define any variables that you want */
    
    trace_name = argv[argc-1];
    if (argc < 3) {
        printf("Usage: %s -c cap:assoc:block_size [-x] input_trace \n",argv[0]);
        exit(1);
    }
    while((opt = getopt(argc, argv, "c:x")) != -1){
        switch(opt){
            case 'c':
                // extern char *optarg;
                token = strtok(optarg, ":");
                capacity = atoi(token);
                token = strtok(NULL, ":");
                way = atoi(token);
                token = strtok(NULL, ":");
                blocksize  = atoi(token);
                break;
            case 'x':
                xflag = 1;
                break;
            default:
                printf("Usage: %s -c cap:assoc:block_size [-x] input_trace \n",argv[0]);
                exit(1);
                
        }
    }
    
    // allocate
    set = capacity/way/blocksize; //Cache set num
    
    simCache.s=index_bit(set);
    simCache.E=way;
    simCache.b=index_bit(blocksize);
    
    /*
     index_num=set;
     offset_num=index_bit(blocksize);
     tag_num=32-(index_num+offset_num);
     */
    /* TODO: Define a cache based on the struct declaration */
    build_cache(&simCache,set,way);
    //simCache = build_cache();
    
    // simulate
    fp = fopen(trace_name, "r"); // read trace file
    if(fp == NULL){
        printf("\nInvalid trace file: %s\n", trace_name);
        return 1;
    }
    cdump(capacity, way, blocksize);
    
    /* TODO: Build an access function to load and store data from the file */
    while (fgets(line, sizeof(line), fp) != NULL) {
        op = strtok(line, " ");
        addr = strtoull(strtok(NULL, ","), NULL, 16);
        char*add = NumToBits(addr, 32);
        int index=0;
        int tag=0;
        char ctag[33]="00000000000000000000000000000000";
        char cindex[33]="00000000000000000000000000000000";
        int n1= simCache.b;
        int n2=simCache.s;
        for(int j=32-(n1+n2);j<32-n1;j++){
            cindex[j+n1]=*(add+j);
        }
        index=strtoull(cindex,NULL,2);
        
        for(int j=0;j<32-n1-n2;j++){
            ctag[j+n1+n2]=*(add+j);
        }
        tag=strtoull(ctag, NULL, 2);
        
        //Read case
        int flag=0;
        
        if(strcmp(op,"R")==0){
            access_cache(&simCache,index,tag,0,&flag,&writeback,set,way);
            read=read+1;
            if(flag==1){
                readhit=readhit+1;
            }
            else{
                readmiss=readmiss+1;
            }
        }
        //Write case
        else{
            access_cache(&simCache,index,tag,1,&flag,&writeback,set,way);
            write = write +1;
            if(flag==1){
                writehit=writehit+1;
            }
            else{
                writemiss=writemiss+1;
            }
            //write_back, write_hit, write_miss
        }
        
#ifdef DEBUG
        // You can use #define DEBUG above for seeing traces of the file.
        fprintf(stderr, "op: %s\n", op);
        fprintf(stderr, "addr: %x\n", addr);
#endif
        
        // ...
        // access_cache()
        // ...
    }
    
    // test example
    sdump(read, write, writeback, readhit, writehit, readmiss, writemiss);
    if (xflag){
        xdump(&simCache);
    }
    
    
    return 0;
}


