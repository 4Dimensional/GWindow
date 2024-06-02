#include <X11/X.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <unistd.h>
#include <chrono>

#define NIL (0)

namespace gph
{

struct Color
{
	int r, g, b, a;
	Color(int _r = 255, int _g = 255, int _b = 255, int _a = 255)
	: r(_r)
	, g(_g)
	, b(_b)
	, a(_a){};
};

struct Vector2
{
	int x, y;
	Vector2(int _x = 0, int _y = 0)
	: x(_x)
	, y(_y){};
};

int		get_buffer_index(Vector2 pos, int WINDOW_WIDTH);
Vector2 get_buffer_pixel(int index);
Color	get_buffer_pixel_color(Vector2 pos, int WINDOW_WIDTH, unsigned char* screenbuffer);

float lerp(float a, float b, float time, bool looping);
Color lerpRGB(Color c1, Color c2, float time);
float smoothstep(float t);
bool  point_in_triangle(Vector2 aPoint, Vector2 t1, Vector2 t2, Vector2 t3);

class FillStyle
{
public:
	virtual Color operator()(Vector2 aPos) const = 0;
	virtual ~FillStyle()						 = default;
};

class SolidFill : public FillStyle
{
	Color color;

public:
	explicit SolidFill(Color aColor)
	: color(aColor)
	{
	}
	Color operator()(Vector2 aPos) const override { return color; }
};

class RadialGradientFill : public FillStyle
{
	Vector2 center;
	int		radius;
	Color	centerRGB, edgeRGB;

public:
	RadialGradientFill(Vector2 aPos, int r, Color centerColor, Color edgeColor)
	: center(aPos)
	, radius(r)
	, centerRGB(centerColor)
	, edgeRGB(edgeColor)
	{
	}
	Color operator()(Vector2 aPos) const override
	{
		Color gradientColor;
		float x2 = float(aPos.x) + 0.5, y2 = float(aPos.y) + 0.5;
		float distX = x2 - center.x, distY = y2 - center.y;
		float distance = sqrt(distX * distX + distY * distY);

		if (distance >= radius)
			return edgeRGB;

		float t		  = distance / radius;
		gradientColor = lerpRGB(centerRGB, edgeRGB, t);
		return gradientColor;
	}
};

class GWindow
{
public:
	void	set_screen(unsigned char* rgb_out, int w, int h);
	XImage* create_ximage(Display* display, Visual* visual, int width, int height);
	void	fill_rectangle(Vector2 aPos, int width, int height, const FillStyle& fillStyle);
	void	fill_line(Vector2 aPos1, Vector2 aPos2, const FillStyle& fillStyle);
	void	fill_circle(Vector2 center, int radius, const FillStyle& fillStyle);
	void	fill_triangle(Vector2 p1, Vector2 p2, Vector2 p3, const FillStyle& fillStyle);

	void fill_pixel(Vector2 aPos, Color aColor);
	void blend_pixel(Vector2 aPos, Color aColor);

	void Update();
	void Tick();
	void Start();
	void Close();

	GWindow(int width = 640, int height = 480, std::string title = "Window");
	~GWindow();

protected:
	int											   WINDOW_WIDTH;
	int											   WINDOW_HEIGHT;
	int											   WINDOW_PIXEL;
	const int									   BYTES_PER_PIXEL = 4;
	std::string									   WINDOW_TITLE;
	unsigned char*								   screenbuffer;
	std::chrono::high_resolution_clock::time_point program_start_clock;
	std::chrono::duration<double>				   elapsed_time;
	float										   double_timestep;
	Display*									   m_Display;
	Visual*										   m_Visual;
	XImage*										   m_Image;
	Window										   m_Window;
	GC											   m_Graphics;
	XEvent										   m_Event;
};


};
