#include <stdio.h>
#include <string.h>

int main()
{
    char buf[80];
    sprintf(buf, "123456789");
    printf("buf len: %d, %d\n", strlen(buf), strlen(buf + 2));
    return 0;
}