#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <Windows.h>
#include <jlib.h>


typedef struct {

}TetrisColor;
typedef struct {
    
}TetrisField;

int mouseX;
int mouseY;
int targetFPS = 60;

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 1500, NULL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    srand(time(NULL));
    SDL_ShowCursor(SDL_ENABLE);

    LARGE_INTEGER frequency;
    LARGE_INTEGER t1, t2;
    double delta = 0.0;
    QueryPerformanceFrequency(&frequency);

    const Uint8* keyboardState = SDL_GetKeyboardState(NULL);
    bool leftMouseHold = false;

    /*projectileMutex = SDL_CreateMutex();
    for (int i = 0; i < 128; i++) {
        cProjectiles[0][i] = 0.0;
    }*/

    QueryPerformanceCounter(&t1);


    bool running = true;
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

        // Window clearen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); //0 0 0 für schwarz
        SDL_RenderClear(renderer);
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}