#ifndef CLASSIFY_H
#define CLASSIFY_H

#define MAX_TAUGHT 40          /* teach-mode memory, flicks */

typedef struct {
    int   tree;        /* decision tree class */
    int   nn;          /* neural network class */
    float nn_conf;     /* neural network confidence, 0..1 */
    int   knn;         /* kNN class (can be a taught class) */
    float health;      /* 0..100 % */
    float change;      /* distance from healthy baseline */
    float novelty;     /* distance to nearest known flick */
    int   changed;     /* 1 = differs from the healthy baseline */
    int   unknown;     /* 1 = unlike anything it has learned */
    int   unsafe;      /* 1 = health below UNSAFE_HEALTH */
} Result;

void        classify_init(void);   /* call once at start-up */
void        classify(const float *feat, Result *r);
const char *class_name(int id);
int         class_is_taught(int id);

/* Teach mode: store this flick under a new class (id returned by teach_new_class). */
int  teach_new_class(void);
int  teach_add(const float *feat, int class_id);

/* Self-calibration: learn a new healthy baseline from a few flicks. */
void calib_start(void);
int  calib_add(const float *raw);     /* raw features; returns flicks collected so far */
void calib_finish(void);

#endif
