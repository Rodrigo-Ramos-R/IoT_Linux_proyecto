#include "voice_recognition.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define VR_MAX_SAMPLES   200
#define VR_NUM_LABELS    4
#define VR_NUM_FEATURES  6


// ------------------------------
// Model storage
// ------------------------------
typedef struct {
    int num_samples;
    int num_features;
    int k;
    float X[VR_MAX_SAMPLES][VR_NUM_FEATURES];
    int   y[VR_MAX_SAMPLES];
    int   loaded;
} VR_Model;

static VR_Model vr_model = {0};


// ========================================================
// MODEL LOADING
// ========================================================
static int VR_Load_Model(const char *path)
{
    if (vr_model.loaded) {
        printf("[VR-DEBUG] Model already loaded, skipping reload.\n");
        return 0;
    }

    printf("[VR-DEBUG] Opening model file: %s\n", path);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("[VR] Failed to open model file");
        return -1;
    }

    int N, D, K;
    if (fscanf(fp, "%d %d %d", &N, &D, &K) != 3) {
        printf("[VR-DEBUG] ERROR: Invalid model header\n");
        fclose(fp);
        return -1;
    }

    printf("[VR-DEBUG] Model header: N=%d  D=%d  K=%d\n", N, D, K);

    if (N > VR_MAX_SAMPLES || D != VR_NUM_FEATURES) {
        printf("[VR-DEBUG] ERROR: Model dimensions incorrect\n");
        fclose(fp);
        return -1;
    }

    vr_model.num_samples  = N;
    vr_model.num_features = D;
    vr_model.k            = K;

    for (int i = 0; i < N; i++) {
        if (fscanf(fp, "%d", &vr_model.y[i]) != 1) {
            printf("[VR-DEBUG] ERROR: Reading label row %d\n", i);
            fclose(fp);
            return -1;
        }
        for (int j = 0; j < D; j++) {
            if (fscanf(fp, "%f", &vr_model.X[i][j]) != 1) {
                printf("[VR-DEBUG] ERROR: Reading feature %d row %d\n", j, i);
                fclose(fp);
                return -1;
            }
        }
    }

    fclose(fp);
    vr_model.loaded = 1;

    printf("[VR-DEBUG] Model loaded successfully (%d samples)\n", N);
    return 0;
}



// ========================================================
// SAFE DOWNSAMPLING (48 kHz → 16 kHz) 
// ========================================================
static float *VR_Downsample_48k_to_16k(float *audio, long num_samples, long *out_newN)
{
    long newN = num_samples / 3;
    float *down = malloc(newN * sizeof(float));
    if (!down) {
        printf("[VR-DEBUG] ERROR: malloc failed for downsample buffer\n");
        return NULL;
    }

    for (long i = 0, j = 0; i < newN; i++, j += 3)
        down[i] = audio[j];

    *out_newN = newN;
    return down;
}



