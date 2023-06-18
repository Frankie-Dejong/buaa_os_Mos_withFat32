#include "serv.h"
#include <mmu.h>
#include "time.h"

struct FAT_Super fat_super;

u_int SECT2CLUS;

struct FSInfo *fs_info;

uint32_t *FAT1, *FAT2;

u_int FAT1_ClusNo;


void setTime(struct Fat32_Dir *cur, int create) {
    mytime_struct timer;
    u_int us;
    utc_sec_2_mytime(get_time(&us) + 8 * SEC_PER_HOUR - 8 * SEC_PER_MIN, &timer, 0);
    if(create) {
        cur->CrtDate = ((timer.nYear - 1980) << 9) | (timer.nMonth << 5) | (timer.nDay);
        cur->CrtTime = (timer.nHour << 11) | ((timer.nMin) << 5) | (timer.nSec >> 1);
    }
    cur->WrtDate = ((timer.nYear - 1980) << 9) | (timer.nMonth << 5) | (timer.nDay);
    cur->WrtTime = (timer.nHour << 11) | ((timer.nMin) << 5) | (timer.nSec >> 1);
}



int bytencmp(const void *a, const void *b, u_int n) {
    char *ca = (char *)a;
    char *cb = (char *)b;
    for(int i = 0;i < n;i ++) {
        if(ca[i] != cb[i]) {
            return ca[i] - cb[i];
        }
    }
    return 0;
}

int cmpFileName(char *fs_name, char *user_name) {
    
    char c1[11];
    memset(c1, 32, 11);
    int i;
    for(i = 0; (user_name[i] != '.') && (user_name[i] != 0) && (i < 8);i ++)
    {
        c1[i] = (user_name[i] >= 'a' && user_name[i] <= 'z') ? (user_name[i] - 'a' + 'A') : user_name[i];
    }
    if(user_name[i] == '.') {
        i ++;
        int j;
        for(j = 8;(user_name[i] != 0) && j < 11;i ++)
        {
            c1[j ++] = user_name[i];
        }
    }
    return bytencmp(fs_name, c1, 11);
}

int getFileName(char *user_name, char *c1) {
    memset(c1, 32, 11);
    int i;
    for(i = 0; (user_name[i] != '.') && (user_name[i] != 0) && (i < 8);i ++)
    {
        c1[i] = (user_name[i] >= 'a' && user_name[i] <= 'z') ? (user_name[i] - 'a' + 'A') : user_name[i];
    }
    if(user_name[i] == '.') {
        i ++;
        int j;
        for(j = 8;(user_name[i] != 0) && j < 11;i ++)
        {
            c1[j ++] = user_name[i];
        }
    }
    return 0;
}

int fat_get_clus() {
    return BY2SECT * fat_super.FAT_SECPClus;
}

unsigned char ChkSum (unsigned char *pFcbName) 
{ 
    short FcbNameLen; 
    unsigned char Sum; 
 
    Sum = 0; 
    for (FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) { 
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++; 
    } 
    return (Sum); 
} 

int cmpShortEntry(char *user_name, char *c1) {
    char tmp[11];
    getFileName(user_name, tmp);
    for(int i = 0;i < 6;i ++) {
        if(tmp[i] != c1[i]) {
            return 1;
        }
    }
    for(int i = 8;i < 11;i ++) {
        if(tmp[i] != c1[i]) {
            return 1;
        }
    }
    return 0;
}


int getFileName_(char *user_name, char *c1, struct Fat32_Dir *dir) {
    memset(c1, 32, 11);
    int i;
    for(i = 0; (user_name[i] != '.') && (user_name[i] != 0) && (i < 8);i ++)
    {
        c1[i] = (user_name[i] >= 'a' && user_name[i] <= 'z') ? (user_name[i] - 'a' + 'A') : user_name[i];
    }
    if(user_name[i] == '.') {
        i ++;
        int j;
        for(j = 8;(user_name[i] != 0) && j < 11;i ++)
        {
            c1[j ++] = user_name[i];
        }
    }
    c1[6] = '~';
    c1[7] = '1' + fat_dir_lookup_all(dir, user_name);
    return 0;
}

int strncpy(char *dst, char *src, int n) {
    for(int i = 0;i < n;i ++) {
        dst[i] = src[i];
    }
    return 0;
}

void *alignWithPage(u_int va) {
    return (void *)((va / BY2PG) * BY2PG);
}


void *fat_diskaddr(u_int clusno) {
    if(clusno * BY2CLUS >= FATDISKMAX) {
        user_panic("fat blockno overflowed! clusno: %d\n", clusno);
    }
    return (void *)(FATDISKMAP + clusno * BY2CLUS);
}

int isData(u_int clusno) {
    u_int rootSec = fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int rootClus = rootSec / SECT2CLUS;
    return clusno >= rootClus && clusno < fat_super.TotClus;
}

int Clusno2Data(u_int clusno) {
    if(!isData(clusno)) {
        return -E_INVAL;
    }
    u_int rootSec = fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int rootClus = rootSec / SECT2CLUS;
    return clusno - rootClus + fat_super.RootClus;
}

int Data2ClusNO(u_int dataClusNO) {
    u_int rootSec = fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int rootClus = rootSec / SECT2CLUS;
    return dataClusNO + rootClus - fat_super.RootClus;
}

int va2ClusNo(void *va) {
    return ((u_int)va - FATDISKMAP) / BY2CLUS;
}

u_int alignWithClus(u_int clusno) {
    void *va = alignWithPage(fat_diskaddr(clusno));
    return ((u_int)va - FATDISKMAP) / BY2CLUS;
}


int fat_clus_is_dirty(u_int dataClusNO) {
    u_int clusno = Data2ClusNO(dataClusNO);
    void *va = fat_diskaddr(clusno);
    return va_is_mapped(va) && va_is_dirty(va);
}

int dirty_cluster(u_int clusno) {
    void *va = fat_diskaddr(clusno);
    void *alignedVa = alignWithPage(va);
    if(!va_is_mapped(va)) {
        return -E_NOT_FOUND;
    }

    if(va_is_dirty(va)) {
        return 0;
    }

    write_cluster(clusno);

    return syscall_mem_map(0, alignedVa, 0, alignedVa, PTE_D | PTE_DIRTY);
}

int dirty_Fat(u_int dataClusNo) {
    write_cluster(FAT1_ClusNo + (dataClusNo * 4) / BY2CLUS);
    return 0;
}

void write_cluster(u_int clusno) {
    void *va = fat_diskaddr(clusno);
    void *alignedVa = alignWithPage(va);
    if(!va_is_mapped(va)) {
        user_panic("write unmapped cluster 0x%08x", clusno);
    }
    ide_write(1, clusno * SECT2CLUS, va, SECT2CLUS);
}

