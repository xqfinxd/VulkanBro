#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>

#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "engine/interfaces.h"
#include "engine/engine.h"
#include "engine/program.h"

#define STB_IMAGE_IMPLEMENTATION
#include "engine/stb_image.h"

constexpr int kTickPerFrame = 1000 / 60;

void Update(marble::Buffer& buffer, float x, float y, float z) {
	int width = 0, height = 0;
	auto win_size = marble::GetWindowSize();
	width = win_size.x;
	height = win_size.y;

	float fov = glm::radians(45.0f);
	if (width > height && width != 0 && height != 0) {
		fov *= static_cast<float>(height) / static_cast<float>(width);
	}
	auto projection = glm::perspective(fov, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
	auto view = glm::lookAt(glm::vec3(x, y, z),  // Camera is at (-5,3,-10), in World Space
		glm::vec3(0, 0, 0),     // and looks at the origin
		glm::vec3(0, -1, 0)     // Head is up (set to 0,-1,0 to look upside-down)
	);
	auto model = glm::mat4(1.0f);
	// Vulkan clip space has inverted Y and half Z.
	auto clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

	auto MVP = /*clip * projection * view **/ model;

	buffer.SetData(glm::value_ptr(MVP), sizeof(MVP));
}

int main()
{
	marble::ShowWindow();
	marble::GetEngine().Init();

	auto program = marble::CreateProgram();
	program->AddShaderUniform(0, 0, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	program->AddShaderUniform(0, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	program->GetLayout();
	marble::Buffer uniform_buffer{};
	uniform_buffer.BindBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	uniform_buffer.BufferData(sizeof(glm::mat4), nullptr);
	program->BindUniformBuffer(0, 0, uniform_buffer, sizeof(glm::mat4));
	marble::Texture sampled_image{};
	int width = 0, height = 0, channel = 0;
	unsigned char* image_data = stbi_load("resources/wall.jpg", &width, &height, &channel, 4);
	sampled_image.TexImage2D(width, height, 4, image_data);
	stbi_image_free(image_data);
	sampled_image.EnableSampler();
	program->BindTexture(0, 1, sampled_image);
	program->Build();

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

		Update(uniform_buffer, x, y, z);
		program->Draw();

		uint32_t frame_end_tick = SDL_GetTicks();

		int delta_tick = frame_end_tick - frame_begin_tick;
		if (delta_tick < kTickPerFrame) {
			SDL_Delay(kTickPerFrame - delta_tick);
		}
    }
	uniform_buffer.Clear();
	sampled_image.Clear();
	marble::DestoryProgram(program);

	marble::GetEngine().Quit();
	marble::CloseWindow();

    return 0;
}
