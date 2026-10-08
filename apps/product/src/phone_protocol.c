#include "phone_protocol.h"
#include "product_state.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static void event(wf_phone_t *p, wf_phone_event_t e, int32_t id)
{
    if (p->callback) p->callback(e, id, p->context);
}
static void reject(wf_phone_t *p)
{
    p->rejected++;
    event(p, WF_PHONE_REJECT, 0);
}
void wf_phone_init(wf_phone_t *p, wf_phone_callback_t cb, void *ctx)
{
    memset(p, 0, sizeof *p); p->callback = cb; p->context = ctx;
}
void wf_phone_reconnect(wf_phone_t *p) { p->used = 0; p->dropping = false; }
void wf_phone_gap(wf_phone_t *p) { p->used = 0; p->dropping = true; reject(p); }
void wf_phone_clear(wf_phone_t *p) { p->count = 0; }
static int find(const wf_phone_t *p, int32_t id)
{
    for (unsigned i = 0; i < p->count; ++i) if (p->messages[i].id == id) return (int)i;
    return -1;
}
bool wf_phone_remove(wf_phone_t *p, int32_t id)
{
    int i = find(p, id);
    if (i < 0) return false;
    memmove(&p->messages[i], &p->messages[i + 1], (p->count - (unsigned)i - 1) * sizeof p->messages[0]);
    --p->count;
    return true;
}
static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static unsigned utf8_sequence_length(unsigned c, const char *s)
{
    if ((c & 0xE0) == 0xC0 && c >= 0xC2) {
        if (((unsigned char)s[0] & 0xC0) == 0x80) return 2;
    } else if ((c & 0xF0) == 0xE0) {
        if (((unsigned char)s[0] & 0xC0) == 0x80 && ((unsigned char)s[1] & 0xC0) == 0x80) {
            if (c != 0xED || ((unsigned char)s[0] < 0xA0)) return 3;
        }
    } else if ((c & 0xF8) == 0xF0 && c <= 0xF4) {
        if (((unsigned char)s[0] & 0xC0) == 0x80 &&
            ((unsigned char)s[1] & 0xC0) == 0x80 &&
            ((unsigned char)s[2] & 0xC0) == 0x80) return 4;
    }
    return 0;
}

/* Gadgetbridge uses UTF-8 or ISO-8859-1 with limited JS string syntax.
 * Normalize escapes to JSON; never evaluate expressions, atob or scripts.
 * Text as Bitmaps must be OFF. Unsupported representations fail atomically. */
