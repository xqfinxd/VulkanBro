#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace marble {

std::vector<char> ReadFile(const char* filename);

void ShowWindow();
void CloseWindow();

void RaiseWindow();
void SetWindowPos(int x, int y);
glm::ivec2 GetWindowSize();
glm::ivec2 GetMousePos();

}