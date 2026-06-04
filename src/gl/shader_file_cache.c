#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "init.h"
#include "GL/gl.h"
#include "logs.h"

static pthread_mutex_t g_shadercache_mutex = PTHREAD_MUTEX_INITIALIZER;
static char* g_shadercache_root = NULL;

static uint64_t shadercache_fnv1a64(const void* data, size_t size, uint64_t seed)
{
    const unsigned char* p = (const unsigned char*)data;
    uint64_t h = seed;
    size_t i;

    for (i = 0; i < size; ++i) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void shadercache_hex64(uint64_t v, char out[17])
{
    static const char* hex = "0123456789abcdef";
    int i;

    for (i = 15; i >= 0; --i) {
        out[i] = hex[v & 0xF];
        v >>= 4;
    }
    out[16] = '\0';
}

static int shadercache_mkdir_one(const char* path)
{
    if (!path || !path[0]) return 0;
    int result = mkdir(path, 0777);
    return 1;
}

static int shadercache_ensure_tree(const char* dir)
{
    char* tmp;
    char* p;

    if (!dir || !dir[0]) return 0;

    tmp = strdup(dir);
    if (!tmp) return 0;

    p = tmp;
    while (*p) {
        if (*p == '/' && p != tmp) {
            *p = '\0';
            if (!shadercache_mkdir_one(tmp)) {
                free(tmp);
                return 0;
            }
            *p = '/';
        }
        ++p;
    }

    if (!shadercache_mkdir_one(tmp)) {
        free(tmp);
        return 0;
    }
    free(tmp);
    return 1;
}

static char* shadercache_build_path(const char* key)
{
    uint64_t h1, h2;
    char a[17], b[17];
    size_t root_len;
    size_t need;
    char* path;

    if (!g_shadercache_root || !g_shadercache_root[0] || !key) {
        return NULL;
    }

    h1 = shadercache_fnv1a64(key, strlen(key), 1469598103934665603ULL);
    h2 = shadercache_fnv1a64(key, strlen(key), 1099511628211ULL ^ 0x9e3779b97f4a7c15ULL);

    shadercache_hex64(h1, a);
    shadercache_hex64(h2, b);

    root_len = strlen(g_shadercache_root);
    need = root_len + 1 + 3 + 1 + 2 + 1 + 2 + 1 + 16 + 1 + 16 + 4 + 1;
    path = (char*)malloc(need);
    if (!path) return NULL;

    snprintf(path, need, "%s/v1/%c%c/%c%c/%s_%s.bin",
             g_shadercache_root,
             a[0], a[1],
             a[2], a[3],
             a, b);
    return path;
}

void gl4es_shadercache_set_root(const char* root_dir)
{
    pthread_mutex_lock(&g_shadercache_mutex);

    if (g_shadercache_root) {
        free(g_shadercache_root);
        g_shadercache_root = NULL;
    }

    if (root_dir && root_dir[0]) {
        g_shadercache_root = strdup(root_dir);
        if (g_shadercache_root) {
            shadercache_ensure_tree(root_dir);
        }
    }

    pthread_mutex_unlock(&g_shadercache_mutex);
}

void shadercache_build_key(GLenum shader_type, const char* source, char* out, size_t outsz)
{
    char version_buf[32];
    char flags_buf[64];
    uint64_t h1, h2;
    char ha[17], hb[17];
    char* material;
    size_t src_len;
    size_t mat_len;

    if (!out || outsz == 0) return;

    snprintf(version_buf, sizeof(version_buf),
             "type=%u|es=%d|esversion=%d|simple=%d|",
             (unsigned)shader_type,
             globals4es.es,
             200,1);

    src_len = source ? strlen(source) : 0;
    mat_len = strlen(version_buf) + src_len + 1;

    material = (char*)malloc(mat_len);
    if (!material) {
        out[0] = '\0';
        return;
    }

    strcpy(material, version_buf);
    if (source) strcat(material, source);

    h1 = shadercache_fnv1a64(material, strlen(material), 1469598103934665603ULL);
    h2 = shadercache_fnv1a64(material, strlen(material), 1099511628211ULL ^ 0x517cc1b727220a95ULL);

    shadercache_hex64(h1, ha);
    shadercache_hex64(h2, hb);

    snprintf(flags_buf, sizeof(flags_buf),
             "v1|%s|h1=%s|h2=%s",
             version_buf, ha, hb);

    strncpy(out, flags_buf, outsz - 1);
    out[outsz - 1] = '\0';

    free(material);
}

int shadercache_load(const char* key, char** out_source)
{
    char* path;
    FILE* fp;
    char magic[4];
    uint32_t version;
    uint32_t key_len;
    uint32_t src_len;
    char* stored_key;
    char* src;

    if (!out_source) return 0;
    *out_source = NULL;

    pthread_mutex_lock(&g_shadercache_mutex);
    path = shadercache_build_path(key);
    pthread_mutex_unlock(&g_shadercache_mutex);

    if (!path) return 0;

    fp = fopen(path, "rb");
    if (!fp) {
        free(path);
        return 0;
    }

    if (fread(magic, 1, 4, fp) != 4) goto fail;
    if (memcmp(magic, "G4SC", 4) != 0) goto fail;
    if (fread(&version, sizeof(version), 1, fp) != 1) goto fail;
    if (version != 1) goto fail;
    if (fread(&key_len, sizeof(key_len), 1, fp) != 1) goto fail;
    if (fread(&src_len, sizeof(src_len), 1, fp) != 1) goto fail;

    if (key_len == 0 || src_len == 0 || key_len > (4u * 1024u * 1024u) || src_len > (64u * 1024u * 1024u)) {
        goto fail;
    }

    stored_key = (char*)malloc((size_t)key_len + 1);
    src = (char*)malloc((size_t)src_len + 1);
    if (!stored_key || !src) {
        free(stored_key);
        free(src);
        goto fail;
    }

    if (fread(stored_key, 1, key_len, fp) != key_len) {
        free(stored_key);
        free(src);
        goto fail;
    }
    stored_key[key_len] = '\0';

    if (fread(src, 1, src_len, fp) != src_len) {
        free(stored_key);
        free(src);
        goto fail;
    }
    src[src_len] = '\0';

    fclose(fp);
    free(path);

    if (strcmp(stored_key, key) != 0) {
        free(stored_key);
        free(src);
        return 0;
    }

    free(stored_key);
    *out_source = src;
    return 1;

    fail:
    fclose(fp);
    free(path);
    return 0;
}

void shadercache_store(const char* key, const char* source)
{
    char* path;
    char* tmp;
    char* dir;
    FILE* fp;
    uint32_t version;
    uint32_t key_len;
    uint32_t src_len;
    int ok = 0;

    if (!key || !source) return;

    pthread_mutex_lock(&g_shadercache_mutex);
    path = shadercache_build_path(key);
    pthread_mutex_unlock(&g_shadercache_mutex);

    if (!path) return;

    dir = strdup(path);
    if (!dir) {
        free(path);
        return;
    }

    {
        char* slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            shadercache_ensure_tree(dir);
        }
    }

    tmp = (char*)malloc(strlen(path) + 8);
    if (!tmp) {
        free(dir);
        free(path);
        return;
    }

    snprintf(tmp, strlen(path) + 8, "%s.tmp", path);

    fp = fopen(tmp, "wb");
    if (!fp) {
        free(tmp);
        free(dir);
        free(path);
        return;
    }

    version = 1;
    key_len = (uint32_t)strlen(key);
    src_len = (uint32_t)strlen(source);

    ok = 1;
    ok = ok && (fwrite("G4SC", 1, 4, fp) == 4);
    ok = ok && (fwrite(&version, sizeof(version), 1, fp) == 1);
    ok = ok && (fwrite(&key_len, sizeof(key_len), 1, fp) == 1);
    ok = ok && (fwrite(&src_len, sizeof(src_len), 1, fp) == 1);
    ok = ok && (fwrite(key, 1, key_len, fp) == key_len);
    ok = ok && (fwrite(source, 1, src_len, fp) == src_len);

    fflush(fp);
    fclose(fp);

    if (!ok) {
        remove(tmp);
        free(tmp);
        free(dir);
        free(path);
        return;
    }

    remove(path);
    if (rename(tmp, path) != 0) {
        remove(tmp);
    }

    free(tmp);
    free(dir);
    free(path);
}