// ========================================================
// FEATURE EXTRACTION (DEBUG VERSION)
// ========================================================
static int VR_Extract_Features(const char *wav_path, float feats[VR_NUM_FEATURES])
{
    printf("[VR-DEBUG] Opening WAV file: %s\n", wav_path);

    FILE *fp = fopen(wav_path, "rb");
    if (!fp) {
        perror("[VR] Failed to open WAV file");
        return -1;
    }

    // ---------------------------------------
    // File size
    // ---------------------------------------
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    printf("[VR-DEBUG] WAV filesize = %ld bytes\n", filesize);

    if (filesize < 44) {
        printf("[VR-DEBUG] ERROR: WAV file too small\n");
        fclose(fp);
        return -1;
    }

    // ---------------------------------------
    // Dump first 64 bytes of header
    // ---------------------------------------
    printf("[VR-DEBUG] WAV header dump:\n");
    unsigned char header[64];
    fread(header, 1, 64, fp);
    for (int i = 0; i < 64; i++) {
        printf("%02X ", header[i]);
        if ((i % 16) == 15) printf("\n");
    }
    printf("\n");

    rewind(fp);

    long data_start = 44; 
    printf("[VR-DEBUG] data_start = %ld\n", data_start);

    if (filesize <= data_start) {
        printf("[VR-DEBUG] ERROR: File smaller than header\n");
        fclose(fp);
        return -1;
    }

    fseek(fp, data_start, SEEK_SET);
    long data_bytes = filesize - data_start;
    long num_samples = data_bytes / 2;

    printf("[VR-DEBUG] data_bytes=%ld  num_samples=%ld (48kHz)\n",
           data_bytes, num_samples);

    // ---------------------------------------
    // Read PCM
    // ---------------------------------------
    int16_t *raw = malloc(num_samples * sizeof(int16_t));
    if (!raw) {
        printf("[VR-DEBUG] ERROR: malloc failed for raw\n");
        fclose(fp);
        return -1;
    }

    long nread = fread(raw, sizeof(int16_t), num_samples, fp);
    fclose(fp);

    printf("[VR-DEBUG] fread returned %ld samples\n", nread);
    if (nread < num_samples)
        num_samples = nread;

    // Convert to float
    float *audio = malloc(num_samples * sizeof(float));
    if (!audio) {
        printf("[VR-DEBUG] ERROR: malloc failed for audio\n");
        free(raw);
        return -1;
    }

    for (long i = 0; i < num_samples; i++)
        audio[i] = raw[i] / 32768.0f;

    free(raw);

    // ---------------------------------------
    // SAFE DOWNSAMPLING
    // ---------------------------------------
    printf("[VR-DEBUG] Starting downsample...\n");
    long newN = 0;
    float *down = VR_Downsample_48k_to_16k(audio, num_samples, &newN);

    free(audio);

    if (!down) {
        printf("[VR-DEBUG] ERROR: Downsample failed\n");
        return -1;
    }

    audio = down;
    num_samples = newN;

    printf("[VR-DEBUG] Downsampled length = %ld samples\n", num_samples);

    long N = num_samples;

    // ---------------------------------------
    // Feature extraction
    // ---------------------------------------
    printf("[VR-DEBUG] Extracting features...\n");

    // 1. Energy
    double energy = 0.0;
    for (long i = 0; i < N; i++)
        energy += audio[i] * audio[i];
    energy /= N;

    // 2. Zero-crossing rate
    long zc = 0;
    for (long i = 1; i < N; i++) {
        int s1 = (audio[i-1] >= 0.0f);
        int s2 = (audio[i]   >= 0.0f);
        if (s1 != s2) zc++;
    }
    float zcr = (float)zc / (float)(N - 1);

    // 3. Mean absolute
    double mean_abs = 0.0;
    for (long i = 0; i < N; i++)
        mean_abs += fabsf(audio[i]);
    mean_abs /= N;

    // 4. STD
    double mean_val = 0.0;
    for (long i = 0; i < N; i++)
        mean_val += audio[i];
    mean_val /= N;

    double var = 0.0;
    for (long i = 0; i < N; i++) {
        double d = audio[i] - mean_val;
        var += d * d;
    }
    var /= N;
    float std_amp = sqrtf(var);

    // 5. Envelope slope
    int segs = 50;
    long seg_len = N / segs;
    if (seg_len < 1) seg_len = 1;

    float first_env = 0.0f;
    float last_env  = 0.0f;

    for (int s = 0; s < segs; s++) {
        long start = s * seg_len;
        long end   = start + seg_len;
        if (start >= N) break;
        if (end > N) end = N;

        double sum_abs = 0.0;
        for (long i = start; i < end; i++)
            sum_abs += fabsf(audio[i]);
        float env = sum_abs / (double)(end - start);

        if (s == 0) first_env = env;
        last_env = env;
    }

    float slope = last_env - first_env;

    // 6. Duration
    float duration = (float)N / 16000.0f;

    free(audio);

    feats[0] = (float)energy;
    feats[1] = zcr;
    feats[2] = (float)mean_abs;
    feats[3] = std_amp;
    feats[4] = slope;
    feats[5] = duration;

    printf("[VR-DEBUG] Feature extraction complete.\n");
    for (int i = 0; i < VR_NUM_FEATURES; i++)
        printf("  feats[%d] = %f\n", i, feats[i]);

    return 0;
}



// ========================================================
// KNN PREDICTION
// ========================================================
static int VR_Predict_Label(const float feats[VR_NUM_FEATURES])
{
    int N = vr_model.num_samples;
    int D = vr_model.num_features;
    int k = vr_model.k;

    float dist[VR_MAX_SAMPLES];
    int   idx[VR_MAX_SAMPLES];

    for (int i = 0; i < N; i++) {
        double s = 0.0;
        for (int j = 0; j < D; j++) {
            double d = feats[j] - vr_model.X[i][j];
            s += d * d;
        }
        dist[i] = s;
        idx[i] = i;
    }

    // Simple selection sort
    for (int i = 0; i < N - 1; i++) {
        int min_i = i;
        for (int j = i + 1; j < N; j++)
            if (dist[j] < dist[min_i]) min_i = j;

        float tmpd = dist[i]; dist[i] = dist[min_i]; dist[min_i] = tmpd;
        int   tmpi = idx[i];  idx[i] = idx[min_i];  idx[min_i] = tmpi;
    }

    int votes[VR_NUM_LABELS] = {0};
    for (int i = 0; i < k; i++)
        votes[vr_model.y[idx[i]]]++;

    int best = 0;
    for (int i = 1; i < VR_NUM_LABELS; i++)
        if (votes[i] > votes[best]) best = i;

    return best;
}



// ========================================================
// PUBLIC ENTRY POINT
// ========================================================
int VR_Recognize_Command(const char *wav_path)
{
    printf("[VR-DEBUG] VR_Recognize_Command('%s')\n", wav_path);

    if (VR_Load_Model("/home/root/model_knn.txt") != 0) {
        printf("[VR-DEBUG] Model load FAILED\n");
        return -1;
    }

    printf("[VR-DEBUG] Model loaded OK\n");

    float feats[VR_NUM_FEATURES];
    printf("[VR-DEBUG] Calling VR_Extract_Features...\n");

    if (VR_Extract_Features(wav_path, feats) != 0) {
        printf("[VR-DEBUG] VR_Extract_Features FAILED\n");
        return -1;
    }

    printf("[VR-DEBUG] Calling VR_Predict_Label...\n");
    int label = VR_Predict_Label(feats);

    printf("[VR-DEBUG] Prediction = %d\n", label);
    return label;
}