void write_clusters_in_page(u_int clusno) {
    void *va = fat_diskaddr(clusno);
    void *alignedVa = alignWithPage(va);
    if(!va_is_mapped(va)) {
        user_panic("write unmapped cluster 0x%08x", clusno);
    }
    ide_write(1, alignWithClus(clusno) * SECT2CLUS, alignedVa, BY2PG / BY2SECT);
}



void *fat_clus_is_mapped(u_int clusno) {
    void *va = fat_diskaddr(clusno);
    if(va_is_mapped(va)) {
        return va;
    }
    return NULL;
}

int fat_clus_is_free(u_int dataClusNO) {
    user_assert(dataClusNO >= 2);
    u_int secno = (dataClusNO - 2) * SECT2CLUS +fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int clusno = secno / SECT2CLUS;
    if((fat_clus_is_mapped(0) == NULL) || clusno >= fat_super.TotClus) {
        return 0;
    }
    return (FAT1[dataClusNO] == 0);
} 

void fat_free_cluster(u_int dataClusNO) {
    u_int clusno = Data2ClusNO(dataClusNO);
    if(clusno >= fat_super.TotClus) {
        return;
    }
    user_assert(FAT1 != NULL && fs_info != NULL);
    FAT1[dataClusNO] = 0;
    dirty_Fat(dataClusNO);
    fs_info->FSI_FreeCount += 1;
    fs_info->FSI_NxtFree = (fs_info->FSI_NxtFree < dataClusNO) ? fs_info->FSI_NxtFree : dataClusNO;
}

int fat_alloc_cluster_num() {
    user_assert(fs_info != NULL);
    user_assert(fs_info->FSI_FreeCount > 0);
    fs_info->FSI_FreeCount --;
    int res = fs_info->FSI_NxtFree;
    FAT1[res] = EOF_Fat32;
    dirty_Fat(res);
    u_int rootClus = Data2ClusNO(2);
    int i;
    for(i = res;i < fat_super.TotClus - rootClus;i ++) {
        if(fat_clus_is_free(i)) {
            fs_info->FSI_NxtFree = i;
            break;
        }
    }
    user_assert(i < (fat_super.TotClus - rootClus));
    return res;
}

int fat_alloc_cluster() {
    int r, num;
    r = fat_alloc_cluster_num();
    user_assert(fat_clus_is_free(r) == 0);
    num = r;
    r = fat_map_cluster(Data2ClusNO(num));
    if(r < 0) {
        fat_free_cluster(num);
        return r;
    }
    return num;
}



int fat_map_cluster(u_int clusno) {
    if(fat_clus_is_mapped(clusno) != NULL) {
        return 0;
    }
    return syscall_mem_alloc(0, alignWithPage(fat_diskaddr(clusno)), PTE_V | PTE_D);
}


void fat_unmap_cluster(u_int dataClusNO) {
    u_int clusno = Data2ClusNO(dataClusNO);
    void *va = fat_clus_is_mapped(clusno);
    user_assert(va != NULL);
    if(fat_clus_is_dirty(dataClusNO)) {
        u_int begin = alignWithClus(clusno);
        u_int dataBegin = Clusno2Data(begin);
        for(int i = 0;i < CLUS2Page;i ++) 
        {
            if(!fat_clus_is_free(dataBegin + i)) {
                write_cluster(begin + i);
            }
        }
    }
    syscall_mem_unmap(0, alignWithPage(va));
    user_assert(fat_clus_is_mapped(clusno) == NULL);
}

//read BPB
void fat_readBPB()
{
    void *va = fat_diskaddr(0);
    if(va_is_mapped(va) == 0) {
        syscall_mem_alloc(0, va, PTE_D);
        ide_read(1, 0, va, BY2PG / BY2SECT);
    }
    char *parameter = (char *)va;
    /*get bytes per sector*/
    fat_super.FAT_BY2SEC = (parameter[12] << 8) + parameter[11];
    /*get file type*/
    strncpy(fat_super.FSType, parameter + 82, 8);
    fat_super.FSType[8] = 0;
    /*get sectors per FAT(number of the clusters)*/
    fat_super.SECPFAT = *((int *)(parameter + 36));
    /*get sectors per cluster*/
    fat_super.FAT_SECPClus = (int)parameter[13];
    SECT2CLUS = fat_super.FAT_SECPClus; 
    /*get number of sectors belongs to Reserved Region*/
    fat_super.numOfReserved = (parameter[15] << 8) + (parameter[14] & 0xff);
    /*get the index of rootCluster*/
    fat_super.RootClus = *((int *)(parameter + 44));
    /*get the total number of blocks*/
    fat_super.TotClus = (*((int *)(parameter + 32))) / SECT2CLUS;
    /*get the number of FAT*/
    fat_super.numOfFats = parameter[16];
    /*check end number*/
    fat_super.FAT_endNumber = (parameter[510] << 8) + (parameter[511] & 0xff);
    
    user_assert(fat_super.FAT_endNumber == FAT_BPB_END_NUMBER);
    user_assert(strcmp(fat_super.FSType, "FAT32   ") == 0);
    debugf("There is 0x%x SECTORS in ONE CLUSTER\n", fat_super.FAT_SECPClus);
    debugf("fat_readBPB is good\n");
    return 0;
}

void read_FSInfo() {
    if(fs_info == NULL) {
        read_cluster(1 / SECT2CLUS, NULL, 0);
        fs_info = (struct FSInfo *)(FATDISKMAP + BY2SECT);
    }
    user_assert(fs_info != NULL);
    user_assert(fs_info->FSI_LeadSig == 0x41615252);
    user_assert(fs_info->FSI_StructSig == 0x61417272);
    user_assert(fs_info->FSI_TrailSig == 0xAA550000);
    u_int rootSec = fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int rootClus = rootSec / SECT2CLUS;
    fs_info->FSI_FreeCount = 0;
    for(int i = 0;i < fat_super.TotClus - rootClus;i ++) {
        if(FAT1[i] == 0) {
            fs_info->FSI_NxtFree = i;
            break;
        }
    }
    for(int i = fs_info->FSI_NxtFree;i < fat_super.TotClus - rootClus;i ++) {
        if(FAT1[i] == 0) {
            fs_info->FSI_FreeCount ++;
        }
    }
    debugf("read_FSInfo is good\n================================\n");
    debugf("Total free Cluster Count:0x%08x\n", fs_info->FSI_FreeCount);
    debugf("Next Available Cluster Number:0x%08x\n================================\n", fs_info->FSI_NxtFree);
}

