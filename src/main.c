#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include<unistd.h>
#include<termios.h>

//#define __JGUI
#ifdef __JGUI
#include<SDL2/SDL.h>
#endif

#include"cpu.h"

#define SCR_WIDTH  160
#define SCR_HEIGHT 100
#define SCR_TEXT   "85Emu"
#define SCR_SCALE  4

#ifdef __JGUI
void display_pixel(SDL_Renderer* render, int x, int y, uint8_t colour)
{
	uint8_t R = (colour & 0b11) * 85;		// Scale the value to 0-255 range
	uint8_t G = ((colour >> 2) & 0b11) * 85; // Scale the value to 0-255 range
	uint8_t B = ((colour >> 4) & 0b11) * 85; // Scale the value to 0-255 range

	SDL_Rect box = {.x = x, .y = y, .w = SCR_SCALE, .h = SCR_SCALE};
	SDL_SetRenderDrawColor(render, R, G, B, 255);  // Use the RGB values
	SDL_RenderFillRect(render, &box);
}
#endif

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("invalid usage, ./85Emu <ROM PATH>\n");
		exit(1);
	}
#ifdef __JGUI
	SDL_Event event;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	SDL_Window* window = SDL_CreateWindow(SCR_TEXT,
										  SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
										  SCR_WIDTH*SCR_SCALE, SCR_HEIGHT*SCR_SCALE, SDL_WINDOW_SHOWN);
	if (!window) {
		fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		return EXIT_FAILURE;
	}
#endif
	int err = load_file(argv[1], 0);
	if (err != 0) {
		printf("%s", msgs[err]);
		exit(1);
	}

	LOG_FILE = fopen("log.txt", "w");
	fprintf(LOG_FILE, "%.4x bytes read\n", bytes_read);

	reset();

	bool quit = false;
	while (!quit) {
#ifdef __JGUI
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_QUIT:
					quit = true;
					break;
			}
		}
#endif		
		if (running) execute(fetch(ram, PC++));
#ifndef __JGUI
		else { quit = true; break; }
#endif
#ifdef __JGUI
		if (redraw) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);

			for (int y = 0; y < SCR_HEIGHT; ++y) {
				for (int x = 0; x < SCR_WIDTH; ++x) {
					if (ram[((y*SCR_WIDTH) + x) + (RAM_SIZE - (SCR_WIDTH * SCR_HEIGHT))]) {
						display_pixel(renderer, x, y, ram[((y*SCR_WIDTH) + x) + (RAM_SIZE - (SCR_WIDTH * SCR_HEIGHT))]);
						printf("%x\n", ram[((y*SCR_WIDTH) + x) + (RAM_SIZE - (SCR_WIDTH * SCR_HEIGHT))]);
					}
				}
			}

			SDL_RenderPresent(renderer);
			redraw = false;
		}
		SDL_Delay(10); // Reduce CPU usage by adding a small delay
#endif
	}

	fprintf(LOG_FILE, "A   - %.2x\n", psw.A);
	fprintf(LOG_FILE, "BC  - %.4x\n", X(BC));
	fprintf(LOG_FILE, "DE  - %.4x\n", X(DE));
	fprintf(LOG_FILE, "HL  - %.4x\n", X(HL));
	fprintf(LOG_FILE, "SP  - %.4x\n", X(SP));
	fprintf(LOG_FILE, "PC  - %.4x\n", PC);
	fprintf(LOG_FILE, "PSW -\n\tS  - %x\n\tZ  - %x\n\tK  - %x\n\tAC - %x\n\tP  - %x\n\tCY - %x\n", psw.F.S, psw.F.Z, psw.F.K, psw.F.AC, psw.F.P, psw.F.CY);

	fclose(LOG_FILE);

#ifdef __JGUI
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
#endif
	return 0;
}
