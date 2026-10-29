/* All the on-board "intelligence". The numbers come from model.h,
   which pc/train.py writes. */
#include <math.h>
#include "classify.h"
#include "config.h"
#include "features.h"
#include "model.h"

static float taught_x[MAX_TAUGHT][N_FEAT];
static unsigned char taught_y[MAX_TAUGHT];
static int n_taught = 0;
static int n_new_classes = 0;
static const char *const TAUGHT_NAME[] = {"TAUGHT_A", "TAUGHT_B", "TAUGHT_C", "TAUGHT_D"};

/* Healthy baseline in RAM: starts as the trained one, self-calibration can replace it */
static float hb_mean[N_FEAT], hb_std[N_FEAT];

/* Calibration baseline: frequency is divided by base_f1 and damping has base_damp
   taken away, so the model reads a healthy beam as 1.0 and 0.0. Starts at the trained
   values; calib_finish() replaces them with this beam's healthy median. */
static float base_f1 = BASE_F1, base_damp = BASE_DAMP;
static float cal_f1[CALIB_FLICKS], cal_damp[CALIB_FLICKS];
static int n_cal = 0;

/* raw feature vector (from extract_features) -> the normalized one the models use */
static void norm_feat(const float *raw, float *out)
{
    out[0] = raw[F_FREQ] / base_f1;
    out[1] = raw[F_DAMP] - base_damp;
}

