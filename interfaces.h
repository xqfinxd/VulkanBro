#pragma once

#include <cstdint>
#include <glm/glm.hpp>

class MWindow {
public:
	MWindow();
	~MWindow();

	void Show();
	void Hide();
	void Raise();

	glm::ivec2 GetPos() const;
	glm::ivec2 GetSize() const;

private:

};
