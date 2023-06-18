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


