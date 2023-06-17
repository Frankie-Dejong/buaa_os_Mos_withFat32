# Lab5 Challenge Mos with Fat32

date: 2023/6/16

# 实现功能

- 读取BPB，获得Fat32文件系统各个参数
- 支持兼容不同每簇扇区数的Fat32文件系统
- 维护内存中的FAT表，并实时刷新
- 实现了目录结构的读取和写入
    - 支持长文件名
    - 支持创建和修改时间功能
- 支持文件操作相关功能（兼容原有文件接口）
    - 打开、关闭文件
    - 读写文件
    - 新建文件和文件夹
    - 删除文件
    - 刷新缓冲区，每次关闭（重新挂载磁盘）会将修改内容写入磁盘
    - 支持相对路径功能

# 实现细节

## 读取BPB

创建了新的结构体以适应BPB的组织结构

```cpp
//fs.h
struct FAT_Super {
	int FAT_BY2SEC;
	int FAT_SECPClus;
	char FSType[9];
	int FAT_endNumber;
	int RootClus;
	int SECPFAT;
	int TotClus;
	int numOfFats;
	int numOfReserved;
};
```

具体实现细节见附录中fat_readBPB()函数

## 支持不同每簇扇区数的Fat32文件系统

在BPB基础上，有

```cpp
//fs.h
#define BY2CLUS (BY2SECT * SECT2CLUS)

//fatfs.h
u_int SECT2CLUS
SECT2CLUS = fat_super.FAT_SECPClus；
```

此外，对于块缓存部分，在实现时为支持不同每簇扇区数的Fat32文件系统，并适应Mos的页式内存管理结构，当读写某簇或将某簇标记为已在缓存中修改时，实际上是按页为单位进行操作，也即，当读取一簇时会将对应页上的簇均进行相同的操作，具体可以参见附录中read_cluster函数

## 维护内存中的FAT表，FS-Info表，并实时刷新

在Fat32文件系统初始化时会读入FAT表和fs_Info块到内存中

为保证fs_Info块的正确性，每次启动时会进行自查以确保更新到了正确的数值

在FAT表的内容被修改时，会写回磁盘（即刷新FAT，确保FAT内容时完全正确的），即使用户在编写程序时未执行任何的close或flush操作，该操作也会进行，否则在下次挂载磁盘时将出现不可预知的错误，导致文件系统崩溃

```cpp
int dirty_Fat(u_int dataClusNo) {
    write_cluster(FAT1_ClusNo + (dataClusNo * 4) / BY2CLUS);
    return 0;
}
```

此后，以后任意对簇的申请和释放必须通过函数接口fat_alloc_cluster()和fat_free_cluster()完成，他们分别实现了申请和释放簇，同时维护、刷新FAT表和fs_Info的功能

这里，我们保证在任一时刻，fs_Info中的FSI_Nxtfree成员都存放着编号最小的一个可用簇的编号，因此两函数的逻辑可用下列伪代码来表示

```cpp
int fat_alloc_cluster() {
		1. 检查是否还有可用的簇（fs_info的FreeCount是否大于0）
		2. 将fs_info的FreeCount数量-1
		3. 将fs_info中存放的Nxtfree所对应的编号取出
		4. 将对应的FAT表的值设定为EOF
		5. 更新FAT表
		6. 从得到的编号向上寻找第一个空闲的簇，更新fs_info的Nxtfree成员
		7. 返回该编号
}

void fat_free_cluster(int dataClusNO) {
		1. 将FAT表中对应的元素置0
		2. 更新FAT表
		3. 将fs_info的FreeCount+1
		4. 将fs_info的Nxtfree成员更新为min(Nxtfree, dataClusNO)
		5. 返回
}
```

## 目录结构的读取和写入

对于基础的短文件名目录项，有结构体如下

```cpp
struct Fat32_Dir {
	char Name[11];
	char Attr;
	char NTRes;
	char CrtTimeTenth;
	short CrtTime;
	short CrtDate;
	char LstAccDate[2];
	char ClusHI[2];
	short WrtTime;
	short WrtDate;
	char ClusLO[2];
	int FileSize;
}__attribute__((packed));
```

Fat32文件系统结构中，每个目录下的所有文件和目录的目录项均存放于该目录所对应的簇中，因此我们只需在对应的目录下按文件名去查找即可，即从上层目录所对应的第一个簇开始，遍历从属于上层目录的所有簇，直到找到对应的目录项，具体可以参见附录中的

长文件名的支持

Fat32对于长文件名，选择将其名称连续的存放于目录控制项之前，即

![截屏2023-06-16 23.01.46.png](Lab5%20Challenge%20Mos%20with%20Fat32%201ecee693a87e4c92810d0f2215a221f0/%25E6%2588%25AA%25E5%25B1%258F2023-06-16_23.01.46.png)

