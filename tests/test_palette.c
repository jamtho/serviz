#include <assert.h>
#include <stdio.h>
#include "palette.h"

static void test_all_distinct(void) {
    const Color *pal = palette_get_default();
    for (int i = 0; i < PALETTE_SIZE; i++) {
        for (int j = i + 1; j < PALETTE_SIZE; j++) {
            assert(pal[i].r != pal[j].r ||
                   pal[i].g != pal[j].g ||
                   pal[i].b != pal[j].b);
        }
    }
    printf("PASS: all distinct\n");
}

static void test_alpha(void) {
    for (int i = 0; i < PALETTE_SIZE; i++) {
        assert(palette_color(i).a == 255);
    }
    printf("PASS: alpha\n");
}

static void test_wrap(void) {
    Color c0 = palette_color(0);
    Color c10 = palette_color(10);
    assert(c0.r == c10.r && c0.g == c10.g && c0.b == c10.b);

    Color c3 = palette_color(3);
    Color c13 = palette_color(13);
    assert(c3.r == c13.r && c3.g == c13.g && c3.b == c13.b);
    printf("PASS: wrap\n");
}

int main(void) {
    test_all_distinct();
    test_alpha();
    test_wrap();
    printf("All palette tests passed.\n");
    return 0;
}
