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
volatile int stepDelay = 1000;

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
DWORD WINAPI DropBlocks(LPVOID lpParam) {
    while (true) {
        SDL_LockMutex(activefieldMutex);
        if (activefieldbool) {
            bool dropIsPossible = true;
            for (int i = 0; i < sizeof(activeField) / sizeof(activeField[0]); i++) {
                if (activeField[i].field.block && IsBlock(activeField->x, activeField->y + 1)) {
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
    if (font == NULL) {
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


        //Title
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
    SetBlock(0, 0, (HSVColor) { 45, 255, 204 });
    SetBlock(1, 0, (HSVColor) { 210, 255, 204 });
    SetBlock(5, 2, (HSVColor) { 0, 255, 204 });
    SetBlock(5, 3, (HSVColor) { 0, 255, 204 }); //70 für Grün
    SetBlock(7, 8, (HSVColor) { 120, 255, 204 });
    SetBlock(6, 1, (HSVColor) { 0, 255, 204 });
    SetBlock(6, 2, (HSVColor) { 0, 255, 204 });
    SDL_UnlockMutex(fieldMutex);
    HANDLE threadHandleTime;

    //Timer starten
    threadHandleTime = CreateThread(
        NULL,               // Standard Sicherheitsattribute
        0,                  // Standard Stackgröße
        IncrementTime,   // Startadresse (Funktion des Threads)
        NULL,               // Parameter für die Funktion
        0,                  // Standard Flags
        NULL                // Thread-ID (optional)
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
            }
        }

        /*
        // Input handling
        if (keyboardState[SDL_SCANCODE_W]) characterY -= characterSpeed * delta;
        if (keyboardState[SDL_SCANCODE_S]) characterY += characterSpeed * delta;
        if (keyboardState[SDL_SCANCODE_A]) characterX -= characterSpeed * delta;
        if (keyboardState[SDL_SCANCODE_D]) characterX += characterSpeed * delta;
        if (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT) & !leftMouseHold) {
        }
        else if (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT) & leftMouseHold) {

        }
        else {
            leftMouseHold = false;
        }
        */
         
        //Bildschirm leeren
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        //Rendern
        for (int i = 0; i < 18; i++) {       //Y
            for (int j = 0; j < 10; j++) {   //X
                int id = i * 10 + j;
                SDL_LockMutex(fieldMutex);
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
                SDL_UnlockMutex(fieldMutex);
            }
        }
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