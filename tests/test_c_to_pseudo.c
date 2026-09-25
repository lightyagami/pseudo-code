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

    int arr[5] = {10, 20, 30, 40, 50};
    int arrSum = 0;
    for (int k = 0; k < 5; ++k) {
        arrSum += arr[k];
    }
    printf("ArrSum: %d\n", arrSum);

    int downSum = 0;
    for (int j = 10; j > 0; j -= 2) {
        downSum += j;
    }
    printf("DownSum: %d\n", downSum);

    int stepCount = 0;
    for (; stepCount < 3;) {
        stepCount++;
    }
    printf("StepCount: %d\n", stepCount);

    int modVal = 17;
    modVal %= 5;
    printf("ModVal: %d\n", modVal);

    return 0;
}
