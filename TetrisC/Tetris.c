#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <Windows.h>
#include <jlib.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

typedef struct {
    int r, g, b, a;
}RGBAColor;
typedef struct {
    int h, s, v;
}HSVColor;
typedef struct {
    bool block;
    HSVColor color;
}TetrisField;
typedef struct {
    TetrisField field;
    int x, y;
}TetrisFieldActive;

TetrisField field[180];
TetrisFieldActive activeField[4];

bool running = false;
int mouseX;
int mouseY;
int targetFPS = 60;
volatile int score = 0;
volatile int playtime = 0;
SDL_mutex* fieldMutex;
SDL_mutex* timeMutex;
SDL_mutex* activefieldMutex;
volatile bool activefieldbool = false;
volatile int stepDelay = 250;
volatile bool blockout = false;
bool firstBlockout = true;


HSVColor cyan = { 115, 255, 204 };
HSVColor yellow = { 40, 255, 204 };
HSVColor blue = { 140, 255, 204 };
HSVColor orange = { 25, 255, 204 };
HSVColor red = { 0, 255, 204 };
HSVColor green = { 80, 255, 204 };
HSVColor purple = { 220, 255, 204 };

RGBAColor hsv_to_rgb(HSVColor color) {
    // Normalisierung der Eingabewerte auf den Bereich 0-1
    float h = color.h / 255.0f * 360.0f; // Umrechnung von 0–255 auf 0–360 Grad
    float s = color.s / 255.0f;          // Sättigung (0–1)
    float v = color.v / 255.0f;          // Helligkeit (0–1)

    // Chroma berechnen
    float C = v * s;
    float X = C * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = v - C;

    float r_prime = 0, g_prime = 0, b_prime = 0;

    // Bestimmen der Primärwerte basierend auf h
    if (h >= 0 && h < 60) {
        r_prime = C; g_prime = X; b_prime = 0;
    }
    else if (h >= 60 && h < 120) {
        r_prime = X; g_prime = C; b_prime = 0;
    }
    else if (h >= 120 && h < 180) {
        r_prime = 0; g_prime = C; b_prime = X;
    }
    else if (h >= 180 && h < 240) {
        r_prime = 0; g_prime = X; b_prime = C;
    }
    else if (h >= 240 && h < 300) {
        r_prime = X; g_prime = 0; b_prime = C;
    }
    else {  // h >= 300 && h < 360
        r_prime = C; g_prime = 0; b_prime = X;
    }

    // Offset m hinzufügen und in den Bereich 0–255 skalieren
    RGBAColor rgb;
    rgb.r = (int)((r_prime + m) * 255);
    rgb.g = (int)((g_prime + m) * 255);
    rgb.b = (int)((b_prime + m) * 255);
    rgb.a = 255; // Deckkraft auf 255 setzen

    return rgb;
}