static float median(float *v, int n)
{
    int i, j;
    float t;
    for (i = 1; i < n; i++)                          /* insertion sort, n is 10 */
        for (j = i; j > 0 && v[j - 1] > v[j]; j--) { t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    return (n & 1) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

void classify_init(void)
{
    int i;
    for (i = 0; i < N_FEAT; i++) { hb_mean[i] = HB_MEAN[i]; hb_std[i] = HB_STD[i]; }
}

void calib_start(void)
{
    n_cal = 0;
}

int calib_add(const float *raw)
{
    if (n_cal < CALIB_FLICKS) {
        cal_f1[n_cal] = raw[F_FREQ];
        cal_damp[n_cal] = raw[F_DAMP];
        n_cal++;
    }
    return n_cal;
}

void calib_finish(void)
{
    float nf[N_FEAT], sum[N_FEAT] = {0.0f, 0.0f}, sq[N_FEAT] = {0.0f, 0.0f};
    float raw[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int i, k;
    float m, v;
    if (n_cal < 2) return;
    base_f1 = median(cal_f1, n_cal);
    base_damp = median(cal_damp, n_cal);
    for (k = 0; k < n_cal; k++) {
        raw[F_FREQ] = cal_f1[k];
        raw[F_DAMP] = cal_damp[k];
        norm_feat(raw, nf);
        for (i = 0; i < N_FEAT; i++) { sum[i] += nf[i]; sq[i] += nf[i] * nf[i]; }
    }
    for (i = 0; i < N_FEAT; i++) {
        m = sum[i] / (float)n_cal;
        v = sq[i] / (float)n_cal - m * m;
        hb_mean[i] = m;
        v = v > 0.0f ? sqrtf(v) : 0.0f;
        /* a few flicks can under-estimate spread, so never go below half the trained one */
        hb_std[i] = v > 0.5f * HB_STD[i] ? v : 0.5f * HB_STD[i];
    }
}

static void scale(const float *f, float *xs)
{
    int i;
    for (i = 0; i < N_FEAT; i++) xs[i] = (f[i] - SC_MEAN[i]) / SC_STD[i];
}

static float dist(const float *a, const float *b)
{
    int i;
    float s = 0.0f, d;
    for (i = 0; i < N_FEAT; i++) { d = a[i] - b[i]; s += d * d; }
    return sqrtf(s);
}

static int nn_predict(const float *xs, float *conf)
{
    int i, j, best = 0;
    float h[N_HIDDEN], o[N_CLASS], s, mx, sum;
    for (j = 0; j < N_HIDDEN; j++) {
        s = NN_B1[j];
        for (i = 0; i < N_FEAT; i++) s += NN_W1[j][i] * xs[i];
        h[j] = s > 0.0f ? s : 0.0f;                     /* ReLU */
    }
    for (j = 0; j < N_CLASS; j++) {
        s = NN_B2[j];
        for (i = 0; i < N_HIDDEN; i++) s += NN_W2[j][i] * h[i];
        o[j] = s;
        if (o[j] > o[best]) best = j;
    }
    mx = o[best]; sum = 0.0f;                           /* softmax for confidence */
    for (j = 0; j < N_CLASS; j++) sum += expf(o[j] - mx);
    *conf = 1.0f / sum;
    return best;
}

/* keeps the k smallest distances in d[] with their labels */
static void push(float dd, int lab, float *d, int *l, int k)
{
    int i = k - 1;
    if (dd >= d[i]) return;
    while (i > 0 && d[i - 1] > dd) { d[i] = d[i - 1]; l[i] = l[i - 1]; i--; }
    d[i] = dd; l[i] = lab;
}

void classify(const float *raw, Result *r)
{
    float feat[N_FEAT], xs[N_FEAT], d3[3], dd, z, s, votes[N_CLASS + 4];
    float dmg_d[3], dmg_v[3], dmg;
    int l3[3], dmg_i[3], i, best;

    norm_feat(raw, feat);
    scale(feat, xs);
    r->tree = tree_predict(feat);
    r->nn = nn_predict(xs, &r->nn_conf);

    /* kNN over stored + taught flicks; also health from the 3 nearest labelled ones */
    for (i = 0; i < 3; i++) { d3[i] = 1e30f; l3[i] = 0; dmg_d[i] = 1e30f; dmg_i[i] = 0; }
    for (i = 0; i < KNN_N; i++) {
        dd = dist(xs, KNN_X[i]);
        push(dd, KNN_Y[i], d3, l3, 3);
        if (KNN_DAMAGE[i] >= 0.0f) push(dd, i, dmg_d, dmg_i, 3);
    }
    for (i = 0; i < n_taught; i++) push(dist(xs, taught_x[i]), taught_y[i], d3, l3, 3);

    for (i = 0; i < N_CLASS + 4; i++) votes[i] = 0.0f;
    for (i = 0; i < 3; i++) votes[l3[i]] += 1.0f;
    best = l3[0];                                       /* ties go to the nearest */
    for (i = 0; i < N_CLASS + 4; i++) if (votes[i] > votes[best]) best = i;
    r->knn = best;
    r->novelty = d3[0];

    for (i = 0; i < 3; i++) dmg_v[i] = KNN_DAMAGE[dmg_i[i]];
    dmg = (dmg_v[0] + dmg_v[1] + dmg_v[2]) / 3.0f;
    r->health = 100.0f * (1.0f - dmg);
    if (r->health < 0.0f) r->health = 0.0f;
    if (r->health > 100.0f) r->health = 100.0f;

    /* change detector: RMS z-score from the healthy baseline */
    s = 0.0f;
    for (i = 0; i < N_FEAT; i++) { z = (feat[i] - hb_mean[i]) / hb_std[i]; s += z * z; }
    r->change = sqrtf(s / (float)N_FEAT);

    r->changed = r->change > CHANGE_THR;
    r->unknown = r->novelty > NOVELTY_THR;
    r->unsafe = r->health < UNSAFE_HEALTH;
}

const char *class_name(int id)
{
    if (id < N_CLASS) return CLASS_NAME[id];
    if (id - N_CLASS < 4) return TAUGHT_NAME[id - N_CLASS];
    return "?";
}

int class_is_taught(int id)
{
    return id >= N_CLASS;
}

int teach_new_class(void)
{
    if (n_new_classes >= 4) return -1;
    return N_CLASS + n_new_classes++;
}

int teach_add(const float *raw, int class_id)
{
    float feat[N_FEAT];
    if (n_taught >= MAX_TAUGHT) return 0;
    norm_feat(raw, feat);
    scale(feat, taught_x[n_taught]);
    taught_y[n_taught++] = (unsigned char)class_id;
    return 1;
}
