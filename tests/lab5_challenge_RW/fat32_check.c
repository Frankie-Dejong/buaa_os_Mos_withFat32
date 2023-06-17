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







