/* Laptop test: runs the exact board code on recorded flick files.
   Build: gcc -O2 -Wall -Wdeclaration-after-statement board/host_test.c board/features.c board/classify.c -lm -o host_test
   Run:   ./host_test data/sim/FILES.txt                                       */
#include <stdio.h>
#include "config.h"
#include "features.h"
#include "classify.h"

int main(int argc, char **argv)
{
    static unsigned short adc[N];
    float f[5];
    Result r;
    int a, n, v;
    FILE *fp;

    classify_init();
    printf("file,f1_hz,damping,amp_g,energy_ratio,decay_s,tree,nn,nn_conf,knn,health,change,novelty,changed,unknown\n");
    for (a = 1; a < argc; a++) {
        fp = fopen(argv[a], "r");
        if (!fp) { fprintf(stderr, "cannot open %s\n", argv[a]); continue; }
        for (n = 0; n < N && fscanf(fp, "%d", &v) == 1; n++) adc[n] = (unsigned short)v;
        fclose(fp);
        if (n != N) { fprintf(stderr, "%s: only %d samples\n", argv[a], n); continue; }
        extract_features(adc, f);
        classify(f, &r);
        printf("%s,%.6g,%.6g,%.6g,%.6g,%.6g,%s,%s,%.3f,%s,%.1f,%.3f,%.3f,%d,%d\n", argv[a],
               f[0], f[1], f[2], f[3], f[4], class_name(r.tree), class_name(r.nn), r.nn_conf,
               class_name(r.knn), r.health, r.change, r.novelty, r.changed, r.unknown);
    }
    return 0;
}
