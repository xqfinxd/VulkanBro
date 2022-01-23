#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

#include <iostream>
#include <vector>

#include "interfaces.h"
#include "engine/engine.h"

extern SDL_Window* GetWindow();

int main()
{
    // Create an SDL window that supports Vulkan rendering.
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "Could not initialize SDL." << std::endl;
        return 1;
    }
    
	MWindow window{};
	MEngine engine{};
	engine.AttachDevice();

	SDL_Renderer* renderer = SDL_CreateRenderer(GetWindow(), -1, SDL_RENDERER_ACCELERATED);

	bool isDrag = false;
	int deltaX = 0, deltaY = 0;
	int mouseX = 0, mouseY = 0;

    // Poll for user input.
    bool stillRunning = true;
    while(stillRunning) {

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
			
            switch(event.type) {

            case SDL_QUIT:
                stillRunning = false;
				break;

			case SDL_MOUSEBUTTONDOWN:
				isDrag = true;
				deltaX = event.button.x;
				deltaY = event.button.y;
				break;
			case SDL_MOUSEBUTTONUP:
				isDrag = false;
				break;

            default:
                break;
            }
        }

		if (isDrag) {
			SDL_GetGlobalMouseState(&mouseX, &mouseY);
			SDL_SetWindowPosition(GetWindow(), mouseX - deltaX, mouseY - deltaY);
		}

		// SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		// SDL_RenderClear(renderer);

		engine.drawFrame();

		SDL_Rect rect{ 100, 100, 100, 100 };

		SDL_SetRenderDrawColor(renderer, 255, 0, 0, 254);
		SDL_RenderDrawRect(renderer, &rect);

		SDL_RenderPresent(renderer);

        SDL_Delay(10);
    }

    // Clean up.
	engine.DetachDevice();
    SDL_Quit();

    return 0;
}