为删除、查找方便，我们保证在申请长文件名目录项时，其所有的long name directory entry和对应的short name directory entry应当被存放在同一个簇当中，而不能跨簇存放，且所有的长文件名目录项必须紧跟一个短文件名目录项

因此，在查找的过程中，对于长文件名的处理情况，我们只需将文件名和长文件名目录项中所存储的对应部位相比对，若所有的长文件名目录项均比对成功，则说明查找到了对应的目录项

支持创建和修改时间

在创建或修改时，读取gxemul所提供的utc时间，并转换为北京时间，写入目录项所对应的内容中，读取时间代码如下, 转换北京时间代码参见time.c

```cpp
//time.h
#define RTC 0x15000000

//time.c
u_int get_time(u_int *us)
{
    u_int tmp;
    u_int seconds;
    syscall_read_dev(&tmp, RTC, 4);
    syscall_read_dev(&seconds, RTC + 0x10, 4);
    syscall_read_dev(us, RTC + 0x20, 4);
    return seconds;
}
```

# 支持文件操作相关功能

## 用户进程

提供给用户的接口完全不变，若在路径前加上一个冒号或FAT，则视为访问Fat32文件系统，否则视为访问原有文件系统，如”:/test_directory/test_file1”，即为一个Fat32文件系统的路径，在Fat32文件系统下，暴露给用户的文件IO函数的功能如下

- int open(const char *path, int o_mode)：打开（创建）一个文件（文件夹）
    - path：一个以冒号或FAT开头的字符串，代表要打开（创建）文件路径
    - o_mode:
        - O_RDONLY：以只读的方式打开已经存在的文件，返回对应的文件描述符编号，否则返回小于0的值
        - O_WRONLY：以只写的方式打开已经存在的文件，返回对应的文件描述符编号，否则返回小于0的值
        - O_RDWR：以读写的方式打开已经存在的文件，返回对应的文件描述符的编号，否则返回小于0的值
        - O_CREAT：若文件不存在，则创建文件并打开，否则打开已经存在的文件，若出错返回小于0的值
        - O_MKDIR：若文件夹不存在，则创建文件夹并打开，否则打开已经存在的文件夹否则返回小于0的值
- int read(int fd, void *buf, u_int nbytes)：从文件中读入数据
    - fd，来自于open函数的合法的返回值
    - buf，将要读取入的缓冲区
    - nbytes，读取的最大字节数
- int write(int fd, void *src, u_int nbytes)：向文件中写入数据，若有必要，调整文件的大小
    - fd，来自于open函数的合法的返回值
    - src，将要写入内容的首地址
    - nbytes，写入的最大字节数
- int close(int fd)：关闭一个文件并刷新缓冲区
    - fd，来自于open函数的合法的返回值
- int remove(const char *path)：删除一个文件
    - path：一个以冒号或FAT开头的字符串，代表要删除的文件路径
- int seek(int fd, u_int offset)：调整文件读写指针的位置
    - fd，来自于open函数的合法的返回值
    - offset，要设置的文件指针相对于文件起始位置的偏移
- int openat(int fd, char *path, int omode)：
    - fd，一个已经打开的目录的文件描述符
    - path，相对于该目录的相对路径
    - omode
        - O_RDONLY：以只读的方式打开已经存在的文件，返回对应的文件描述符编号，否则返回小于0的值
        - O_WRONLY：以只写的方式打开已经存在的文件，返回对应的文件描述符编号，否则返回小于0的值
        - O_RDWR：以读写的方式打开已经存在的文件，返回对应的文件描述符的编号，否则返回小于0的值
        - O_CREAT：若文件不存在，则创建文件并打开，否则打开已经存在的文件，若出错返回小于0的值
        - O_MKDIR：若文件夹不存在，则创建文件夹并打开，否则打开已经存在的文件夹否则返回小于0的值
- int removeAt(int fd, char *path):
    - fd，一个已经打开的目录的文件描述符
    - path，相对于该目录的相对路径

此外，所有file.c中的文件操作函数均已对Fat32文件系统进行了兼容

## 文件系统服务进程

```cpp
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
```

其中，fat_walk_path函数会从根目录开始查找对应的文件，并将pfile设置为对应的文件目录项，对于长文件名的查找如上文所述

而在创建时，fat_alloc_file或fat_alloc_dir会写入新的文件目录项，并完成对应的初始化操作，再有fat_walk_path进行打开

```cpp
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
    f->Name[0] = '\0';
    fat_dir_dirty(f);
    fat_dir_flush(f);
    return 0;
}
```

remove对应的操作即将文件的大小置为0，并将Name的首位记为0，对于长文件名目录项，会将其长文件名清空

```cpp
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
```

将文件f的大小设置为新的大小，值得注意的是，在将一个新创建的（大小为0）的文件夹设定为大于0的大小时，将为它分配一个簇

```cpp
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
```

