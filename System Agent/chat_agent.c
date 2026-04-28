#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
#else
    #include <unistd.h>
#endif

#define URL "http://localhost:5000/count"
#define INTERVAL_MS 2000

// ===== HTTP buffer =====
struct Memory {
    char *data;
    size_t size;
};

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    char *ptr = realloc(mem->data, mem->size + total + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(mem->data + mem->size, contents, total);
    mem->size += total;
    mem->data[mem->size] = 0;

    return total;
}

// ===== fetch count =====
int fetch_count() {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct Memory chunk = {0};

    curl_easy_setopt(curl, CURLOPT_URL, URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !chunk.data) {
        free(chunk.data);
        return -1;
    }

    cJSON *json = cJSON_Parse(chunk.data);
    free(chunk.data);

    if (!json) return -1;

    cJSON *c = cJSON_GetObjectItem(json, "count");
    int count = (cJSON_IsNumber(c)) ? c->valueint : -1;

    cJSON_Delete(json);
    return count;
}

#ifdef _WIN32

// ===== WINDOWS TRAY =====
NOTIFYICONDATA nid;

void tray_init() {
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uID = 1;
    nid.hWnd = GetConsoleWindow();
    nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    strcpy(nid.szTip, "Chat agent");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void tray_update(int new_msgs) {
    if (new_msgs > 0) {
        nid.hIcon = LoadIcon(NULL, IDI_WARNING);
        strcpy(nid.szTip, "New messages!");
    } else {
        nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
        strcpy(nid.szTip, "Chat agent");
    }

    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void sleep_ms(int ms) {
    Sleep(ms);
}

#else

// ===== LINUX TRAY (fallback: brak GUI, tylko logika) =====
void tray_init() {
    printf("Tray init (Linux fallback)\n");
}

void tray_update(int new_msgs) {
    if (new_msgs > 0) {
        //printf("[TRAY] NEW MESSAGES!\n");
	char cmd[256];
	snprintf(cmd,sizeof(cmd),"notify-send 'ChatNorp' 'You have %d new messages.'",new_msgs);
	system(cmd);
    }
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}

#endif

// ===== MAIN =====
int main() {
    tray_init();

    int last = fetch_count();
    if (last < 0) last = 0;

    while (1) {
        sleep_ms(INTERVAL_MS);

        int current = fetch_count();
        if (current < 0) continue;

        int diff = current - last;

        if (diff > 0) {
            tray_update(diff);
        }

        last = current;
    }

    return 0;
}