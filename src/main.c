#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_STYLES 100
#define MAX_ELEMENTS 100

typedef enum {TEXT, BUTTON} ElementType;

typedef struct {
    char id[64];
    SDL_Color color;
} Style;

typedef struct {
    ElementType type;
    char content[256];
    SDL_Color color;
    char id[64];
} Element;

Style styles[MAX_STYLES];
int style_count = 0;

Element elements[MAX_ELEMENTS];
int element_count = 0;

SDL_Color hexToColor(const char *hex) {
    SDL_Color color = {255, 255, 255, 255};
    if (hex[0] == '#') {
        unsigned int r, g, b;
        if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            color.r = r; color.g = g; color.b = b;
        }
    }
    return color;
}

char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    fread(buffer, 1, size, f);
    buffer[size] = 0;
    fclose(f);
    return buffer;
}

void parseStyles(const char* xml) {
    const char* pos = xml;
    while ((pos = strstr(pos, "<sty>"))) {
        pos += 5;
        const char* end = strstr(pos, "</sty>");
        if (!end) break;
        char block[512] = {0};
        int len = (int)(end - pos);
        if (len > 511) len = 511;
        strncpy(block, pos, len);
        pos = end + 6;

        char* line = strtok(block, "\n");
        while (line) {
            while (*line == ' ' || *line == '\t') line++;
            if (*line == '*') line++;
            char* eq = strchr(line, '=');
            if (eq) {
                *eq = 0;
                char* key = line;
                char* value = eq + 1;
                while (*key == ' ' || *key == '\t') key++;
                int key_len = (int)strlen(key);
                while (key_len > 0 && (key[key_len - 1] == ' ' || key[key_len - 1] == '\t')) {
                    key[key_len - 1] = 0;
                    key_len--;
                }
                while (*value == ' ' || *value == '\t') value++;
                char hexcode[16] = {0};
                const char* start = strstr(value, "color(");
                if (start) {
                    start += 6;
                    const char* end_paren = strchr(start, ')');
                    if (end_paren) {
                        int len = (int)(end_paren - start);
                        if (len > 15) len = 15;
                        strncpy(hexcode, start, len);
                        hexcode[len] = 0;
                    }
                } else {
                    strncpy(hexcode, value, 15);
                    hexcode[15] = 0;
                }
                if (style_count < MAX_STYLES) {
                    strncpy(styles[style_count].id, key, 63);
                    styles[style_count].id[63] = 0;
                    styles[style_count].color = hexToColor(hexcode);
                    style_count++;
                }
            }
            line = strtok(NULL, "\n");
        }
    }
}

SDL_Color getColorById(const char* id) {
    for (int i = 0; i < style_count; i++) {
        if (strcmp(styles[i].id, id) == 0) {
            return styles[i].color;
        }
    }
    return (SDL_Color){255, 255, 255, 255};
}

void parseElements(const char* xml) {
    const char* pos = xml;
    while ((pos = strstr(pos, "<text"))) {
        const char* startTagEnd = strchr(pos, '>');
        if (!startTagEnd) break;
        const char* endTag = strstr(startTagEnd, "</text>");
        if (!endTag) break;
        int contentLen = (int)(endTag - startTagEnd - 1);
        if (element_count >= MAX_ELEMENTS) break;

        char content[256] = {0};
        strncpy(content, startTagEnd + 1, contentLen);
        content[contentLen] = 0;

        const char* colorAttr = strstr(pos, "color=\"");
        SDL_Color color = {255, 255, 255, 255};
        if (colorAttr && colorAttr < startTagEnd) {
            colorAttr += 7;
            const char* colorEnd = strchr(colorAttr, '"');
            if (colorEnd) {
                char colorStr[16] = {0};
                int colorLen = (int)(colorEnd - colorAttr);
                if (colorLen > 15) colorLen = 15;
                strncpy(colorStr, colorAttr, colorLen);
                colorStr[colorLen] = 0;
                color = hexToColor(colorStr);
            }
        }
        elements[element_count].type = TEXT;
        strcpy(elements[element_count].content, content);
        elements[element_count].color = color;
        element_count++;
        pos = endTag + 7;
    }
    pos = xml;
    while ((pos = strstr(pos, "<btn"))) {
        const char* startTagEnd = strchr(pos, '>');
        if (!startTagEnd) break;
        const char* endTag = strstr(startTagEnd, "</btn>");
        if (!endTag) break;
        int contentLen = (int)(endTag - startTagEnd - 1);
        if (element_count >= MAX_ELEMENTS) break;

        char content[256] = {0};
        strncpy(content, startTagEnd + 1, contentLen);
        content[contentLen] = 0;

        const char* idAttr = strstr(pos, "id=\"");
        char id[64] = {0};
        if (idAttr && idAttr < startTagEnd) {
            idAttr += 4;
            const char* idEnd = strchr(idAttr, '"');
            if (idEnd) {
                int idLen = (int)(idEnd - idAttr);
                if (idLen > 63) idLen = 63;
                strncpy(id, idAttr, idLen);
                id[idLen] = 0;
            }
        }
        elements[element_count].type = BUTTON;
        strcpy(elements[element_count].content, content);
        elements[element_count].color = getColorById(id);
        strcpy(elements[element_count].id, id);
        element_count++;
        pos = endTag + 6;
    }
}

void renderElements(SDL_Renderer* renderer, TTF_Font* font, int win_w, int win_h) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    int y = 20;
    for (int i = 0; i < element_count; i++) {
        SDL_Surface* surface = TTF_RenderText_Solid(font, elements[i].content, elements[i].color);
        if (!surface) continue;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) {
            SDL_FreeSurface(surface);
            continue;
        }
        int w, h;
        SDL_QueryTexture(texture, NULL, NULL, &w, &h);
        SDL_Rect dstrect = {(win_w - w) / 2, y, w, h};

        if (elements[i].type == BUTTON) {
            SDL_Rect rectBg = {dstrect.x - 5, dstrect.y - 3, w + 10, h + 6};
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderFillRect(renderer, &rectBg);
        }

        SDL_RenderCopy(renderer, texture, NULL, &dstrect);
        y += h + 10;
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (TTF_Init() == -1) return 1;

    SDL_Window* window = SDL_CreateWindow("TXML Interpreter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 400, 300, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font* font = TTF_OpenFont("Arial.ttf", 24);
    if (!font) return 1;

    char* xml = loadFile("/storage/emulated/0/ZZ Txtml/index.txt");
    if (!xml) return 1;

    parseStyles(xml);
    parseElements(xml);

    renderElements(renderer, font, 400, 300);

    int quit = 0;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
        }
        SDL_Delay(16);
    }

    free(xml);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
