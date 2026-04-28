#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
#endif

#define URL "http://192.168.0.183:5000/count"
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
HICON icon_normal;
HICON icon_unread;

int has_unread = 0;

// callback window (musi istnieć)
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

        case WM_CREATE:
            icon_normal = LoadIcon(NULL, IDI_INFORMATION);
            icon_unread = LoadIcon(NULL, IDI_WARNING);

            ZeroMemory(&nid, sizeof(nid));
            nid.cbSize = sizeof(nid);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_APP + 1;
            nid.hIcon = icon_normal;
            strcpy(nid.szTip, "Chat agent");

            Shell_NotifyIcon(NIM_ADD, &nid);
            break;

        case WM_APP + 1:
            if (LOWORD(lParam) == WM_LBUTTONUP) {
                // klik = oznacz jako przeczytane
                has_unread = 0;

                nid.hIcon = icon_normal;
                strcpy(nid.szTip, "All messages read");

                Shell_NotifyIcon(NIM_MODIFY, &nid);
            }
            break;

        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void tray_init() {
    // tworzymy ukryte okno
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "ChatAgentClass";

    RegisterClass(&wc);

    CreateWindowEx(
        0,
        "ChatAgentClass",
        "ChatAgent",
        0,
        0, 0, 0, 0,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );
}

void tray_update(int new_msgs) {
    if (new_msgs > 0) {
        has_unread = 1;
        nid.hIcon = icon_unread;
        strcpy(nid.szTip, "New messages!");
        Shell_NotifyIcon(NIM_MODIFY, &nid);
    }
}

void sleep_ms(int ms) {
    Sleep(ms);
}

#else

// ===== LINUX =====
void tray_init() {
    printf("Tray init (Linux fallback)\n");
}

void tray_update(int new_msgs) {
    if (new_msgs > 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
            "notify-send 'ChatNorp' 'You have %d new messages.'",
            new_msgs);
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

#ifdef _WIN32
    // message loop Windows (TRAY CLICK)
    MSG msg;
#endif

    while (1) {
        sleep_ms(INTERVAL_MS);

        int current = fetch_count();
        if (current < 0) continue;

        int diff = current - last;

        if (diff > 0) {
            tray_update(diff);
        }

        last = current;

#ifdef _WIN32
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
#endif
    }

    return 0;
}