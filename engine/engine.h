#pragma once

#include <vector>

#include "glm/glm.hpp"

namespace marble {

struct Program;

std::vector<char> ReadFile(const char* filename);

void InitEngine();
void QuitEngine();

Program* CreateProgram();
void DestoryProgram(Program*);
void Build(Program*);
void Draw(Program*);
void Update(Program*, float x, float y, float z);

}