void read_FAT() {
    u_int i;
    user_assert(fat_super.FAT_endNumber == FAT_BPB_END_NUMBER);
    user_assert(strcmp(fat_super.FSType, "FAT32   ") == 0);
    u_int FAT1ClusNo = fat_super.numOfReserved;
    u_int total = fat_super.FAT_SECPClus / SECT2CLUS;
    if(fat_super.numOfFats == 2) {
        for(i = 0;i < total;i ++) {
            read_cluster(FAT1ClusNo + i, NULL, 0);
            read_cluster(FAT1ClusNo + i + total, NULL, 0);
        }
        FAT1 = (uint32_t *)(fat_diskaddr(FAT1ClusNo));
        FAT2 = (uint32_t *)fat_diskaddr(FAT1ClusNo + total);
        user_assert(FAT1[0] == 0xfffffff8);
    } else {
        for(i = 0;i < total;i ++) {
            read_cluster(FAT1ClusNo + i, NULL, 0);
        }
        FAT1 = (uint32_t *)fat_diskaddr(FAT1ClusNo);
        user_assert(FAT1[0] == 0xfffffff8);
    }
    FAT1_ClusNo = FAT1ClusNo;
    debugf("read_FAT is good\n");
}

// read a cluster by clusterno
int read_cluster(u_int clusno, void **cluster, u_int *isnew) {
    if(clusno >= fat_super.TotClus) {
        user_panic("reading non-existent cluster %08x\n", clusno);
    }
    void *va = fat_diskaddr(clusno);
    if(FAT1) {
        if(isData(clusno) && fat_clus_is_free(Clusno2Data(clusno)))
        {
            user_panic("reading free cluster %08x\n", clusno);
        }
    }
    if(va_is_mapped(va)) {
        if(isnew) {
            *isnew = 0;
        }
    } else {
        if(isnew) {
            *isnew = 1;
        }
        syscall_mem_alloc(0, alignWithPage(va), PTE_D);
        ide_read(1, alignWithClus(clusno), alignWithPage(va), BY2PG / BY2SECT);
    }
    if(cluster) {
        *cluster = va;
    }
    return 0;
}


/* read data regeion by cluster*/
void *read_data_cluster(int dataClusNO) {
    u_int secno = (dataClusNO - 2) * SECT2CLUS + fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int clusno = secno / SECT2CLUS;
    void *data;
    read_cluster(clusno, &data, 0);
    user_assert(data != NULL);
    return data;
}


void check_RW_cluster() {
    char *first_dir = (char *)read_data_cluster(fat_super.RootClus);
    int r = fat_alloc_cluster();
    user_assert(r > 0);
    char buf[BY2CLUS];
    char *test = (char *)read_data_cluster(r);
    strncpy(buf, test, BY2CLUS);
    strncpy(test, first_dir, BY2CLUS);
    write_cluster(Data2ClusNO(r));
    test = (char *)read_data_cluster(r);
    user_assert(bytencmp(first_dir, test, BY2CLUS) == 0);
    strncpy(test, buf, BY2CLUS);
    write_cluster(Data2ClusNO(r));
    user_assert(bytencmp(buf, test, BY2CLUS) == 0);
    fat_free_cluster(r);
    user_assert(fat_clus_is_free(r));
    debugf("read & write cluster is good\n");
}

void fat32_init() {
    fat_readBPB();
    read_FAT();
    read_FSInfo();
    check_RW_cluster();
    debugf("================================\nfat32_init success!\n================================\n");
}

int file_get_dataClusNO(struct Fat32_Dir *file) {
    return ((file->ClusHI[1] << 24) & (0xff << 24)) + ((file->ClusHI[0] << 16) & (0xff << 16)) + ((file->ClusLO[1] << 8) & (0xff << 8)) + ((file->ClusLO[0]) & 0xff);
}

int getSubName(struct Fat32_LDIR *cur, short *s) {
    for(int i = 0;i < 5;i ++) {
        s[i] = cur->LDIR_Name1[i];
    }
    for(int i = 0;i < 6;i ++){
        s[i + 5] = cur->LDIR_Name2[i];
    }
    for(int i = 0;i < 2;i ++){
        s[i + 11] = cur->LDIR_Name3[i];
    }
}

int cmpLongName(struct Fat32_LDIR *cur, char *s) {
    short tmp[13];
    u_int ord;
    if(cur->LDIR_Ord & LAST_LONG_ENTRY) {
        ord = cur->LDIR_Ord ^ LAST_LONG_ENTRY;
    } else {
        ord = cur->LDIR_Ord;
    }
    getSubName(cur, tmp);
    char *p = s + (ord - 1) * 13;
    for(int i = 0;i < 13;i ++) {
        if(p[i] == '\0' && tmp[i] == 0) {
            return 1;
        }
        if((short)p[i] != tmp[i]) {
            return 0;
        }
    }
    return 1;
}


int fat_dir_lookup(struct Fat32_Dir *pdir, char *name, struct Fat32_Dir ** file, struct Fat32_Dir ** entry) {
    u_int dataClusNo = Clusno2Data(va2ClusNo(pdir));
    u_int check = 1;
    short tmp[13];
    do {
        struct Fat32_Dir *dir = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
        check = 1;
        for(int i = 0;i < BY2CLUS / sizeof(struct Fat32_Dir);i ++)
        {
            
            struct Fat32_Dir *cur = dir + i;
            if(cur->Attr & ATTR_LONG_NAME) {
                struct Fat32_LDIR *cur_ = (struct Fat32_LDIR *)cur;
                if(cur_->LDIR_Ord & LAST_LONG_ENTRY) {
                    check = cur_->LDIR_Ord ^ LAST_LONG_ENTRY;
                }
                check -= cmpLongName(cur_, name);
            } else if(cmpFileName(cur->Name, name) == 0 || check == 0) {
                u_int targetClusNO = file_get_dataClusNO(cur);
                *entry = cur;
                if(file != 0) {
                    *file = (struct Fat32_Dir *)read_data_cluster(targetClusNO);
                }                
                return 0;
            }
        }
        dataClusNo = FAT1[dataClusNo];

    } while(dataClusNo != EOF_Fat32);
    return -E_NOT_FOUND;
}

int fat_dir_lookup_all(struct Fat32_Dir *pdir, char *name) {
    u_int dataClusNo;
    if(pdir == NULL) {
        dataClusNo = fat_super.RootClus;
    } else {
        dataClusNo = file_get_dataClusNO(pdir);
    }
    if(dataClusNo == 0) {
        return 0;
    }
    u_int shot = 0;
    do {
        struct Fat32_Dir *dir = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
        for(int i = 0;i < BY2CLUS / sizeof(struct Fat32_Dir);i ++)
        {
            struct Fat32_Dir *cur = dir + i;
            if(cur->Attr & ATTR_LONG_NAME) {
                continue;
            }
            if(cmpShortEntry(name, cur->Name) == 0) {
                shot ++;
            }
        }
        dataClusNo = FAT1[dataClusNo];
    } while(dataClusNo != EOF_Fat32);
    return shot;
}



