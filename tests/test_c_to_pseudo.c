#include <stdio.h>
#include <stdbool.h>

struct Point {
    int x;
    int y;
};

int add(int a, int b) {
    return a + b;
}

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main(void) {
    int x = 10;
    int y = 20;
    printf("Before swap: %d, %d\n", x, y);
    swap(&x, &y);
    printf("After swap: %d, %d\n", x, y);

    int sum = 0;
    for (int i = 1; i <= 5; i++) {
        sum += i;
    }
    printf("Sum: %d\n", sum);

    int count = 0;
    do {
        count += 2;
    } while (count < 6);
    printf("Count: %d\n", count);

    int val = 2;
    switch (val) {
        case 1:
            printf("One\n");
            break;
        case 2:
            printf("Two\n");
            break;
        default:
            printf("Other\n");
            break;
    }

    int res = add(x, y);
    printf("Result: %d\n", res);
    return 0;
}
