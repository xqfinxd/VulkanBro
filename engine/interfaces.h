#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace marble {

void ShowWindow();
void CloseWindow();

void RaiseWindow();
void SetWindowPos(int x, int y);
glm::ivec2 GetWindowSize();
glm::ivec2 GetMousePos();

}