int fat_free_dir_lookup(struct Fat32_Dir *pdir, struct Fat32_Dir **entry, char *this) {
    u_int dataClusNo = Clusno2Data(va2ClusNo(pdir));
    do {
        struct Fat32_Dir *dir = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
        for(int i = 0;i < BY2CLUS / sizeof(struct Fat32_Dir);i ++)
        {
            struct Fat32_Dir *cur = dir + i;
            if(cur->Attr == ATTR_LONG_NAME) {
                continue;
            }
            if(cmpFileName(cur->Name, this) == 0) {
                return 0;
            }
            if(cur->Name[0] == '\0') {
                u_int dataClusNO = file_get_dataClusNO(cur);
                *entry = cur;
                return 0;
            }
        }
        dataClusNo = FAT1[dataClusNo];
    } while(dataClusNo != EOF_Fat32);
    return -E_NOT_FOUND;
}

int fat_total_clus(struct Fat32_Dir *dir) {
    int i = 0;
    u_int dataClusNo;
    if(dir == NULL) {
        dataClusNo = fat_super.RootClus;
    } else {
        dataClusNo = file_get_dataClusNO(dir);
    }
    
    while(FAT1[dataClusNo] != EOF_Fat32) {
        dataClusNo = FAT1[dataClusNo];
        i ++;
    }

    return i;
}


int fat_walk_cluster(struct Fat32_Dir *f, u_int fClusNo, u_int *pClusNo, u_int alloc) {
    int i;
    u_int dataClusNo;
    if(f == NULL) {
        dataClusNo = fat_super.RootClus;
    } else {
        dataClusNo = file_get_dataClusNO(f);
    } 
    
    for(i = 0;i < fClusNo;i ++) {
        if(FAT1[dataClusNo] == EOF_Fat32) {
            if(alloc) {
                FAT1[dataClusNo] = fat_alloc_cluster();
                dirty_Fat(dataClusNo);
            } else {
                *pClusNo = 0;
                return -E_NOT_FOUND;
            }
        }
        dataClusNo = FAT1[dataClusNo];
    }
    
    *pClusNo = dataClusNo;
    return 0;
}

int fat_walk_cluster_(struct Fat32_Dir *f, u_int fClusNo, struct Fat32_Dir **pClus, u_int alloc) {
    int i;
    u_int dataClusNo;
    if(f == NULL) {
        dataClusNo = fat_super.RootClus;
    } else {
        dataClusNo = file_get_dataClusNO(f);
    }
    for(i = 0;i < fClusNo;i ++) {
        if(FAT1[dataClusNo] == EOF_Fat32) {
            if(alloc) {
                FAT1[dataClusNo] = fat_alloc_cluster();
                dirty_Fat(dataClusNo);
            } else {
                *pClus = NULL;
                return -E_NOT_FOUND;
            }
        }
        dataClusNo = FAT1[dataClusNo];
    }
    
    *pClus = (struct Fat32_Dir *)read_data_cluster(dataClusNo);

    return 0;
}



int fat_file_get_cluster(struct Fat32_Dir *f, u_int fileClusNo, void **Fat32_buf) {
    int r;
    u_int clusNO;
    void *va = FATPAGEMAP;

    if(va_is_mapped(va) == 0) {
        syscall_mem_alloc(0, va, PTE_D);
    }
    memset(va, 0, BY2PG);
    
    if((r = fat_walk_cluster(f, fileClusNo, &clusNO, 1))) {
        return r;
    }
    memcpy(va, read_data_cluster(clusNO), BY2CLUS);
    
    *Fat32_buf = va;
    return 0;
}


char *skip_root(char *p) {
    if(*p == ':') {
        p ++;
        return p;
    }
    if(p[0] == 'F' && p[1] == 'A' && p[2] == 'T') {
        return p + 3;
    }
    return p;
}

char *fat_skip_slash(char *p) {
	while (*p == '/') {
		p++;
	}
	return p;
}


// Overview:
//  Evaluate a path name, starting at the root.
//
// Post-Condition:
//  On success, set *pfile to the file we found and set *pdir to the directory
//  the file is in.
//  If we cannot find the file but find the directory it should be in, set
//  *pdir and copy the final path element into lastelem.
int fat_walk_path(char *path, struct Fat32_Dir **pdir, struct Fat32_Dir **pfile, char *lastelem) {
    char *p;
    char name[MAXNAMELEN];
    struct Fat32_Dir *dir, *file, *entry = NULL;
    int r;
    path = skip_root(path);
    path = fat_skip_slash(path);
    file = (struct Fat32_Dir *)read_data_cluster(fat_super.RootClus);
    dir = 0;
    name[0] = 0;

    if(pdir) {
        *pdir = 0;
    }

    *pfile = 0;
    while(*path != '\0') {
        dir = file;
        p = path;
        while (*path != '/' && *path != '\0') {
            path ++;
        }
        if(*path == '/') {
            path ++;
        }
        if(path - p > MAXNAMELEN) {
            return -E_BAD_PATH;
        }
        for(int i = 0;i < path - p;i ++)
        {
            name[i] = p[i];
        }
        if(name[path - p - 1] == '/') {
            name[path - p - 1] = '\0';
        }
        name[path - p] = '\0';
        if((entry != 0) && ((entry->Attr & ATTR_DIRECTORY) == 0)) {
            return -E_NOT_FOUND;
        }

        if((r = fat_dir_lookup(dir, name, &file, &entry)) < 0) {
            
            if(r == -E_NOT_FOUND && *path == '\0') {
                if(pdir) {
                    *pdir = dir;
                }

                if(lastelem) {
                    strcpy(lastelem, name);
                }

                *pfile = 0;
            }

            return r;
        }
        
    }

    if(pdir) {
        *pdir = ROUNDDOWN(entry, BY2CLUS);
    }
    *pfile = entry;
    return 0;
}


