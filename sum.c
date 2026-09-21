#include <stdio.h>
#include <stdbool.h>
#include <string.h>

int main(void) {
    long long n = 0;
    long long total = 0;
    long long i = 0;

    if (scanf("%lld", &n) != 1) { n = 0; }
    total = 0LL;
    {
        long long pc_end1 = n;
        for (i = 1LL; i <= pc_end1; i += 1) {
            total = (total + i);
        }
    }
    printf("Sum is %lld\n", total);
    return 0;
}
