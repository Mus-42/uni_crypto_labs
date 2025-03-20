#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char* read_sample_data(const char* filename) {
    FILE* f = fopen(filename, "r");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = (char*)malloc(len+1);
    fread(data, 1, len, f);
    fclose(f);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if ('a' <= data[i] && data[i] <= 'z') {
            data[j++] = data[i];
        } else if ('A' <= data[i] && data[i] <= 'Z'){
            data[j++] = tolower(data[i]);
        }
    }
    data[j] = 0;
    return data;
}

void encrypt(const char* key, char* message) {
    size_t key_len = strlen(key);
    size_t msg_len = strlen(message);
    for (size_t i = 0; i < msg_len; i++) {
        message[i] = 'a' + ((int)key[i % key_len] + (int)message[i] - 2 * 'a') % 26;
    }
}

#define KEY_BUF_LEN 256
char reverse_key[KEY_BUF_LEN];

void decrypt(const char* key, char* message) {
    size_t key_len = strlen(key);
    assert(key_len < KEY_BUF_LEN);
    for (size_t i = 0; i < key_len; i++) {
        reverse_key[i] = (26 + 'a' - (int)key[i]) % 26 + 'a';
    }
    reverse_key[key_len] = 0;
    encrypt(reverse_key, message);
}

// https://en.wikipedia.org/wiki/Index_of_coincidence
static double compute_ic_impl(const char* text, size_t offset, size_t stride) {
    unsigned freq[26];
    memset(freq, 0, sizeof (unsigned) * 26);
    size_t len = strlen(text);
    for (size_t i = offset; i < len; i += stride) {
        freq[text[i] - 'a'] += 1;
    }
    double ic = 0.;
    size_t count = (len - offset) / stride;
    for (size_t i = 0; i < 26; i++) {
        ic += (double)(freq[i] * (freq[i] - 1)) / (double)(count * (count - 1));
    }
    return ic * 26.;
}

double compute_ic(const char* text, size_t stride) {
    double ic = 0.;
    for (size_t i = 0; i < stride; i++) {
        ic += compute_ic_impl(text, i, stride);
    }
    return ic / (double)stride;
}

// https://en.wikipedia.org/wiki/Pearson%27s_chi-squared_test
double chi_squared_test(size_t n, const double* p, const double* x, unsigned samples) {
    double chisqr = 0;
    for (size_t i = 0; i < n; i++) {
        double expected = (double)samples * p[i];
        double diff = x[i] - expected;
        chisqr += diff * diff / expected;
    }
    return chisqr;
}

// https://en.wikipedia.org/wiki/Letter_frequency
// https://web.archive.org/web/20080708193159/http://pages.central.edu/emp/LintonT/classes/spring01/cryptography/letterfreq.html
static const double expected_freq[26] = {
    0.08167, 0.01492, 0.02782, 0.04253, 0.12702, 0.02228, 0.02015, 0.06094, 0.06966, 0.00153, 0.00772, 0.04025, 0.02406, 0.06749, 0.07507, 0.01929, 0.00095, 0.05987, 0.06327, 0.09056, 0.02758, 0.00978, 0.02360, 0.00150, 0.01974, 0.00074
};

char break_caesar(const char* text, size_t offset, size_t stride) {
    unsigned freq[26];
    memset(freq, 0, sizeof (unsigned) * 26);
    size_t len = strlen(text);
    for (size_t i = offset; i < len; i += stride) {
        freq[text[i] - 'a'] += 1;
    }
    size_t count = (len - offset) / stride;

    double min_error = 1e9;
    size_t best_shift = 0;
    double x[26];
    for (size_t i = 0; i < 26; i++) {
        // compute freq for caesar subtition with offset i
        for (size_t j = 0; j < 26; j++) { 
            x[(j + 26 - i) % 26] = (double)freq[j];
        }

        double chisqr = chi_squared_test(26, expected_freq, x, count);
        if (chisqr < min_error) {
            min_error = chisqr;
            best_shift = i;
        }
    }

    return 'a' + (char)best_shift;
}

#define IC_THRESHOLD 1.6

const char* break_vigenere(const char* text) {
    size_t key_len = 0;
    while (key_len < KEY_BUF_LEN) {
        key_len += 1;
        double ic = compute_ic(text, key_len);
        if (ic > IC_THRESHOLD) break;
    } 
    if (key_len == KEY_BUF_LEN) return NULL;
    char* key = malloc(key_len+1);
    for (size_t i = 0; i < key_len; i++) {
        key[i] = break_caesar(text, i, key_len);   
    }
    key[key_len] = 0;
    return key;
}

#define PASSWORD "abcd"
//#define PASSWORD "password"
//#define PASSWORD "averyverylongpassword"

int main(int argc, const char** argv) {
    // TODO unhardcode filename (through args?)
    //char* data = read_sample_data("data/rfc8446.txt");
    char* data = read_sample_data("data/sillymessage.txt");
    
    printf("len(data) = %zu\n", strlen(data));
    encrypt(PASSWORD, data);
    printf("encrypted = %s\n", data);

    const char* key_guess = break_vigenere(data);
    printf("len(key_guess) = %zu\n", strlen(key_guess));
    printf("key_guess = %s\n", key_guess);

    //decrypt(PASSWORD, data);
    decrypt(key_guess, data);
    printf("decrypted = %s\n", data);

    free((void*)key_guess);
    free((void*)data);
}