int fat_walk_path_at(struct Fat32_Dir *par_dir, char *path, struct Fat32_Dir **pdir, struct Fat32_Dir **pfile, char *lastelem) {
    char *p;
    char name[MAXNAMELEN];
    struct Fat32_Dir *dir, *file, *entry = NULL;
    int r;
    path = skip_root(path);
    path = fat_skip_slash(path);
    file = (struct Fat32_Dir *)read_data_cluster(file_get_dataClusNO(par_dir));
    dir = par_dir;
    name[0] = 0;

    if(pdir) {
        *pdir = 0;
    }

    *pfile = 0;
    while(*path != '\0') {
        dir = file;
        p = path;
        while (*path != '/' && *path != '\0') {
            path ++;
        }
        if(*path == '/') {
            path ++;
        }
        if(path - p > MAXNAMELEN) {
            return -E_BAD_PATH;
        }
        for(int i = 0;i < path - p;i ++)
        {
            name[i] = p[i];
        }
        if(name[path - p - 1] == '/') {
            name[path - p - 1] = '\0';
        }
        name[path - p] = '\0';
        if((entry != 0) && ((entry->Attr & ATTR_DIRECTORY) == 0)) {
            return -E_NOT_FOUND;
        }

        if((r = fat_dir_lookup(dir, name, &file, &entry)) < 0) {
            if(r == -E_NOT_FOUND && *path == '\0') {
                if(pdir) {
                    *pdir = dir;
                }

                if(lastelem) {
                    strcpy(lastelem, name);
                }

                *pfile = 0;
            }

            return r;
        }
        
    }

    if(pdir) {
        *pdir = ROUNDDOWN(entry, BY2CLUS);
    }
    *pfile = entry;
    return 0;
}



void spiltPath(char *path, char *parent, char *this) {
    int i, j;
    for(i = strlen(path); path[i] != '/'; i --) ;
    for(j = 0; j < i; j ++) 
    {
        parent[j] = path[j];
    }
    parent[i] = '\0';
    for(j = i + 1; path[j] != '\0'; j ++)
    {
        this[j - i - 1] = path[j];
    }
    this[j - i - 1] = '\0';
}

int fat_needLN(char *path) {
    int i = 0;
    for(; path[i] != '\0' && path[i] != '.'; i ++) ;
    return (i > 8);
}

void fillFileLN(struct Fat32_LDIR *cur, char *user_name, u_int ord) {
    char *p = user_name + (ord - 1) * 13;
    int i, j;
    for(i = 0;p[i] != '\0';i ++) {
        if(i < 5) {
            cur->LDIR_Name1[i] = (short)p[i];
        } else if(i < 11) {
            cur->LDIR_Name2[i - 5] = (short)p[i];
        } else if(i < 13) {
            cur->LDIR_Name3[i - 11] = (short)p[i];
        } else {
            break;
        }
    }
    if(i < 5) {
        cur->LDIR_Name1[i] = 0;
    } else if(i < 11) {
        cur->LDIR_Name2[i - 5] = 0;
    } else if(i < 13) {
        cur->LDIR_Name3[i - 11] = 0;
    }
    i ++;
    for(;i < 13;i ++) {
        if(i < 5) {
            cur->LDIR_Name1[i] = 0xffff;
        } else if(i < 11) {
            cur->LDIR_Name2[i - 5] = 0xffff;
        } else {
            cur->LDIR_Name3[i - 11] = 0xffff;
        }
    }
}


int fat_free_Ndir_lookup(struct Fat32_Dir *dir, struct Fat32_Dir **entry, char *this, int n) {
    int r = 0;
    u_int dataClusNo, check;
    if(dir == NULL) {
        dataClusNo = fat_super.RootClus;
    } else {
        dataClusNo = file_get_dataClusNO(dir);
    }



    struct Dat32_Dir *p, *q, *file, *target = NULL;
    file = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
    if((r = fat_dir_lookup(file, this, &p, &q)) == 0) {
        return 0;
    }


    do {
        struct Fat32_Dir *tmp = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
        check = 0;
        for(int i = 0;i < BY2CLUS / sizeof(struct Fat32_Dir);i ++)
        {
            struct Fat32_Dir *cur = tmp + i;
            if(cur->Name[0] == '\0') {
                check ++;
                if(target == NULL) {
                    target = cur;
                }
            } else {
                check = 0;
                target = NULL;
            }
            if(check == n) {
                *entry = target;
                return 0;
            }
        }
        dataClusNo = FAT1[dataClusNo];
    } while(dataClusNo != EOF_Fat32);
    return -E_NOT_FOUND;
}








int fat_alloc_LongNameFile(char *this, struct Fat32_Dir *dir, struct Fat32_Dir *file, u_int totalClus) {
    int r;
    char tmp[11];
    u_int newClusNo;
    u_int needEntry = strlen(this) / 13 + 1;
    u_int total = totalClus;
    struct Fat32_Dir *cur = NULL;
    r = fat_free_Ndir_lookup(dir, &cur, this, needEntry + 1);
    if(r == 0 && cur == NULL) {
        return 0;
    }

    if(r < 0) {
        total ++;
        fat_walk_cluster(dir, total, &newClusNo, 1);
        cur = (struct Fat32_Dir *)read_data_cluster(newClusNo);
    }
    user_assert(cur != NULL);
    
    struct Fat32_LDIR *cur_ = (struct Fat32_LDIR *)cur;

    getFileName_(this, tmp, dir);
    cur_->LDIR_Attr = ATTR_LONG_NAME;
    cur_->LDIR_Ord = LAST_LONG_ENTRY | needEntry;
    cur_->LDIR_Chksum = ChkSum(tmp);
    fillFileLN(cur_, this, needEntry);

    

    for (int i = 1; i < needEntry; i++)
    {
        cur_++;
        cur_->LDIR_Attr = ATTR_LONG_NAME;
        cur_->LDIR_Ord = needEntry - i;
        cur_->LDIR_Chksum = ChkSum(tmp);
        fillFileLN(cur_, this, needEntry - i);
    }
    
    cur_++;
    struct Fat32_Dir *shortEntry = (struct Fat32_Dir *)cur_;
    setTime(shortEntry, 1);
    memcpy(shortEntry->Name, tmp, 11);
    shortEntry->FileSize = 0;
    shortEntry->Attr = ATTR_ARCHIVE;
    memset(shortEntry->ClusHI, 0, 2);
    memset(shortEntry->ClusLO, 0, 2);
    dirty_cluster(va2ClusNo(shortEntry));
    return 0;
}


