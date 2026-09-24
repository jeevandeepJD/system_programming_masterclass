#include <stdio.h>

#define SCALE(v) ((v) * 3)

extern int shared_counter;
extern int external_adjust(int);

static const char banner[] = "source -> object";
static int local_bias = 7;

int transform(int value)
{
    return external_adjust(SCALE(value) + local_bias + shared_counter);
}

int main(void)
{
    printf("%s: %d\n", banner, transform(5));
    return 0;
}