static bool normalize(wf_phone_t *p, const char *s)
{
    size_t n = 0;
    unsigned depth = 0;
    bool quoted = false;
    while (*s) {
        unsigned c = (unsigned char)*s++;
        if (n + 12 >= sizeof p->json) return false;
        if (c == '"') quoted = !quoted;
        else if (quoted && c == '\\') {
            c = (unsigned char)*s++;
            if (!c) return false;
            if (c == 'x') {
                if (!s[0] || !s[1] || hex(s[0]) < 0 || hex(s[1]) < 0) return false;
                c = (unsigned)(hex(s[0]) * 16 + hex(s[1])); s += 2;
            } else if (c >= '0' && c <= '7') {
                unsigned value = c - '0', digits = 1;
                while (digits < 3 && *s >= '0' && *s <= '7') { value = value * 8 + (unsigned)(*s++ - '0'); ++digits; }
                if (value > 255) return false;
                c = value;
            } else if (c == 'v') c = 11;
            else if (c == 'u') {
                unsigned value = 0;
                for (unsigned i = 0; i < 4; ++i) {
                    if (!*s || hex(*s) < 0) return false;
                    value = value * 16 + (unsigned)hex(*s++);
                }
                if (value == 0) return false; /* cJSON strings cannot retain embedded NUL. */
                n += (size_t)snprintf(p->json + n, sizeof p->json - n, "\\u%04x", value);
                continue;
            } else {
                if (!strchr("\"\\/bfnrt", (int)c)) return false;
                p->json[n++] = '\\'; p->json[n++] = (char)c;
                continue;
            }
            if (!c) return false;
            n += (size_t)snprintf(p->json + n, sizeof p->json - n, "\\u%04x", c);
            continue;
        } else if (!quoted) {
            if (c == '{' || c == '[') { if (++depth > 12) return false; }
            if (c == '}' || c == ']') { if (!depth) return false; --depth; }
        }
        if (c >= 128) {
            if (!quoted) return false;
            unsigned ulen = utf8_sequence_length(c, s);
            if (ulen > 0) {
                p->json[n++] = (char)c;
                for (unsigned i = 1; i < ulen; ++i) p->json[n++] = *s++;
            } else {
                n += (size_t)snprintf(p->json + n, sizeof p->json - n, "\\u%04x", c);
            }
        } else {
            if (quoted && c < 32) return false;
            p->json[n++] = (char)c;
        }
    }
    p->json[n] = 0;
    return !quoted && depth == 0;
}
static bool field(cJSON *o, const char *key, char *dest, size_t capacity, bool partial)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!v) { if (!partial) dest[0] = 0; return true; }
    if (cJSON_IsNull(v)) { dest[0] = 0; return true; }
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= capacity) return false;
    strcpy(dest, v->valuestring);
    return true;
}
static bool process_weather_json(wf_phone_t *p, cJSON *o)
{
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(o, "temp");
    if (!temp || !cJSON_IsNumber(temp) || !isfinite(temp->valuedouble)) {
        return false;
    }
    double temp_val = temp->valuedouble;
    if (temp_val > 150.0) {
        temp_val -= 273.15;
    }
    double rounded = round(temp_val);
    if (rounded < -60 || rounded > 70) return false;
    int8_t temp_c = (int8_t)rounded;

    wristflow_phone_weather_t w = {0};
    w.temp = temp_c;
    w.code = -1;

    cJSON *loc = cJSON_GetObjectItemCaseSensitive(o, "loc");
    if (loc && cJSON_IsString(loc) && loc->valuestring) {
        strncpy(w.city, loc->valuestring, sizeof(w.city) - 1);
    }

    cJSON *txt = cJSON_GetObjectItemCaseSensitive(o, "txt");
    if (txt && cJSON_IsString(txt) && txt->valuestring) {
        strncpy(w.condition, txt->valuestring, sizeof(w.condition) - 1);
    }

    cJSON *code = cJSON_GetObjectItemCaseSensitive(o, "code");
    if (code && cJSON_IsNumber(code) && isfinite(code->valuedouble) &&
        code->valuedouble >= 0 && code->valuedouble <= INT16_MAX &&
        floor(code->valuedouble) == code->valuedouble) {
        w.code = (int16_t)code->valuedouble;
    }

    cJSON *hum = cJSON_GetObjectItemCaseSensitive(o, "hum");
    if (hum && cJSON_IsNumber(hum) && isfinite(hum->valuedouble)) {
        double h = hum->valuedouble;
        if (h >= 0 && h <= 100) {
            w.humidity = (uint8_t)round(h);
            w.humidity_valid = true;
        }
    }

    cJSON *wind = cJSON_GetObjectItemCaseSensitive(o, "wind");
    if (wind) {
        if (cJSON_IsString(wind) && wind->valuestring) {
            strncpy(w.wind, wind->valuestring, sizeof(w.wind) - 1);
        } else if (cJSON_IsNumber(wind) && isfinite(wind->valuedouble)) {
            snprintf(w.wind, sizeof(w.wind), "%.0f km/h", wind->valuedouble);
        }
    }

    w.timestamp = p->utc;
    p->weather = w;
    p->has_weather = true;
    event(p, WF_PHONE_WEATHER, 0);
    return true;
}