int fat_alloc_LongNameDir(char *this, struct Fat32_Dir *dir, struct Fat32_Dir *file, u_int totalClus, struct Fat32_Dir **entry) {
    int r;
    char tmp[11];
    u_int newClusNo;
    u_int needEntry = strlen(this) / 13 + 1;
    u_int total = totalClus;
    struct Fat32_Dir *cur = NULL;
    r = fat_free_Ndir_lookup(dir, &cur, this, needEntry + 1);
    if(r == 0 && cur == NULL) {
        return 0;
    }
    if(r < 0) {
        total ++;
        fat_walk_cluster(dir, total, &newClusNo, 1);
        cur = (struct Fat32_Dir *)read_data_cluster(newClusNo);
    }
    user_assert(cur != NULL);
    
    struct Fat32_LDIR *cur_ = (struct Fat32_LDIR *)cur;

    getFileName_(this, tmp, dir);
    cur_->LDIR_Attr = ATTR_LONG_NAME;
    cur_->LDIR_Ord = LAST_LONG_ENTRY | needEntry;
    cur_->LDIR_Chksum = ChkSum(tmp);
    fillFileLN(cur_, this, needEntry);
    for (int i = 1; i < needEntry; i++)
    {
        cur_++;
        cur_->LDIR_Attr = ATTR_LONG_NAME;
        cur_->LDIR_Ord = needEntry - i;
        cur_->LDIR_Chksum = ChkSum(tmp);
        fillFileLN(cur_, this, needEntry - i);
    }
    cur_++;
    *entry = (struct Fat32_Dir *)cur_;
    memcpy((*entry)->Name, tmp, 11);
    dirty_cluster(va2ClusNo(cur_));
    return 0;
}



int fat_alloc_file(char *path) {
    int r;
    struct Fat32_Dir *dir, *file;
    char parentDir[MAXPATHLEN];
    char this[MAXPATHLEN];
    u_int totalClus, newClusNo;
    spiltPath(path, parentDir, this);

    if((r = fat_walk_path(parentDir, 0, &dir, 0)) < 0) {
        return r;
    }

    totalClus = fat_total_clus(dir);


    if(dir == NULL) {
        file = (struct Fat32_Dir *)read_data_cluster(fat_super.RootClus);
    } else {
        file = (struct Fat32_Dir *)read_data_cluster(file_get_dataClusNO(dir));
    }

    
    if(fat_needLN(this)) {
        return fat_alloc_LongNameFile(this, dir, file, totalClus);
    }
    

    struct Fat32_Dir *cur = NULL;
    
    if((r = fat_free_dir_lookup(file, &cur, this)) < 0) {
        totalClus ++;
        fat_walk_cluster(dir, totalClus, &newClusNo, 1);
        cur = (struct Fat32_Dir *)read_data_cluster(newClusNo);
    }
    if(r == 0 && cur == NULL) {
        return 0;
    }
    user_assert(cur != NULL);
    setTime(cur, 1);
    getFileName(this, cur->Name); 
    cur->FileSize = 0;
    cur->Attr = ATTR_ARCHIVE;
    memset(cur->ClusHI, 0, 2);
    memset(cur->ClusLO, 0, 2);
    dirty_cluster(va2ClusNo(cur));
    return 0;
}


int fat_alloc_file_at(char *path, struct Fat32_Dir *par_dir) {
    int r;
    struct Fat32_Dir *dir, *file;
    char this[MAXPATHLEN];
    u_int totalClus, newClusNo;
    
    strcpy(this, path);
    dir = par_dir;

    totalClus = fat_total_clus(dir);


    file = (struct Fat32_Dir *)read_data_cluster(file_get_dataClusNO(dir));

    
    if(fat_needLN(this)) {
        return fat_alloc_LongNameFile(this, dir, file, totalClus);
    }
    
    struct Fat32_Dir *cur = NULL;
    
    if((r = fat_free_dir_lookup(file, &cur, this)) < 0) {
        totalClus ++;
        fat_walk_cluster(dir, totalClus, &newClusNo, 1);
        cur = (struct Fat32_Dir *)read_data_cluster(newClusNo);
    }
    if(r == 0 && cur == NULL) {
        return 0;
    }
    user_assert(cur != NULL);
    setTime(cur, 1);
    getFileName(this, cur->Name); 
    cur->FileSize = 0;
    cur->Attr = ATTR_ARCHIVE;
    memset(cur->ClusHI, 0, 2);
    memset(cur->ClusLO, 0, 2);
    dirty_cluster(va2ClusNo(cur));
    return 0;
}

int fat_alloc_dir_at(char *path, struct Fat32_Dir *par_dir) {
    int r;
    struct Fat32_Dir *dir = par_dir;
    struct Fat32_Dir *file;
    char parentDir[MAXPATHLEN];
    char this[MAXPATHLEN];
    u_int totalClus, newClusNo;
    strcpy(this, path);


    file = (struct Fat32_Dir *)read_data_cluster(file_get_dataClusNO(dir));
    
    struct Fat32_Dir *cur = NULL;
    
    totalClus = fat_total_clus(dir);

    if(fat_needLN(this)) {
        r = fat_alloc_LongNameDir(this, dir, file, totalClus, &cur);
        if(r == 0 && cur == NULL) {
            return 0;
        }
    } else {
        if ((r = fat_free_dir_lookup(file, &cur, this)) < 0)
        {
            totalClus++;
            fat_walk_cluster(dir, totalClus, &newClusNo, 1);
            cur = (struct Fat32_Dir *)read_data_cluster(newClusNo);
        }
        if(r == 0 && cur == NULL) {
            return 0;
        }
        user_assert(cur != NULL);
        getFileName(this, cur->Name); 
    }

    cur->FileSize = 0;
    cur->Attr = ATTR_DIRECTORY;
    
    setTime(cur, 1);

    int dataClusNo = fat_alloc_cluster();
    cur->ClusHI[1] = (dataClusNo & (0xff << 24)) >> 24;
    cur->ClusHI[0] = (dataClusNo & (0xff << 16)) >> 16;
    cur->ClusLO[1] = (dataClusNo & (0xff << 8)) >> 8;
    cur->ClusLO[0] = dataClusNo & 0xff;
    
    file = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
    file->FileSize = 0;
    file->Attr = ATTR_DIRECTORY;
    memset(file->Name, 32, 11);
    file->Name[0] = '.';
    file->ClusHI[1] = cur->ClusHI[1];
    file->ClusHI[0] = cur->ClusHI[0];
    file->ClusLO[1] = cur->ClusLO[1];
    file->ClusLO[0] = cur->ClusLO[0];
    user_assert(file_get_dataClusNO(file) == file_get_dataClusNO(cur));

    setTime(file, 1);

    file ++;

    file->FileSize = 0;
    file->Attr = ATTR_DIRECTORY;
    memset(file->Name, 32, 11);
    file->Name[0] = '.';
    file->Name[1] = '.';
    if(dir == NULL) {
        memset(file->ClusHI, 0, 2);
        memset(file->ClusLO, 0, 2);
        user_assert(file_get_dataClusNO(file) == 0);
    } else {
        file->ClusHI[1] = dir->ClusHI[1];
        file->ClusHI[0] = dir->ClusHI[0];
        file->ClusLO[1] = dir->ClusLO[1];
        file->ClusLO[0] = dir->ClusLO[0];
        user_assert(file_get_dataClusNO(file) == file_get_dataClusNO(dir));
    }

    setTime(file, 1);
    dirty_cluster(va2ClusNo(cur));
    dirty_cluster(Data2ClusNO(dataClusNo));
    return 0;
}


