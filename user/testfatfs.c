#include <lib.h>
int main()
{
    int r = open(":/test_fat/testfile", O_RDWR);
    debugf("%d\n", r);
    return 0;
}