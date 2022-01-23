#include "interfaces.h"
#include <SDL2/SDL.h>

static SDL_Window* s_window;
SDL_Window* GetWindow() {
	return s_window;
}

MWindow::MWindow() {
	s_window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN);
	assert(s_window && SDL_GetError());
}

MWindow::~MWindow() {
	SDL_DestroyWindow(s_window);
	s_window = nullptr;
}

void MWindow::Show() {
	SDL_ShowWindow(GetWindow());
}

void MWindow::Hide() {
	SDL_HideWindow(GetWindow());
}

void MWindow::Raise() {
	SDL_RaiseWindow(GetWindow());
}

glm::ivec2 MWindow::GetPos() const {
	glm::ivec2 pos{};
	SDL_GetWindowPosition(GetWindow(), &pos.x, &pos.y);
	return pos;
}

glm::ivec2 MWindow::GetSize() const {
	glm::ivec2 size{};
	SDL_GetWindowSize(GetWindow(), &size.x, &size.y);
	return size;
}