int fat_alloc_dir(char *path) {
    int r;
    struct Fat32_Dir *dir = NULL;
    struct Fat32_Dir *file;
    char parentDir[MAXPATHLEN];
    char this[MAXPATHLEN];
    u_int totalClus, newClusNo;
    spiltPath(path, parentDir, this);

    if((r = fat_walk_path(parentDir, 0, &dir, 0)) < 0) {
        return r;
    }
    if(dir == NULL) {
        file = (struct Fat32_Dir *)read_data_cluster(fat_super.RootClus);
    } else {
        file = (struct Fat32_Dir *)read_data_cluster(file_get_dataClusNO(dir));
    }
    struct Fat32_Dir *cur = NULL;
    
    totalClus = fat_total_clus(dir);

    if(fat_needLN(this)) {
        r = fat_alloc_LongNameDir(this, dir, file, totalClus, &cur);
        if(r == 0 && cur == NULL) {
            return 0;
        }
    } else {
        if ((r = fat_free_dir_lookup(file, &cur, this)) < 0)
        {
            totalClus++;
            fat_walk_cluster(dir, totalClus, &newClusNo, 1);
            cur = (struct Fat32_Dir *)read_data_cluster(newClusNo);
        }
        if(r == 0 && cur == NULL) {
            return 0;
        }
        user_assert(cur != NULL);
        getFileName(this, cur->Name); 
    }

    cur->FileSize = 0;
    cur->Attr = ATTR_DIRECTORY;
    
    setTime(cur, 1);

    int dataClusNo = fat_alloc_cluster();
    cur->ClusHI[1] = (dataClusNo & (0xff << 24)) >> 24;
    cur->ClusHI[0] = (dataClusNo & (0xff << 16)) >> 16;
    cur->ClusLO[1] = (dataClusNo & (0xff << 8)) >> 8;
    cur->ClusLO[0] = dataClusNo & 0xff;
    
    file = (struct Fat32_Dir *)read_data_cluster(dataClusNo);
    file->FileSize = 0;
    file->Attr = ATTR_DIRECTORY;
    memset(file->Name, 32, 11);
    file->Name[0] = '.';
    file->ClusHI[1] = cur->ClusHI[1];
    file->ClusHI[0] = cur->ClusHI[0];
    file->ClusLO[1] = cur->ClusLO[1];
    file->ClusLO[0] = cur->ClusLO[0];
    user_assert(file_get_dataClusNO(file) == file_get_dataClusNO(cur));

    setTime(file, 1);

    file ++;

    file->FileSize = 0;
    file->Attr = ATTR_DIRECTORY;
    memset(file->Name, 32, 11);
    file->Name[0] = '.';
    file->Name[1] = '.';
    if(dir == NULL) {
        memset(file->ClusHI, 0, 2);
        memset(file->ClusLO, 0, 2);
        user_assert(file_get_dataClusNO(file) == 0);
    } else {
        file->ClusHI[1] = dir->ClusHI[1];
        file->ClusHI[0] = dir->ClusHI[0];
        file->ClusLO[1] = dir->ClusLO[1];
        file->ClusLO[0] = dir->ClusLO[0];
        user_assert(file_get_dataClusNO(file) == file_get_dataClusNO(dir));
    }

    setTime(file, 1);
    dirty_cluster(va2ClusNo(cur));
    dirty_cluster(Data2ClusNO(dataClusNo));
    return 0;
}


// Overview:
//  Open "path".
//
// Post-Condition:
//  On success set *pfile to point at the file and return 0.
//  On error return < 0.
int fat_file_open(char *path, struct Fat32_Dir **pfile, int omode) {
    int r;
    switch (omode & 0xff00)
    {
    case O_CREAT:
        if((r = fat_alloc_file(path)) < 0 ){
            return r;
        }
        break;
    case O_MKDIR:
        if((r = fat_alloc_dir(path)) < 0 ){
            return r;
        }
        break;
    default:
        break;
    }

    r = fat_walk_path(path, 0, pfile, 0);
    return r;
}


int fat_file_openat(struct Fat32_Dir *dir, char *path, struct Fat32_Dir **pfile, int omode) {
    int r;
    if((dir->Attr & ATTR_DIRECTORY) == 0) {
        return -E_INVAL;
    }
    switch (omode & 0xff00)
    {
    case O_CREAT:
        if((r = fat_alloc_file_at(path, dir)) < 0 ){
            return r;
        }
        break;
    case O_MKDIR:
        if((r = fat_alloc_dir_at(path, dir)) < 0 ){
            return r;
        }
        break;
    default:
        break;
    }

    r = fat_walk_path_at(dir, path, 0, pfile, 0);
    return r;
}





int dirty_page(int beginClus) {
    int r;
    u_int dataClusNo = beginClus;
    for(int i = 0;i < CLUS2Page;i ++)
            {
        r = dirty_cluster(Data2ClusNO(dataClusNo));
        if(r < 0) {
            return r;
        }

        if(FAT1[dataClusNo] == EOF_Fat32) {
            break;
        }
        dataClusNo = FAT1[dataClusNo];
    }
    return 0;
}

int fat_file_dirty(struct Fat32_Dir *f, u_int offset) {
    int r;
    u_int clusterNo;
    clusterNo = file_get_dataClusNO(f);
    for(int i = 0;i < offset;i += BY2CLUS) {
        if(FAT1[clusterNo] == EOF_Fat32) {
            break;
        }
        clusterNo = FAT1[clusterNo];
    }
    
    return dirty_page(clusterNo);
}


void fat_file_flush(struct Fat32_Dir *f) {
    u_int dataClusNo = file_get_dataClusNO(f);
    if(dataClusNo == 0) {
        return;
    }
    int r;
    do{
        if(fat_clus_is_dirty(dataClusNo)) {
            write_cluster(Data2ClusNO(dataClusNo));
        }
        dataClusNo = FAT1[dataClusNo];
    } while(dataClusNo != EOF_Fat32);
}

int fat_dir_dirty(struct Fat32_Dir *f) {
    void *dirClus = ROUNDDOWN(f, BY2CLUS);
    return dirty_cluster(va2ClusNo(dirClus));
}