将对应的簇读取到一个缓冲区上，并将缓冲区传递给用户进程，从而完成了读取，与之对应的，用户进程的写操作的内容也会传递给文件服务进程，由文件服务进程进行刷新，这样屏蔽了簇的大小和FAT的链式组织形式，从用户角度看文件中所存放的内容都是连续存放的

openat,removeAt函数逻辑与open和removeat相仿，区别为依靠fd参数寻找到已打开的目录，并从该目录开始寻找，这一功能赋予了open函数打开文件夹的意义

# 执行流程

以下面这条语句为例

```cpp
int r = open(":/test_long_name_file", O_CREAT | O_RDWR);
```

![截屏2023-06-17 21.00.59.png](/Users/mac/Library/Application Support/typora-user-images/截屏2023-06-17 21.24.42.png)

# 正确结果展示

## lab5_challenge_RW

执行

```cpp
cd ~/21373157
make test lab=5_challenge_RW
make run > output.txt
```

得到output.txt内容和目录结构

```cpp
gxemul -T -C R3000 -M 64 -E testmips -d 0:target/fs.img -d 1:myFS/fat.img target/mos
GXemul 0.7.0+dfsg    Copyright (C) 2003-2021  Anders Gavare
Read the source code and/or documentation for other Copyright messages.

net: 
    simulated network: 
        10.0.0.0/8 (max outgoing: TCP=100, UDP=100)
        gateway+nameserver: 10.0.0.254 (60:50:40:30:20:10)
        nameserver uses real nameserver 202.112.128.51
machine: 
    model: MIPS test machine
    cpu: R3000 (32-bit, I+D = 4+4 KB, L2 = 0 KB)
    memory: 64 MB
    diskimage: target/fs.img
        IDE DISK id 0, read/write, 4 MB (9072 512-byte blocks)
    diskimage: myFS/fat.img
        IDE DISK id 1, read/write, 33 MB (68544 512-byte blocks)
    file: loading target/mos
    cpu0: starting at 0x800119b0 <_start>
-------------------------------------------------------------------------------

init.c:	mips_init() is called
Memory size: 65536 KiB, number of pages: 16384
to memory 80430000 for struct Pages.
pmap.c:	 mips vm init success
FS is running
superblock is good
read_bitmap is good
fat_readBPB is good
read_FAT is good
read_FSInfo is good
================================
Total free Cluster Count:0x000103cd
Next Available Cluster Number:0x00000003
================================
read & write cluster is good
================================
fat32_init success!
================================
test_build passed
test write passed
test read passed
test close passed
================================
这世上真话本就不多, 一位女子的脸红便胜过一大段对白
可后来有了胭脂, 便分不清是真情还是假意
================================
[00000800] destroying 00000800
[00000800] free env 00000800
i am killed ... 
panic at sched.c:40 (schedule): NO RUNNABLE ENV
ra:    80016b08  sp:  803ffe80  Status: 0008100c
Cause: 00000020  EPC: 004050c0  BadVA:  00411eac
curenv:    NULL
cur_pgdir: 83ffc000
```