void SetBlock(int x, int y, HSVColor hsv) {
    SDL_LockMutex(fieldMutex);
    field[x + y * 10].block = true;
    field[x + y * 10].color = hsv;
    SDL_UnlockMutex(fieldMutex);
}
bool IsBlock(int x, int y) {
    if (x > 9 || y > 17) {
        return true;
    }
    SDL_LockMutex(fieldMutex);
    if (field[x + y * 10].block == true) {
        SDL_UnlockMutex(fieldMutex);
        return true;
    }
    SDL_UnlockMutex(fieldMutex);
    return false;
}
void RemoveBlock(int x, int y) {
    SDL_LockMutex(fieldMutex);
    field[x + y * 10].block = false;
    field[x + y * 10].color = (HSVColor){ 0, 0, 0 };
    SDL_UnlockMutex(fieldMutex);
}
DWORD WINAPI IncrementTime(LPVOID lpParam) {
    while (true) {
        SDL_LockMutex(timeMutex);
        playtime++;
        SDL_UnlockMutex(timeMutex);
        Sleep(1000);
    }
    return 0;
}
bool IsRow(int y) {
    SDL_LockMutex(fieldMutex);
    int count = 0;
    for (int i = y * 10; i < y * 10 + 10; i++) {
        if (field[i].block) {
            count++;
        }
    }
    SDL_UnlockMutex(fieldMutex);
    if (count == 10) {
        return true;
    }
    return false;
}
void RotateActive(int direction) { //0 down 1 left 2 right
    if (activefieldbool) {
        bool possible = true;
        switch (direction) {
        case 0:
            SDL_LockMutex(fieldMutex);
            for (int i = 0; i < 4; i++) {
                if (activeField[i].y + 1 >= 18 || IsBlock(activeField[i].x, activeField[i].y + 1)) possible = false;
            }
            if (possible){
                for (int i = 0; i < 4; i++) {
                    activeField[i].y++;
                }
            }
            SDL_UnlockMutex(fieldMutex);
            break;
        case 1:
            SDL_LockMutex(fieldMutex);
            for (int i = 0; i < 4; i++) {
                if (activeField[i].x -1 < 0 || IsBlock(activeField[i].x - 1, activeField[i].y)) possible = false;
            }
            if (possible) {
                for (int i = 0; i < 4; i++) {
                    activeField[i].x--;
                }
            }
            SDL_UnlockMutex(fieldMutex);
            break;
        case 2:
            SDL_LockMutex(fieldMutex);
            for (int i = 0; i < 4; i++) {
                if (activeField[i].x + 1 > 17 || IsBlock(activeField[i].x + 1, activeField[i].y)) possible = false;
            }
            if (possible) {
                for (int i = 0; i < 4; i++) {
                    activeField[i].x++;
                }
            }
            SDL_UnlockMutex(fieldMutex);
            break;
        }
    }
}
DWORD WINAPI DropBlocks(LPVOID lpParam) {
    srand((unsigned int)clock());
    while (true) {
        SDL_LockMutex(activefieldMutex);
        if (activefieldbool) {
            bool dropIsPossible = true;
            for (int i = 0; i < sizeof(activeField) / sizeof(activeField[0]); i++) {
                if (activeField[i].field.block && IsBlock(activeField[i].x, activeField[i].y + 1)) {
                    printf("Drop not possible: %d\n", i);
                    dropIsPossible = false;
                }
            }
            if (dropIsPossible) {
                for (int i = 0; i < sizeof(activeField) / sizeof(activeField[0]); i++) {
                    if (activeField[i].field.block) {
                        activeField[i].y = activeField[i].y + 1;
                    }
                }
            }
            else {
                for (int i = 0; i < sizeof(activeField) / sizeof(activeField[0]); i++) {
                    SetBlock(activeField[i].x,activeField[i].y,activeField[i].field.color);
                    activeField[i].field.block = false;
                }
                score += 5;
                activefieldbool = false;
            }
        }
        else {
            switch (rand() % 8)
            {
            case 0:
                if (!IsBlock(5, 0) && !IsBlock(5, 1) && !IsBlock(5, 2) && !IsBlock(5, 3)) {
                    activeField[0].x = 5;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = cyan;
                    activeField[1].x = 5;
                    activeField[1].y = 1;
                    activeField[1].field.block = true;
                    activeField[1].field.color = cyan;
                    activeField[2].x = 5;
                    activeField[2].y = 2;
                    activeField[2].field.block = true;
                    activeField[2].field.color = cyan;
                    activeField[3].x = 5;
                    activeField[3].y = 3;
                    activeField[3].field.block = true;
                    activeField[3].field.color = cyan;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            case 1:
                if (!IsBlock(5, 0) && !IsBlock(5, 1) && !IsBlock(6, 0) && !IsBlock(6, 1)) {
                    activeField[0].x = 5;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = yellow;
                    activeField[1].x = 5;
                    activeField[1].y = 1;
                    activeField[1].field.block = true;
                    activeField[1].field.color = yellow;
                    activeField[2].x = 6;
                    activeField[2].y = 0;
                    activeField[2].field.block = true;
                    activeField[2].field.color = yellow;
                    activeField[3].x = 6;
                    activeField[3].y = 1;
                    activeField[3].field.block = true;
                    activeField[3].field.color = yellow;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            case 2: 
                if (!IsBlock(4, 0) && !IsBlock(5, 0) && !IsBlock(6, 0) && !IsBlock(6, 1)) {
                    activeField[0].x = 4;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = blue;
                    activeField[1].x = 5;
                    activeField[1].y = 0;
                    activeField[1].field.block = true;
                    activeField[1].field.color = blue;
                    activeField[2].x = 6;
                    activeField[2].y = 0;
                    activeField[2].field.block = true;
                    activeField[2].field.color = blue;
                    activeField[3].x = 6;
                    activeField[3].y = 1;
                    activeField[3].field.block = true;
                    activeField[3].field.color = blue;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            case 3:
                if (!IsBlock(4, 0) && !IsBlock(5, 0) && !IsBlock(6, 0) && !IsBlock(4, 1)) {
                    activeField[0].x = 4;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = orange;
                    activeField[1].x = 5;
                    activeField[1].y = 0;
                    activeField[1].field.block = true;
                    activeField[1].field.color = orange;
                    activeField[2].x = 6;
                    activeField[2].y = 0;
                    activeField[2].field.block = true;
                    activeField[2].field.color = orange;
                    activeField[3].x = 4;
                    activeField[3].y = 1;
                    activeField[3].field.block = true;
                    activeField[3].field.color = orange;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            case 4:
                if (!IsBlock(5, 0) && !IsBlock(5, 1) && !IsBlock(6, 1) && !IsBlock(6, 2)) {
                    activeField[0].x = 4;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = red;
                    activeField[1].x = 4;
                    activeField[1].y = 1;
                    activeField[1].field.block = true;
                    activeField[1].field.color = red;
                    activeField[2].x = 5;
                    activeField[2].y = 1;
                    activeField[2].field.block = true;
                    activeField[2].field.color = red;
                    activeField[3].x = 5;
                    activeField[3].y = 2;
                    activeField[3].field.block = true;
                    activeField[3].field.color = red;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            case 5:
                if (!IsBlock(5, 0) && !IsBlock(5, 1) && !IsBlock(6, 1) && !IsBlock(6, 2)) {
                    activeField[0].x = 5;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = green;
                    activeField[1].x = 5;
                    activeField[1].y = 1;
                    activeField[1].field.block = true;
                    activeField[1].field.color = green;;
                    activeField[2].x = 4;
                    activeField[2].y = 1;
                    activeField[2].field.block = true;
                    activeField[2].field.color = green;
                    activeField[3].x = 4;
                    activeField[3].y = 2;
                    activeField[3].field.block = true;
                    activeField[3].field.color = green;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            case 6:
                if (!IsBlock(5, 0) && !IsBlock(4, 1) && !IsBlock(5, 1) && !IsBlock(6, 1)) {
                    activeField[0].x = 5;
                    activeField[0].y = 0;
                    activeField[0].field.block = true;
                    activeField[0].field.color = purple;
                    activeField[1].x = 4;
                    activeField[1].y = 1;
                    activeField[1].field.block = true;
                    activeField[1].field.color = purple;;
                    activeField[2].x = 5;
                    activeField[2].y = 1;
                    activeField[2].field.block = true;
                    activeField[2].field.color = purple;
                    activeField[3].x = 6;
                    activeField[3].y = 1;
                    activeField[3].field.block = true;
                    activeField[3].field.color = purple;
                    activefieldbool = true;
                }
                else {
                    blockout = true;
                }
                break;
            default:
                break;
            }
        }
        SDL_UnlockMutex(activefieldMutex);
        Sleep(stepDelay);
    }
}
int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    SDL_Window* window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 520, 925, NULL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    char* basePath = SDL_GetBasePath();

    //Font setup
    const char* fontFileName = "slkscr.ttf";
    char fullPath[512];
    snprintf(fullPath, sizeof(fullPath), "%s%s", basePath, fontFileName);
    TTF_Font* font = TTF_OpenFont(fullPath, 26);  // Pfad zur Schriftart, Schriftgröße
    TTF_Font* bigfont = TTF_OpenFont(fullPath, 80);
    TTF_Font* verybigfont = TTF_OpenFont(fullPath, 90);
    if (font == NULL || bigfont == NULL || verybigfont == NULL) {
        printf("TTF Error: %s", TTF_GetError());
        return -1;
    }

    //Sound setup
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Fehler bei Mix_OpenAudio: %s\n", Mix_GetError());
        SDL_Quit();
        return -1;
    }
    char backgroundPath[512];
    snprintf(backgroundPath, sizeof(backgroundPath), "%s%s", basePath, "Audio\\background1.mp3");
    Mix_Music* backgroundMusic = Mix_LoadMUS(backgroundPath);
    if (!backgroundMusic) {
        printf("Fehler beim Laden der Hintergrundmusik: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return -1;
    }
    char gameoverPath[512];
    snprintf(gameoverPath, sizeof(gameoverPath), "%s%s", basePath, "Audio\\gameover.mp3");
    Mix_Music* gameoverMusic = Mix_LoadMUS(gameoverPath);
    if (!gameoverMusic) {
        printf("Fehler beim Laden der GameOvermusik: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return -1;
    }
    char sfxPath[512];
    snprintf(sfxPath, sizeof(sfxPath), "%s%s", basePath, "Audio\\sfx1.mp3");
    Mix_Chunk* soundEffect = Mix_LoadWAV(sfxPath);
    if (!soundEffect) {
        printf("Fehler beim Laden des Soundeffekts: %s\n", Mix_GetError());
        Mix_FreeMusic(backgroundMusic);
        Mix_CloseAudio();
        SDL_Quit();
        return -1;
    }


    srand(time(NULL));
    SDL_Color textColor = { 255, 255, 255 };

    SDL_ShowCursor(SDL_ENABLE);

    LARGE_INTEGER frequency;
    LARGE_INTEGER t1, t2;
    double delta = 0.0;
    QueryPerformanceFrequency(&frequency);

    const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
    //Menu
    bool menurunning = true;
    bool hover = false;
    QueryPerformanceCounter(&t1);
    while (menurunning) {
        QueryPerformanceCounter(&t2);
        delta = (double)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart;
        t1 = t2;

        //Maus-koordinaten
        Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (Mix_PlayChannel(-1, soundEffect, 0) == -1) {
                    printf("Fehler beim Abspielen des Soundeffekts: %s\n", Mix_GetError());
                }
                if (event.button.button == SDL_BUTTON_LEFT && hover) {
                    printf("Left mouse button clicked at (%d, %d)\n",
                        event.button.x, event.button.y);
                    running = true;
                    menurunning = false;
                }
            }
        }
        hover = false;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);


        //Menu
        SDL_Surface* TitleSurface = TTF_RenderText_Solid(font, "TETRIS", textColor);
        SDL_Texture* TitleTexture = SDL_CreateTextureFromSurface(renderer, TitleSurface);
        int widthScore;
        int heightScore;
        TTF_SizeText(bigfont, "TETRIS", &widthScore, &heightScore);
        SDL_Rect textRect = { 110, 300, widthScore, heightScore };  // Position und Größe des Textes
        SDL_RenderCopy(renderer, TitleTexture, NULL, &textRect);
        //229 80
        if (mouseX >= 140 && mouseX < 370 && mouseY >= 450 && mouseY <= 530) {
            //Play Button Hover
            hover = true;
            SDL_Color black = { 0, 0, 0 };
            SDL_Surface* PlaySurface = TTF_RenderText_Solid(font, "PLAY", black);
            SDL_Texture* PlayTexture = SDL_CreateTextureFromSurface(renderer, PlaySurface);
            int widhtPlay;
            int heightPlay;
            TTF_SizeText(bigfont, "PLAY", &widhtPlay, &heightPlay);
            SDL_Rect playRect = { 140, 450, widhtPlay, heightPlay };  // Position und Größe des Textes
            SDL_Rect outlineRect = { 140, 450, 230, 80 };
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            SDL_RenderFillRect(renderer, &outlineRect);
            SDL_RenderCopy(renderer, PlayTexture, NULL, &playRect);
        }
        else {
            //Play Button Normal
            SDL_Color green = { 0, 255, 0 };
            SDL_Surface* PlaySurface = TTF_RenderText_Solid(font, "PLAY", green);
            SDL_Texture* PlayTexture = SDL_CreateTextureFromSurface(renderer, PlaySurface);
            int widhtPlay;
            int heightPlay;
            TTF_SizeText(bigfont, "PLAY", &widhtPlay, &heightPlay);
            SDL_Rect playRect = { 140, 450, widhtPlay, heightPlay };  // Position und Größe des Textes
            SDL_RenderCopy(renderer, PlayTexture, NULL, &playRect);
        }

        SDL_RenderPresent(renderer);
        double targetFrameTime = 1.0 / targetFPS;
        double frameDelta = 0;
        do {
            QueryPerformanceCounter(&t2);
            frameDelta = (double)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart;
        } while (frameDelta < targetFrameTime);
    }

    //Game start
    fieldMutex = SDL_CreateMutex();
    timeMutex = SDL_CreateMutex();
    activefieldMutex = SDL_CreateMutex();
    SDL_LockMutex(fieldMutex);
    for (int i = 0; i < sizeof(field) / sizeof(field[0]); i++) {
        field[i].block = false;
        field[i].color = (HSVColor){ 0, 0, 0};
    }
    // COLOR, 255, 204
    // 0 Rot  70 Grün  45 Gelb  210 Pink  120 Cyan
    SDL_UnlockMutex(fieldMutex);
    HANDLE threadHandleTime;
    HANDLE threadDrop;

    //Timer starten
    threadHandleTime = CreateThread(
        NULL,
        0,              //Stackgröße
        IncrementTime,  //Adresse
        NULL,           //Parameter
        0,              //Flags
        NULL            //ID
    );
    threadDrop = CreateThread(
        NULL,
        0,
        DropBlocks,
        NULL,
        0,
        NULL
    );

    //Musik starten
    if (Mix_PlayMusic(backgroundMusic, -1) == -1) {
        printf("Fehler beim Abspielen der Hintergrundmusik: %s\n", Mix_GetError());
    }

    QueryPerformanceCounter(&t1);

    while (running) {
        //Delta berechnen
        QueryPerformanceCounter(&t2);
        delta = (double)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart;
        t1 = t2;

        //Maus-koordinaten
        Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

        //Events/Controll oder so idk
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    //Pause-Menü
                    break;
                }
                if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
                    printf("Down!\n");
                    RotateActive(0);
                }
                if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
                    printf("Left!\n");
                    RotateActive(1);
                }
                if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_d) {
                    printf("Right!");
                    RotateActive(2);
                }
            }
        }

        /*
        // Input handling
        if (keyboardState[SDL_SCANCODE_W]) {
            characterY -= characterSpeed * delta;
        }
        */
         
        //Bildschirm leeren
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        //Rendern
        SDL_LockMutex(fieldMutex);
        for (int i = 0; i < 18; i++) {       //Y
            for (int j = 0; j < 10; j++) {   //X
                int id = i * 10 + j;
                if (field[id].block) {
                    SDL_Rect baseRect = { j * 50 + 10, i * 50 + 25, 50, 50 };
                    HSVColor hsv = field[id].color;
                    RGBAColor color = hsv_to_rgb(hsv);
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(renderer, &baseRect);
                    SDL_Rect leftRect = {j * 50 + 10, i * 50 + 25, 5, 50};
                    hsv.v = hsv.v - 26;
                    hsv.s = hsv.s - 100;
                    color = hsv_to_rgb(hsv);
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(renderer, &leftRect);
                    SDL_Rect upRect = { j * 50 + 10, i * 50 + 25, 50, 5 };
                    hsv.v = hsv.v + 50;
                    hsv.s = hsv.s - 79;
                    color = hsv_to_rgb(hsv);
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(renderer, &upRect);
                    SDL_Rect cornerRect = { j * 50 + 10, i * 50 + 25, 5, 5 };
                    hsv = field[id].color;
                    hsv.v = hsv.v - 100;
                    color = hsv_to_rgb(hsv);
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderFillRect(renderer, &cornerRect);
                }
            }
        }
        for (int i = 0; i < sizeof(activeField) / sizeof(activeField[0]); i++) {
            if (activeField[i].field.block) {
                SDL_Rect baseRect = { activeField[i].x * 50 + 10, activeField[i].y * 50 + 25, 50, 50 };
                HSVColor hsv = activeField[i].field.color;
                RGBAColor color = hsv_to_rgb(hsv);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &baseRect);
                SDL_Rect leftRect = { activeField[i].x * 50 + 10, activeField[i].y * 50 + 25, 5, 50 };
                hsv.v = hsv.v - 26;
                hsv.s = hsv.s - 100;
                color = hsv_to_rgb(hsv);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &leftRect);
                SDL_Rect upRect = { activeField[i].x * 50 + 10, activeField[i].y * 50 + 25, 50, 5 };
                hsv.v = hsv.v + 50;
                hsv.s = hsv.s - 79;
                color = hsv_to_rgb(hsv);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &upRect);
                SDL_Rect cornerRect = { activeField[i].x * 50 + 10, activeField[i].y * 50 + 25, 5, 5 };
                hsv = activeField[i].field.color;
                hsv.v = hsv.v - 100;
                color = hsv_to_rgb(hsv);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderFillRect(renderer, &cornerRect);
            }
        }
        SDL_UnlockMutex(activefieldMutex);
        //Stats rendern
        char stringScore[64];
        char stringTime[64];
        SDL_LockMutex(timeMutex);
        sprintf_s(stringScore, sizeof(stringScore), "SCORE %d", score);
        SDL_UnlockMutex(timeMutex);
        sprintf_s(stringTime, sizeof(stringTime), "TIME %d", playtime);
        SDL_Surface* SurfaceScore = TTF_RenderText_Solid(font, stringScore, textColor);
        SDL_Texture* TextureScore = SDL_CreateTextureFromSurface(renderer, SurfaceScore);
        int widthScore;
        int heightScore;
        TTF_SizeText(font, stringScore, &widthScore, &heightScore);
        SDL_Rect textRect = { 75, 0, widthScore, heightScore };  // Position und Größe des Textes
        SDL_RenderCopy(renderer, TextureScore, NULL, &textRect);
        SDL_Surface* textSurfaceTime = TTF_RenderText_Solid(font, stringTime, textColor);
        SDL_Texture* TextureTime = SDL_CreateTextureFromSurface(renderer, textSurfaceTime);
        int widthTime;
        int heightTime;
        TTF_SizeText(font, stringTime, &widthTime, &heightTime);
        SDL_Rect timeRect = { 320, 0, widthTime, heightTime };
        SDL_RenderCopy(renderer, TextureTime, NULL, &timeRect);

        if (blockout && firstBlockout) {
            TerminateThread(threadHandleTime, 0);
            TerminateThread(threadDrop, 0);
            activefieldbool = false;
            Mix_HaltMusic();
            firstBlockout = false;
            if (Mix_PlayMusic(gameoverMusic, -1) == -1) {
                printf("Fehler beim Abspielen der Gameovermusik: %s\n", Mix_GetError());
            }
        }
        if (blockout) {
            SDL_Surface* SurfaceBlockout = TTF_RenderText_Solid(bigfont, "BLOCKOUT", (SDL_Color) {255, 0, 0, 255});
            SDL_Texture* TextureBlockout = SDL_CreateTextureFromSurface(renderer, SurfaceBlockout);
            int widthBlockout;
            int heightBlockout;
            TTF_SizeText(verybigfont, "BLOCKOUT", &widthBlockout, &heightBlockout);
            SDL_Rect backgroundRect = { 0, 350, 520, 85 };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &backgroundRect);
            SDL_Rect textRect = { 10, 350, widthBlockout, heightBlockout };  // Position und Größe des Textes
            SDL_RenderCopy(renderer, TextureBlockout, NULL, &textRect);
        }

        //Render präsentieren
        SDL_RenderPresent(renderer);

        // Frame-Limitierung
        double targetFrameTime = 1.0 / targetFPS;
        double frameDelta = 0;
        do {
            QueryPerformanceCounter(&t2);
            frameDelta = (double)(t2.QuadPart - t1.QuadPart) / frequency.QuadPart;
        } while (frameDelta < targetFrameTime);
    }
    TerminateThread(threadHandleTime, 0);
    Mix_FreeChunk(soundEffect);
    Mix_FreeMusic(backgroundMusic);
    Mix_CloseAudio();
    CloseHandle(threadHandleTime);
    TTF_CloseFont(bigfont);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}