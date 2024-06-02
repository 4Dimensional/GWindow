#include "graphics.hpp"

using namespace gph;

void GWindow::Update()
{
	Color red(255, 0, 0, 255);
	Color blue(0, 0, 255, 50);
	Color green(0, 255, 0, 255);

	fill_rectangle({ 10, 10 }, 300, 50, SolidFill(green));
	fill_circle({ 100, 100 }, 100, RadialGradientFill({ 100, 100 }, 100, red, blue));

	fill_triangle({ 10, 10 }, { 10, 100 }, { 100, 10 }, SolidFill(red));
}
void GWindow::Start() { return; }
void GWindow::Close() { return; }
void GWindow::Tick() { return; }

int main(int argc, char* argv[])
{
	GWindow m_Window(640, 480, "Window");
	return 0;
}
