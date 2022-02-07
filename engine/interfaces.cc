#include "interfaces.h"

#include <fstream>

#include <SDL2/SDL.h>

namespace marble {

SDL_Window* gWindow = nullptr;

void ShowWindow() {
	if (!gWindow) {
		assert(0 == SDL_Init(SDL_INIT_EVERYTHING));
		gWindow = SDL_CreateWindow(
			"marble",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1280, 800,
			SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
		);
		assert(gWindow);
	}
	SDL_ShowWindow(gWindow);
}

void CloseWindow() {
	if (gWindow) {
		SDL_DestroyWindow(gWindow);
		gWindow = nullptr;
		SDL_Quit();
	}
}

void RaiseWindow() {
	if (gWindow) {
		SDL_RaiseWindow(gWindow);
	}
}

void SetWindowPos(int x, int y) {
	if (gWindow) {
		SDL_SetWindowPosition(gWindow, x, y);
	}
}

glm::ivec2 GetWindowSize() {
	glm::ivec2 window_size{};
	if (gWindow) {
		SDL_GetWindowSize(gWindow, &window_size.x, &window_size.y);
	}
	return window_size;
}

glm::ivec2 GetMousePos() {
	glm::ivec2 mouse_pos{};
	SDL_GetGlobalMouseState(&mouse_pos.x, &mouse_pos.y);
	return mouse_pos;
}

std::vector<char> ReadFile(const char * filename) {
	std::ifstream fs{};
	fs.open(filename, std::ios::binary);
	fs.seekg(0, fs.end);
	uint32_t length = (uint32_t)fs.tellg();
	fs.seekg(0, fs.beg);
	std::vector<char> data(length);
	fs.read(data.data(), length);
	fs.close();
	return data;
}

}