![截屏2023-06-17 21.21.31.png](https://github.com/Frankie-Dejong/buaa_os_Mos_withFat32/blob/main/images/截屏2023-06-17%2021.21.31.png?raw=true)

## lab5_challenge_at

执行

```cpp
cd ~/21373157
make test lab=5_challenge_at
make run > output.txt
```

得到output.txt内容和目录结构如下

```cpp
gxemul -T -C R3000 -M 64 -E testmips -d 0:target/fs.img -d 1:myFS/fat.img target/mos
GXemul 0.7.0+dfsg    Copyright (C) 2003-2021  Anders Gavare
Read the source code and/or documentation for other Copyright messages.

net: 
    simulated network: 
        10.0.0.0/8 (max outgoing: TCP=100, UDP=100)
        gateway+nameserver: 10.0.0.254 (60:50:40:30:20:10)
        nameserver uses real nameserver 202.112.128.51
machine: 
    model: MIPS test machine
    cpu: R3000 (32-bit, I+D = 4+4 KB, L2 = 0 KB)
    memory: 64 MB
    diskimage: target/fs.img
        IDE DISK id 0, read/write, 4 MB (9072 512-byte blocks)
    diskimage: myFS/fat.img
        IDE DISK id 1, read/write, 33 MB (68544 512-byte blocks)
    file: loading target/mos
    cpu0: starting at 0x800119b0 <_start>
-------------------------------------------------------------------------------

init.c:	mips_init() is called
Memory size: 65536 KiB, number of pages: 16384
to memory 80430000 for struct Pages.
pmap.c:	 mips vm init success
FS is running
superblock is good
read_bitmap is good
fat_readBPB is good
read_FAT is good
read_FSInfo is good
================================
Total free Cluster Count:0x000103c6
Next Available Cluster Number:0x00000005
================================
read & write cluster is good
================================
fat32_init success!
================================
test build passed
test write passed
test read passed
test close passed
test remove passed
当我还只有六岁的时候，在一本描写原始森林的名叫《真实的故事》的书中， 看到了一副精彩的插画，画的是一条蟒蛇正在吞食一只大野兽。页头上就是那副画的摹本。
这本书中写道:"这些蟒蛇把它们的猎获物不加咀嚼地囫囵吞下，尔后就不能再动弹了;它们就在长长的六个月的睡眠中消化这些食物
"当时,我对丛林中的奇遇想得很多,于是,我也用彩色铅笔画出了我的第一副图画.我的第一号作品.它是这样的:
        |----------|
        |          |
________|__________|___________
我把我的这副杰作拿给大人看，我问他们我的画是不是叫他们害怕。
他们回答我说:"一顶帽子有什么可怕的?"我画的不是帽子,是一条巨蟒在消化着一头大象.于是我又把巨蟒肚子里的情况画了出来,以便让大人们能够看懂.这些大人总是需要解释.我的第二号作品是这样的:
        |--------------|
        |  ********    |
        |*   ******    |
        |    *    *    |
________|______________|___________
大人们劝我把这些画着开着肚皮的,或闭上肚皮的蟒蛇的图画放在一边,还是把兴趣放在地理,历史,算术,语法上.就这样,在六岁的那年,我就放弃了当画家这一美好的职业.我的第一号,第二号作品的不成功,使我泄了气.这些大人们,靠他们自己什么也弄不懂，还得老是不断地给他们作解释.这真叫孩子们腻味.
后来,我只好选择了另外一个职业,我学会了开飞机,世界各地差不多都飞到过.的确,地理学帮了我很大的忙.我一眼就能分辨出中国和亚里桑那.要是夜里迷失了航向,这是很有用的.
这样,在我的生活中,我跟许多严肃的人有过很多的接触.我在大人们中间生活过很长时间.我仔细地观察过他们,但这并没有使我对他们的看法有多大的改变.
当我遇到一个头脑看来稍微清楚的大人时,我就拿出一直保存着的我那第一号作品来测试测试他.我想知道他是否真的有理解能力.可是,得到的回答总是:"这是顶帽子."我就不和他谈巨蟒呀,原始森林呀,或者星星之类的事.我只得迁就他们的水平,和他们谈些桥牌呀,高尔夫球呀,政治呀,领带呀这些.于是大人们就十分高兴能认识我这样一个通情达理的人.
[00000800] destroying 00000800
[00000800] free env 00000800
i am killed ... 
panic at sched.c:40 (schedule): NO RUNNABLE ENV
ra:    80016b08  sp:  803ffe80  Status: 0008100c
Cause: 00001020  EPC: 00405570  BadVA:  7f4000c4
curenv:    NULL
cur_pgdir: 83ffc000
```

![截屏2023-06-17 21.18.28.png](https://github.com/Frankie-Dejong/buaa_os_Mos_withFat32/blob/main/images/截屏2023-06-17%2021.18.28.png?raw=true)

## lab5_challenge

执行

```cpp
cd ~/21373157
make test lab=5_challenge
make run > output.txt
```

得到output.txt内容和目录结构如下

```cpp
gxemul -T -C R3000 -M 64 -E testmips -d 0:target/fs.img -d 1:myFS/fat.img target/mos
GXemul 0.7.0+dfsg    Copyright (C) 2003-2021  Anders Gavare
Read the source code and/or documentation for other Copyright messages.

net: 
    simulated network: 
        10.0.0.0/8 (max outgoing: TCP=100, UDP=100)
        gateway+nameserver: 10.0.0.254 (60:50:40:30:20:10)
        nameserver uses real nameserver 202.112.128.51
machine: 
    model: MIPS test machine
    cpu: R3000 (32-bit, I+D = 4+4 KB, L2 = 0 KB)
    memory: 64 MB
    diskimage: target/fs.img
        IDE DISK id 0, read/write, 4 MB (9072 512-byte blocks)
    diskimage: myFS/fat.img
        IDE DISK id 1, read/write, 33 MB (68544 512-byte blocks)
    file: loading target/mos
    cpu0: starting at 0x800119b0 <_start>
-------------------------------------------------------------------------------

init.c:	mips_init() is called
Memory size: 65536 KiB, number of pages: 16384
to memory 80430000 for struct Pages.
pmap.c:	 mips vm init success
FS is running
superblock is good
read_bitmap is good
fat_readBPB is good
read_FAT is good
read_FSInfo is good
================================
Total free Cluster Count:0x000103c4
Next Available Cluster Number:0x00000008
================================
read & write cluster is good
================================
fat32_init success!
================================
test build passed
test write passed
test close passed
test reopen passed
test read passed
test close again passed
test remove passed
test challenge passed!
================================
this is the message of test
this is the second message of test
Now this is the lyrics
================================
Lavender Haze

Taylor Swift

Meet me at midnight
Oooh oooh oooh woah
Oooh oooh oooh woah
Staring at the ceiling with you
Oh you don't ever say too much
And you don't really read into
My melancholia
I been under scrutiny (yeah oh yeah)
You handle it beautifully (yeah oh yeah)
All this **** is new to me (yeah oh yeah)
I feel the lavender haze creeping up on me
Surreal
I'm damned if I do give a damn what people say
No deal
The 1950s **** they want from me
I just wanna stay in that lavender haze
Oooh oooh oooh woah
Oooh oooh oooh woah
All they keep asking me (all they keep asking me)
Is if I'm gonna be your bride
Surreal
I'm damned if I do give a damn what people say
No deal
The 1950s **** they want from me
I just wanna stay in that lavender haze
(Oooh Oooh Oooh Woah)
The lavender haze
Talk your talk and go viral
I just need this love spiral
Get it off your chest
Get it off my desk
Talk your talk and go viral
I just need this love spiral
Get it off your chest
Get it off my desk
I feel the lavender haze creeping up on me
Surreal
I'm damned if I do give a damn what people say
No deal
The 1950s **** they want from me
I just wanna stay in that lavender haze
Oooh oooh oooh woah
Oooh oooh oooh woah
Get it off your chest
Get it off my desk
The lavender haze
I just wanna stay
I just wanna stay
In that lavender haze
================================
[00000800] destroying 00000800
[00000800] free env 00000800
i am killed ... 
panic at sched.c:40 (schedule): NO RUNNABLE ENV
ra:    80016b08  sp:  803ffe80  Status: 0008100c
Cause: 00000020  EPC: 00406550  BadVA:  00406090
curenv:    NULL
cur_pgdir: 83ffc000
```

![截屏2023-06-17 21.13.26.png](https://github.com/Frankie-Dejong/buaa_os_Mos_withFat32/blob/main/images/截屏2023-06-17%2021.13.26.png?raw=true)

# 附录

## 部分代码

```cpp
//fatfs.h
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
    debugf("%s\n", fat_super.FSType);
    user_assert(strcmp(fat_super.FSType, "FAT32   ") == 0);
    debugf("fat_readBPB is good\n");
    return 0;
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
            debugf("0x%x\n", Clusno2Data(clusno));
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

int fat_dir_lookup(struct Fat32_Dir *pdir, 
		char *name, 
		struct Fat32_Dir ** file, 
		struct Fat32_Dir ** entry) {
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
                *file = (struct Fat32_Dir *)read_data_cluster(targetClusNO);
                return 0;
            }
        }
        dataClusNo = FAT1[dataClusNo];
    } while(dataClusNo != EOF_Fat32);
    return -E_NOT_FOUND;
}
```

## 测试代码

### lab5_challenge_RW

```cpp
#include <lib.h>
void debugfn(char *s, int n)
{
    for(int i = 0;i < n;i ++)
    {
        debugf("%c", s[i]);
    }
    debugf("\n");
}

char *test_root_rw = ":/test_directory_read_and_write";
char *test_write_file = ":/test_directory_read_and_write/test_write_file";
char *test_rw_file = ":/test_directory_read_and_write/test_rw_file";
char *fs_file = "/motd";
char *message = "这世上真话本就不多, 一位女子的脸红便胜过一大段对白\n可后来有了胭脂, 便分不清是真情还是假意\n";

int fdList[10], top = -1;
char buf[200];
int build_dir(char *name) {
    int r = open(name, O_MKDIR);
    fdList[++top] = r;
    return r;
}
int build_file(char *name) {
    int r = open(name, O_CREAT | O_RDWR);
    fdList[++top] = r;
    return r;
}
int open_file(char *name) {
    int r = open(name, O_RDWR);
    fdList[++top] = r;
    return r;
}
void test_build() {
    user_assert(build_dir(test_root_rw) >= 0);
    user_assert(build_file(test_write_file) >= 0);
    user_assert(build_file(test_rw_file) >= 0);
    user_assert(open_file(fs_file) >= 0);
}
void test_write() {
    user_assert(write(fdList[1], message, strlen(message)) > 0);
    user_assert(write(fdList[2], message, strlen(message)) > 0);
    user_assert(write(fdList[3], message, strlen(message)) > 0);
    seek(fdList[1], 0);
    seek(fdList[2], 0);
    seek(fdList[3], 0);
}
void test_read() {
    user_assert(read(fdList[2], buf, strlen(message)) > 0);
    user_assert(strcmp(buf, message) == 0);
    user_assert(read(fdList[3], buf, strlen(message)) > 0);
    user_assert(strcmp(buf, message) == 0);
    seek(fdList[1], 0);
    seek(fdList[2], 0);
    seek(fdList[3], 0);
}

void test_close() {
    for(int i = 0;i <= top;i ++) {
        user_assert(close(fdList[i]) == 0);
    }
}

int main()
{
    test_build();
    debugf("test_build passed\n");
    test_write();
    debugf("test write passed\n");
    test_read();
    debugf("test read passed\n");
    test_close();
    debugf("test close passed\n");
    debugf("================================\n");
    debugf("%s", buf);
    debugf("================================\n");
    return 0;
}
```

### lab5_challenge_at

```cpp
#include <lib.h>

void debugfn(char *s, int n)
{
    for(int i = 0;i < n;i ++)
    {
        debugf("%c", s[i]);
    }
    debugf("\n");
}

char *message1 = "当我还只有六岁的时候，在一本描写原始森林的名叫《真实的故事》的书中， 看到了一副精彩的插画，画的是一条蟒蛇正在吞食一只大野兽。页头上就是那副画的摹本。\n"
"这本书中写道:\"这些蟒蛇把它们的猎获物不加咀嚼地囫囵吞下，尔后就不能再动弹了;它们就在长长的六个月的睡眠中消化这些食物\n\""
"当时,我对丛林中的奇遇想得很多,于是,我也用彩色铅笔画出了我的第一副图画.我的第一号作品.它是这样的:\n"
"        |----------|\n"
"        |          |\n"
"________|__________|___________\n"
"我把我的这副杰作拿给大人看，我问他们我的画是不是叫他们害怕。\n";
char *message2 = 
"他们回答我说:\"一顶帽子有什么可怕的?\""
"我画的不是帽子,是一条巨蟒在消化着一头大象.于是我又把巨蟒肚子里的情况画了出来,以便让大人们能够看懂.这些大人总是需要解释.我的第二号作品是这样的:\n"
"        |--------------|\n"
"        |  ********    |\n"
"        |*   ******    |\n"
"        |    *    *    |\n"
"________|______________|___________\n"
"大人们劝我把这些画着开着肚皮的,或闭上肚皮的蟒蛇的图画放在一边,还是把兴趣放在地理,历史,算术,语法上.就这样,在六岁的那年,我就放弃了当画家这一美好的职业.我的第一号,第二号作品的不成功,使我泄了气.这些大人们,靠他们自己什么也弄不懂，还得老是不断地给他们作解释.这真叫孩子们腻味.\n"
"后来,我只好选择了另外一个职业,我学会了开飞机,世界各地差不多都飞到过.的确,地理学帮了我很大的忙.我一眼就能分辨出中国和亚里桑那.要是夜里迷失了航向,这是很有用的.\n"
"这样,在我的生活中,我跟许多严肃的人有过很多的接触.我在大人们中间生活过很长时间.我仔细地观察过他们,但这并没有使我对他们的看法有多大的改变.\n"
"当我遇到一个头脑看来稍微清楚的大人时,我就拿出一直保存着的我那第一号作品来测试测试他.我想知道他是否真的有理解能力.可是,得到的回答总是:\"这是顶帽子.\"我就不和他谈巨蟒呀,原始森林呀,或者星星之类的事.我只得迁就他们的水平,和他们谈些桥牌呀,高尔夫球呀,政治呀,领带呀这些.于是大人们就十分高兴能认识我这样一个通情达理的人.\n";

char buf0[1000];
char buf1[2000];
char buf3[3000];

char *test_directory_0 = ":/test_directory_at_first";
char *test_file_0_0 = "test_file0_first";
char *test_file_0_1 = "test_file1_first";
char *test_directory_1 = "test_directory_at_second";
char *test_file_1_0 = "test_file0_second";
char *test_file_1_1 = "test_file1_second";

int d0, f00, f01, d1, f10, f11;

int build_dir(char *name) {
    int r;
    r = open(name, O_MKDIR);
    return r;
}

int build_file_at(int fd, char *name) {
    int r;
    r = openat(fd, name, O_CREAT | O_RDWR);
    return r;
}

int build_directory_at(int fd, char *name) {
    int r;
    r = openat(fd, name, O_MKDIR);
    return r;
}

int remove_file_at(int fd, char *name) {
    int r;
    r = removeAt(fd, name);
    return r;
}

void test_build() {
    d0 = build_dir(test_directory_0);
    user_assert(d0 >= 0);
    f00 = build_file_at(d0, test_file_0_0);
    user_assert(f00 >= 0);
    f01 = build_file_at(d0, test_file_0_1);
    user_assert(f01 >= 0);
    d1 = build_directory_at(d0, test_directory_1);
    user_assert(d1 >= 0);
    f10 = build_file_at(d1, test_file_1_0);
    user_assert(f10 >= 0);
    f11 = build_file_at(d1, test_file_1_1);
    user_assert(f11 >= 0);
}

void test_write() {
    user_assert(write(f10, message1, strlen(message1)) > 0);
    user_assert(write(f11, message2, strlen(message2)) > 0);
    seek(f10, 0);
    seek(f11, 0);
}

void test_read() {
    user_assert(read(f10, buf0, strlen(message1)) > 0);
    user_assert(strcmp(message1, buf0) == 0);
    user_assert(read(f11, buf1, strlen(message2)) > 0);
    user_assert(strcmp(message2, buf1) == 0);
    seek(f10, 0);
    seek(f11, 0);
}

void test_remove() {
    user_assert(removeAt(d1, test_file_1_0) == 0);
    user_assert(openat(d1, test_file_1_0, O_RDWR) < 0);
    user_assert(removeAt(d1, test_file_1_1) == 0);
    user_assert(openat(d1, test_file_1_1, O_RDWR) < 0);
}

void test_close() {
    user_assert(close(f10) == 0);
    user_assert(close(f11) == 0);
    user_assert(close(f00) == 0);
    user_assert(close(f01) == 0);
}

int main()
{
    test_build();
    debugf("test build passed\n");
    test_write();
    debugf("test write passed\n");
    test_read();
    debugf("test read passed\n");
    test_close();
    debugf("test close passed\n");
    test_remove();
    debugf("test remove passed\n");
    close(d0);
    close(d1);
    debugf("%s", message1);
    debugf("%s", message2);
    return 0;
}
```

### lab5_challenge

```cpp
#include <lib.h>
void debugfn(char *s, int n)
{
    for(int i = 0;i < n;i ++)
    {
        debugf("%c", s[i]);
    }
    debugf("\n");
}

char *fat_dir0 = ":/test_directory_strong_check";
char *fat_dir1 = ":/test_directory_strong_check/test_directory_1";
char *fat_dir2 = ":/test_directory_strong_check/test_directory_2";
char *fat_dir3 = ":/test_directory_strong_check/test_directory_1/test_directory_3";
char *fat_dir4 = ":/test_directory_strong_check/test_directory_1/test_directory_3/test_directory_4";
char *fat_file0 = ":/test_directory_strong_check/test_fat_file0";
char *fat_file1 = ":/test_directory_strong_check/test_directory_1/test_fat_file1";
char *fat_file2 = ":/test_directory_strong_check/test_directory_1/test_fat_file2";
char *fat_file3 = ":/test_directory_strong_check/test_directory_1/test_fat_file3";
char *fat_file4 = ":/test_directory_strong_check/test_directory_2/test_fat_file4";
char *fat_file5 = ":/test_directory_strong_check/test_directory_1/test_directory_3/test_fat_file5";
char *fat_file6 = ":/test_directory_strong_check/test_directory_1/test_directory_3/test_fat_file6";
char *fat_file7 = ":/test_directory_strong_check/test_directory_1/test_directory_3/test_directory_4/test_fat_file7";
char *fat_file8 = ":/test_directory_strong_check/test_directory_1/test_directory_3/test_directory_4/test_fat_file8";

char *message0 = "this is the message of test\n";
char *message1 = "this is the second message of test\nNow this is the lyrics\n================================\n";
char *message2 = "Lavender Haze\n\n";
char *message3 = "Taylor Swift\n\nMeet me at midnight\nOooh oooh oooh woah\nOooh oooh oooh woah\nStaring at the ceiling with you\nOh you don't ever say too much\nAnd you don't really read into\nMy melancholia\nI been under scrutiny (yeah oh yeah)\nYou handle it beautifully (yeah oh yeah)\nAll this **** is new to me (yeah oh yeah)\nI feel the lavender haze creeping up on me\n";
char *message4 = "Surreal\nI'm damned if I do give a damn what people say\nNo deal\nThe 1950s **** they want from me\nI just wanna stay in that lavender haze\nOooh oooh oooh woah\nOooh oooh oooh woah\nAll they keep asking me (all they keep asking me)\nIs if I'm gonna be your bride\n";
char *message5 = "Surreal\nI'm damned if I do give a damn what people say\nNo deal\nThe 1950s **** they want from me\nI just wanna stay in that lavender haze\n(Oooh Oooh Oooh Woah)\nThe lavender haze\nTalk your talk and go viral\nI just need this love spiral\nGet it off your chest\nGet it off my desk\nTalk your talk and go viral\nI just need this love spiral\nGet it off your chest\nGet it off my desk\nI feel the lavender haze creeping up on me\n";
char *message6 = "Surreal\nI'm damned if I do give a damn what people say\nNo deal\nThe 1950s **** they want from me\nI just wanna stay in that lavender haze\n";
char *message7 = "Oooh oooh oooh woah\nOooh oooh oooh woah\nGet it off your chest\nGet it off my desk\n";
char *message8 = "The lavender haze\nI just wanna stay\nI just wanna stay\nIn that lavender haze\n";

char buf[10][500];

int fdList[20], top = -1;
int build_dir(char *name) {
    int r = open(name, O_MKDIR);
    fdList[++top] = r;
    return r;
}
int build_file(char *name) {
    int r = open(name, O_CREAT | O_RDWR);
    fdList[++top] = r;
    return r;
}
int open_file(char *name) {
    int r = open(name, O_RDWR);
    fdList[++top] = r;
    return r;
}
void test_build() {
    top = -1;
    user_assert(build_dir(fat_dir0) >= 0);
    user_assert(build_dir(fat_dir1) >= 0);
    user_assert(build_dir(fat_dir2) >= 0);
    user_assert(build_dir(fat_dir3) >= 0);
    user_assert(build_dir(fat_dir4) >= 0);
    user_assert(build_file(fat_file0) >= 0);
    user_assert(build_file(fat_file1) >= 0);
    user_assert(build_file(fat_file2) >= 0);
    user_assert(build_file(fat_file3) >= 0);
    user_assert(build_file(fat_file4) >= 0);
    user_assert(build_file(fat_file5) >= 0);
    user_assert(build_file(fat_file6) >= 0);
    user_assert(build_file(fat_file7) >= 0);
    user_assert(build_file(fat_file8) >= 0);
}

void test_write() {
    int i = 5;
    user_assert(write(fdList[i ++], message0, strlen(message0)) >= 0);
    user_assert(write(fdList[i ++], message1, strlen(message1)) >= 0);
    user_assert(write(fdList[i ++], message2, strlen(message2)) >= 0);
    user_assert(write(fdList[i ++], message3, strlen(message3)) >= 0);
    user_assert(write(fdList[i ++], message4, strlen(message4)) >= 0);
    user_assert(write(fdList[i ++], message5, strlen(message5)) >= 0);
    user_assert(write(fdList[i ++], message6, strlen(message6)) >= 0);
    user_assert(write(fdList[i ++], message7, strlen(message7)) >= 0);
    user_assert(write(fdList[i ++], message8, strlen(message8)) >= 0);
}

void test_reopen() {
    top = -1;
    user_assert(open_file(fat_dir0) >= 0);
    user_assert(open_file(fat_dir1) >= 0);
    user_assert(open_file(fat_dir2) >= 0);
    user_assert(open_file(fat_dir3) >= 0);
    user_assert(open_file(fat_dir4) >= 0);
    user_assert(open_file(fat_file0) >= 0);
    user_assert(open_file(fat_file1) >= 0);
    user_assert(open_file(fat_file2) >= 0);
    user_assert(open_file(fat_file3) >= 0);
    user_assert(open_file(fat_file4) >= 0);
    user_assert(open_file(fat_file5) >= 0);
    user_assert(open_file(fat_file6) >= 0);
    user_assert(open_file(fat_file7) >= 0);
    user_assert(open_file(fat_file8) >= 0);
}

void test_read() {
    int i = 5;
    int j = 0;
    user_assert(read(fdList[i ++], buf[j ++], strlen(message0)) >= 0);
    user_assert(strcmp(message0, buf[0]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message1)) >= 0);
    user_assert(strcmp(message1, buf[1]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message2)) >= 0);
    user_assert(strcmp(message2, buf[2]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message3)) >= 0);
    user_assert(strcmp(message3, buf[3]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message4)) >= 0);
    user_assert(strcmp(message4, buf[4]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message5)) >= 0);
    user_assert(strcmp(message5, buf[5]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message6)) >= 0);
    user_assert(strcmp(message6, buf[6]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message7)) >= 0);
    user_assert(strcmp(message7, buf[7]) == 0);
    user_assert(read(fdList[i ++], buf[j ++], strlen(message8)) >= 0);
    user_assert(strcmp(message8, buf[8]) == 0);
}

void test_close() {
    for(int i = 0;i <= top;i ++) {
        int r = close(fdList[i]);
        user_assert(r == 0);
    }
}

void test_remove() {
    user_assert(remove(fat_file0) == 0);
    user_assert(remove(fat_file2) == 0);
    user_assert(remove(fat_file4) == 0);
    user_assert(remove(fat_file6) == 0);
    user_assert(remove(fat_file8) == 0);
    user_assert((open(fat_file0, O_RDWR)) < 0);
    user_assert((open(fat_file2, O_RDWR)) < 0);
    user_assert((open(fat_file4, O_RDWR)) < 0);
    user_assert((open(fat_file6, O_RDWR)) < 0);
    user_assert((open(fat_file8, O_RDWR)) < 0);
}

void show_lyrics() {
    debugf("================================\n");
    for(int i = 0;i <= 8;i ++) {
        debugf("%s", buf[i]);
    }
    debugf("================================\n");
}
int main()
{
    test_build();
    debugf("test build passed\n");
    test_write();
    debugf("test write passed\n");
    test_close();
    debugf("test close passed\n");
    test_reopen();
    debugf("test reopen passed\n");
    test_read();
    debugf("test read passed\n");
    test_close();
    debugf("test close again passed\n");
    test_remove();
    debugf("test remove passed\n");
    debugf("test challenge passed!\n");
    show_lyrics();
    return 0;
}
```