void fat_dir_flush(struct Fat32_Dir *f) {
    void *dirClus = ROUNDDOWN(f, BY2CLUS);
    u_int dataClusNo = Clusno2Data(va2ClusNo(dirClus));
    if(fat_clus_is_dirty(dataClusNo)) {
        write_cluster(Data2ClusNO(dataClusNo));
    }
}

void fat_file_close(struct Fat32_Dir *f) {
    fat_file_flush(f);
    fat_dir_dirty(f);
    fat_dir_flush(f);
}


void fat_file_truncate(struct Fat32_Dir *f, u_int newsize) {
    u_int dataClusNo, tmp, old_nclusters, new_nclusters, i;
    old_nclusters = f->FileSize / BY2CLUS + 1;
    new_nclusters = newsize / BY2CLUS + 1;
    user_assert(f->FileSize > 0);
    if(newsize == 0) {
        new_nclusters = 0;
    }
    dataClusNo = file_get_dataClusNO(f);
    tmp = dataClusNo;
    for(i = 0;i < new_nclusters;i ++) {
        tmp = dataClusNo;
        dataClusNo = FAT1[dataClusNo];
    }

    if(new_nclusters > 0) {
        FAT1[tmp] = EOF_Fat32;
        dirty_Fat(tmp);
    }

    for(i = new_nclusters; i < old_nclusters;i ++) {
        int r = FAT1[dataClusNo];
        fat_free_cluster(dataClusNo);
        dataClusNo = r;
    }
    if(newsize == 0) {
        memset(f->ClusHI, 0, 2);
        memset(f->ClusLO, 0, 2);
    }
    f->FileSize = newsize;
}



int fat_file_set_size(struct Fat32_Dir *f, u_int newsize) {
    if(f->FileSize > newsize) {
        fat_file_truncate(f, newsize);
    }
    if(f->FileSize == 0 && newsize > 0) {
        int dataClusNo = fat_alloc_cluster();
        f->ClusHI[1] = (dataClusNo & (0xff << 24)) >> 24;
        f->ClusHI[0] = (dataClusNo & (0xff << 16)) >> 16;
        f->ClusLO[1] = (dataClusNo & (0xff << 8)) >> 8;
        f->ClusLO[0] = dataClusNo & 0xff;
    }
    f->FileSize = newsize;
    fat_dir_dirty(f);
    fat_dir_flush(f);
    return 0;
}


int fClusNo2Va(struct Fat32_Dir *f, u_int fClusNo, char **va) {
    u_int pClusNo = 0;
    void *res;
    int r;
    if((r = fat_walk_cluster(f, fClusNo, &pClusNo, 0)) < 0) {
        return r;
    }
    user_assert(pClusNo != 0);
    res = fat_clus_is_mapped(Data2ClusNO(pClusNo));
    if(res == NULL) {
        return -E_INVAL;
    }
    user_assert(va_is_mapped(res));
    *va = (char *)res;
    return 0;
}



int fat_file_writeBack(struct Fat32_Dir *f, u_int offset, u_int n, char *src) {
    //debugf("%s\n", f->Name);
    u_int dataClusNo = file_get_dataClusNO(f);
    u_int fClusNo = offset / BY2CLUS, curPtr = offset % BY2CLUS, pClusNo = 0;
    setTime(f, 0);
    char *va;
    int r;
    
    if((r = fClusNo2Va(f, fClusNo, &va)) < 0) {
        return r;
    }
    dirty_cluster(va2ClusNo(va));
    
    for(int i = 0;i < n;i ++) {
        va[curPtr ++] = src[i];
        if(curPtr >= BY2CLUS) {
            curPtr %= BY2CLUS;
            fClusNo ++;
            if((r = fClusNo2Va(f, fClusNo, &va)) < 0) {
                return r;
            }
            dirty_cluster(va2ClusNo(va));
        }
    }
    fat_dir_dirty(f);
    return 0;
}

int fat_file_remove(char *path) {
    int r;
    struct Fat32_Dir *f;
    struct Fat32_Dir *dir, *cur;
    if((r = fat_walk_path(path, &dir, &f, 0)) < 0) {
        return r;
    }
    if(f->FileSize > 0) {
        fat_file_truncate(f, 0);
    }
    for(int i = f - dir - 1;i >= 0;i --)
    {
        cur = dir + i;
        struct Fat32_LDIR *cur_ = (struct Fat32_LDIR *)cur;
        if((cur_->LDIR_Attr & ATTR_LONG_NAME) == 0) {
            break;
        }
        memset(cur_, 0, sizeof(struct Fat32_LDIR));
    }
    memset(f->Name, '\0', 11);
    fat_dir_dirty(f);
    fat_dir_flush(f);
    return 0;
}

int fat_file_removeat(struct Fat32_Dir *par_dir, char *path) {
    int r;
    struct Fat32_Dir *f;
    struct Fat32_Dir *dir, *cur;
    dir = par_dir;
    
    if((dir->Attr & ATTR_DIRECTORY) == 0) {
        
        return -E_INVAL;
    }


    if((r = fat_dir_lookup(read_data_cluster(file_get_dataClusNO(par_dir)), path, 0, &f)) < 0) {
        return r;
    }
    

    if(f->FileSize > 0) {
        fat_file_truncate(f, 0);
    }
    

    for(int i = f - dir - 1;i >= 0;i --)
    {
        cur = dir + i;
        struct Fat32_LDIR *cur_ = (struct Fat32_LDIR *)cur;
        if((cur_->LDIR_Attr & ATTR_LONG_NAME) == 0) {
            break;
        }
        //memset(cur_, 0, sizeof(struct Fat32_LDIR));
        cur->Name[0] = '\0';
    }

    memset(f->Name, '\0', 11);
    fat_dir_dirty(f);
    fat_dir_flush(f);
    return 0;
}

void flush_Fat() {
    u_int clusNo = va2ClusNo(FAT1);
    u_int total = fat_super.FAT_SECPClus / SECT2CLUS;
    for(int i = clusNo;i < clusNo + total;i ++) {
        void *va = fat_diskaddr(i);
        if(va_is_mapped(va) && va_is_dirty(va)) {
            write_cluster(i);
        }
    }
}

void flush_Data() {
    u_int rootSec = fat_super.numOfReserved  + fat_super.numOfFats * fat_super.SECPFAT;
    u_int rootClus = rootSec / SECT2CLUS;
    for(int i = 2;i < fat_super.TotClus - rootClus;i ++)
    {
        if(fat_clus_is_dirty(i)) {
            write_cluster(Data2ClusNO(i));
        }
    }
}



void fat_fs_sync(void) {
    if(strcmp(fat_super.FSType, "FAT32   ") != 0) {
        return;
    }
    flush_Fat();
    flush_Data();
}