static bool process_json(wf_phone_t *p, cJSON *o)
{
    if (!cJSON_IsObject(o)) return false;
    for (cJSON *a = o->child; a; a = a->next)
        for (cJSON *b = a->next; b; b = b->next)
            if (!strcmp(a->string, b->string)) return false;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(o, "t");
    if (!cJSON_IsString(type)) return false;
    const char *t = type->valuestring;
    if (!strcmp(t, "is_gps_active")) { event(p, WF_PHONE_GPS_QUERY, 0); return true; }
    if (!strcmp(t, "weather")) { return process_weather_json(p, o); }
    if (strcmp(t, "notify") && strcmp(t, "notify~") && strcmp(t, "notify-")) {
        p->unknown++; event(p, WF_PHONE_UNKNOWN, 0); return true;
    }
    cJSON *key = cJSON_GetObjectItemCaseSensitive(o, "id");
    if (!cJSON_IsNumber(key) || !isfinite(key->valuedouble) ||
        key->valuedouble < INT32_MIN || key->valuedouble > INT32_MAX ||
        key->valuedouble != (double)(int32_t)key->valuedouble) return false;
    int32_t id = (int32_t)key->valuedouble;
    if (!strcmp(t, "notify-")) {
        if (wf_phone_remove(p, id)) p->removed++;
        event(p, WF_PHONE_REMOVE, id); return true;
    }
    int index = find(p, id);
    bool partial = !strcmp(t, "notify~");
    if (partial && index < 0) return true; /* Do not resurrect a locally deleted message. */
    wf_phone_message_t next = {0};
    if (partial) next = p->messages[index];
    next.id = id;
    if (!field(o, "src", next.source, sizeof next.source, partial) ||
        !field(o, "title", next.title, sizeof next.title, partial) ||
        !field(o, "body", next.body, sizeof next.body, partial)) return false;
    if (index >= 0) { wf_phone_remove(p, id); p->updated++; }
    else p->received++;
    if (p->count == WF_PHONE_MESSAGES) --p->count;
    memmove(&p->messages[1], &p->messages[0], p->count * sizeof next);
    p->messages[0] = next; ++p->count;
    event(p, WF_PHONE_NOTIFY, id);
    return true;
}
static void line(wf_phone_t *p)
{
    char *s = p->line;
    while (*s == 0x10 || *s == '\r' || *s == ' ') ++s;
    if (!*s) return;
    if (!strncmp(s, "setTime(", 8)) {
        char *end;
        if (s[8] < '0' || s[8] > '9') { reject(p); return; }
        double seconds = strtod(s + 8, &end);
        if (strncmp(end, ");", 2) || !isfinite(seconds) || seconds < WRISTFLOW_TIME_MIN ||
            seconds > WRISTFLOW_TIME_MAX || seconds != (double)(uint32_t)seconds) { reject(p); return; }
        /* Only UTC is consumed; Product still displays its documented UTC+8.
         * The trailing Espruino timezone/storage command is never executed. */
        p->utc = (uint32_t)seconds; event(p, WF_PHONE_TIME, 0); return;
    }
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
    if (n && s[n - 1] == ';') s[--n] = 0;
    if (n < 5 || strncmp(s, "GB(", 3) || s[n - 1] != ')') {
        p->unknown++; event(p, WF_PHONE_UNKNOWN, 0); return;
    }
    s[n - 1] = 0;
    if (!normalize(p, s + 3)) { reject(p); return; }
    cJSON *o = cJSON_ParseWithOpts(p->json, NULL, true);
    bool valid = o && process_json(p, o);
    cJSON_Delete(o);
    if (!valid) reject(p);
}
void wf_phone_feed(wf_phone_t *p, const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        uint8_t c = data[i];
        if (c == '\n') {
            if (!p->dropping) { p->line[p->used] = 0; line(p); }
            p->used = 0; p->dropping = false;
        } else if (!p->dropping) {
            if (!c || p->used + 1 >= sizeof p->line) wf_phone_gap(p);
            else p->line[p->used++] = (char)c;
        }
    }
}
