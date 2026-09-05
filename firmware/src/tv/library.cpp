#include "library.h"

#include <Arduino.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_random.h"

namespace library {

static const int kMaxEpisodes = 512;
static const int kMaxPath = 192;

static char s_dir[64] = "";
static char** s_names = nullptr;   // heap copies of the file names
static int s_count = 0;

// Shuffle bag: a permutation of [0, count) dealt from s_dealt upwards.
static uint16_t* s_bag = nullptr;
static int s_dealt = 0;
static int s_last = -1;            // index of the episode dealt last

static char s_path[kMaxPath];

static void freeList() {
    if (s_names) {
        for (int i = 0; i < s_count; i++) free(s_names[i]);
        free(s_names);
        s_names = nullptr;
    }
    free(s_bag);
    s_bag = nullptr;
    s_count = 0;
    s_dealt = 0;
    s_last = -1;
}

static bool isAvi(const char* name) {
    if (name[0] == '.') return false;   // dotfiles and macOS ._ resource forks
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".avi") == 0;
}

static void reshuffle() {
    for (int i = 0; i < s_count; i++) s_bag[i] = (uint16_t)i;
    for (int i = s_count - 1; i > 0; i--) {
        int j = (int)(esp_random() % (uint32_t)(i + 1));
        uint16_t t = s_bag[i];
        s_bag[i] = s_bag[j];
        s_bag[j] = t;
    }
    // Do not follow the last episode of the old bag with itself.
    if (s_count > 1 && s_bag[0] == s_last) {
        uint16_t t = s_bag[0];
        s_bag[0] = s_bag[s_count - 1];
        s_bag[s_count - 1] = t;
    }
    s_dealt = 0;
}

int scan(const char* dir) {
    freeList();
    strlcpy(s_dir, dir, sizeof(s_dir));

    DIR* d = opendir(dir);
    if (!d) {
        Serial.printf("[library] cannot open %s\n", dir);
        return 0;
    }
    s_names = (char**)calloc(kMaxEpisodes, sizeof(char*));
    if (!s_names) {
        closedir(d);
        return 0;
    }
    struct dirent* e;
    while ((e = readdir(d)) != nullptr && s_count < kMaxEpisodes) {
        if (e->d_type == DT_DIR) continue;
        if (!isAvi(e->d_name)) continue;
        s_names[s_count] = strdup(e->d_name);
        if (!s_names[s_count]) break;
        s_count++;
    }
    closedir(d);

    if (s_count) {
        s_bag = (uint16_t*)malloc(s_count * sizeof(uint16_t));
        if (!s_bag) {
            freeList();
            return 0;
        }
        reshuffle();
    }
    Serial.printf("[library] %d episode(s) in %s\n", s_count, dir);
    for (int i = 0; i < s_count; i++) Serial.printf("[library]   %s\n", s_names[i]);
    return s_count;
}

int count() { return s_count; }

const char* next() {
    if (!s_count) return nullptr;
    if (s_dealt >= s_count) reshuffle();
    s_last = s_bag[s_dealt++];
    snprintf(s_path, sizeof(s_path), "%s/%s", s_dir, s_names[s_last]);
    return s_path;
}

const char* current() { return s_last >= 0 ? s_path : nullptr; }

const char* currentName() { return s_last >= 0 ? s_names[s_last] : nullptr; }

}  // namespace library
