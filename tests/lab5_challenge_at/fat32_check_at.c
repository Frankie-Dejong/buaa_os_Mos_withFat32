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


