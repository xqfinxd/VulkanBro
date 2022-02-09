#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

#include <iostream>
#include <vector>

#include "engine/interfaces.h"
#include "engine/engine.h"
#include "engine/program.h"

#define STB_IMAGE_IMPLEMENTATION
#include "engine/stb_image.h"

constexpr int kTickPerFrame = 1000 / 10;

int main()
{
	marble::ShowWindow();
	marble::GetEngine().Init();

	auto program = marble::CreateProgram();
	
	marble::Build(program);

	bool is_dragged = false;
	int delta_x = 0, delta_y = 0;
	glm::ivec2 mouse_pos;

	float x = 0, y = 0, z = 10;

	auto onkey = [](uint16_t key, float& x, float& y, float& z) {
		switch (key) {
		case SDLK_1:
			x += 0.5;
			break;
		case SDLK_2:
			x -= 0.5;
			break;
		case SDLK_3:
			y += 0.5;
			break;
		case SDLK_4:
			y -= 0.5;
			break;
		case SDLK_5:
			z += 0.5;
			break;
		case SDLK_6:
			z -= 0.5;
			break;
		default:
			break;
		}
	};

    // Poll for user input.
    bool is_running = true;
    while(is_running) {
		uint32_t frame_begin_tick = SDL_GetTicks();

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
			
            switch(event.type) {

            case SDL_QUIT:
                is_running = false;
				break;

			case SDL_MOUSEBUTTONDOWN:
				is_dragged = true;
				delta_x = event.button.x;
				delta_y = event.button.y;
				break;
			case SDL_MOUSEBUTTONUP:
				is_dragged = false;
				break;
			case SDL_KEYDOWN:
				onkey(event.key.keysym.sym, x, y, z);
				break;
            default:
                break;
            }
        }

		if (!is_running) {
			break;
		}

		if (is_dragged) {
			mouse_pos = marble::GetMousePos();
			marble::SetWindowPos(mouse_pos.x - delta_x, mouse_pos.y - delta_y);
		}

		marble::Update(program, x, y, z);
		marble::Draw(program);

		uint32_t frame_end_tick = SDL_GetTicks();

		int delta_tick = frame_end_tick - frame_begin_tick;
		if (delta_tick < kTickPerFrame) {
			SDL_Delay(kTickPerFrame - delta_tick);
		}
    }

	marble::DestoryProgram(program);

	marble::GetEngine().Quit();
	marble::CloseWindow();

    return 0;
}
