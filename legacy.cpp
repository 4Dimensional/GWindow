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

const int WINDOW_WIDTH	= 640;
const int WINDOW_HEIGHT = 480;

const int BYTES_PER_PIXEL = 4;
const int WINDOW_PIXEL	  = WINDOW_WIDTH * WINDOW_HEIGHT * BYTES_PER_PIXEL;

unsigned char* screenbuffer;
int*		   depthbuffer;

void	set_screen(unsigned char* rgb_out, int w, int h);
XImage* create_ximage(Display* display, Visual* visual, int width, int height);

int	 get_buffer_index(int x, int y);
int* get_buffer_pixel(int index);
int* get_buffer_pixel_color(int x, int y);

float lerp(float a, float b, float time, bool looping);
void  lerpRGB(int* result, int* color1, int* color2, float time);
float smoothstep(float t);
bool  point_in_triangle(int x, int y, int* p1, int* p2, int* p3);

void fill_pixel(int x, int y, int* color);
void blend_pixel(int x, int y, int* color);
int* create_color(int r, int g, int b, int a);
void free_color(int* color);

class FillStyle
{
public:
	virtual int* operator()(int x, int y) const = 0;
	virtual ~FillStyle()						= default;
};

class SolidFill : public FillStyle
{
	int* color;

public:
	explicit SolidFill(int* rgba)
	: color(rgba)
	{
	}
	int* operator()(int x, int y) const override { return color; }
};

class RadialGradientFill : public FillStyle
{
	int	 centerX, centerY, radius;
	int *centerRGB, *edgeRGB;

public:
	RadialGradientFill(int cx, int cy, int r, int* centerColor, int* edgeColor)
	: centerX(cx)
	, centerY(cy)
	, radius(r)
	, centerRGB(centerColor)
	, edgeRGB(edgeColor)
	{
	}
	int* operator()(int x, int y) const override
	{
		int* gradientColor = (int*) malloc(BYTES_PER_PIXEL * sizeof(int));
		if (gradientColor == nullptr)
		{
			std::cout << "Radiant gradient fill gradient color malloc fatal error!" << std::endl;
			exit(1);
		}
		float x2 = float(x) + 0.5, y2 = float(y) + 0.5;
		float distX = x2 - centerX, distY = y2 - centerY;
		float distance = sqrt(distX * distX + distY * distY);

		if (distance >= radius)
			return edgeRGB;

		float t = distance / radius;
		lerpRGB(gradientColor, centerRGB, edgeRGB, t);
		return gradientColor;
	}
};

void fill_rectangle(int x, int y, int width, int height, const FillStyle& fillStyle);
void fill_line(int x, int y, int x2, int y2, const FillStyle& fillStyle);
void fill_circle(int centerX, int centerY, int radius, const FillStyle& fillStyle);
void fill_triangle(int* p1, int* p2, int* p3, const FillStyle& fillStyle, bool freeptr);

int main(int argc, char* argv[])
{
	std::cout << "Connecting to X server..." << std::endl;
	Display* dpy = XOpenDisplay(NIL);
	assert(dpy);
	Visual* dvisual;
	XImage* ximage;

	auto						  program_start_clock = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed_time
		= std::chrono::high_resolution_clock::now() - program_start_clock;
	float double_timestep = (sin(elapsed_time.count()) + 1) / 2.0;

	screenbuffer = (unsigned char*) malloc(WINDOW_PIXEL);
	memset(screenbuffer, 255, WINDOW_PIXEL);
	depthbuffer = (int*) malloc(WINDOW_PIXEL * sizeof(int));
	memset(depthbuffer, 0, WINDOW_PIXEL);

	int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
	int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

	Window w = XCreateSimpleWindow(
		dpy, DefaultRootWindow(dpy), 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, blackColor, blackColor);

	XSelectInput(dpy, w, StructureNotifyMask | ExposureMask | KeyPressMask);

	XMapWindow(dpy, w);

	GC gc	= XCreateGC(dpy, w, 0, NIL);
	dvisual = DefaultVisual(dpy, 0);

	ximage = create_ximage(dpy, dvisual, WINDOW_WIDTH, WINDOW_HEIGHT);
	memcpy(ximage->data, screenbuffer, WINDOW_PIXEL);

	bool drawMode = false;
	for (;;)
	{
		XEvent e;
		while (XPending(dpy))
			XNextEvent(dpy, &e);
		if (e.type == MapNotify)
		{
			drawMode = true;
		}
		if (drawMode)
		{
			if (e.type == Expose)
			{
				memcpy(ximage->data, screenbuffer, WINDOW_PIXEL);
				memset(screenbuffer, 255, WINDOW_PIXEL);

				int* red   = create_color(255, 0, 0, 255);
				int* blue  = create_color(0, 0, 255, 100);
				int* green = create_color(0, 255, 0, 255);

				fill_rectangle(10, 10, 300, 50, SolidFill(green));
				fill_circle(100, 100, 100, RadialGradientFill(100, 100, 100, red, blue));

				int* p1 = (int*) malloc(2 * sizeof(int));
				int* p2 = (int*) malloc(2 * sizeof(int));
				int* p3 = (int*) malloc(2 * sizeof(int));

				p1[0] = 10;
				p1[1] = 10;
				p2[0] = 10;
				p2[1] = 100;
				p3[0] = 100;
				p3[1] = 10;

				fill_triangle(p1, p2, p3, SolidFill(red), true);

				free_color(red);
				free_color(blue);
				free_color(green);

				XPutImage(dpy, w, gc, ximage, 0, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
			}
		}

		if (e.type == KeyPress)
		{
			break;
		}

		elapsed_time	= std::chrono::high_resolution_clock::now() - program_start_clock;
		double_timestep = (sin(elapsed_time.count()) + 1) / 2.0;

		XFlush(dpy);
	}

	std::cout << "Closing window & freeing memory..." << std::endl;
	XCloseDisplay(dpy);
	XDestroyWindow(dpy, w);
	XDestroyImage(ximage);

	free(screenbuffer);
	free(depthbuffer);

	return 0;
}

XImage* create_ximage(Display* display, Visual* visual, int width, int height)
{
	unsigned char* image32 = (unsigned char*) malloc(width * height * BYTES_PER_PIXEL);
	if (image32 == nullptr)
	{
		std::cout << "Image32 malloc fatal error!" << std::endl;
		exit(1);
	}
	set_screen(image32, width, height);
	return XCreateImage(display, visual, 24, ZPixmap, 0, (char*) image32, width, height, 32, 0);
}

void set_screen(unsigned char* rgb_out, int w, int h)
{
	std::strncpy((char*) rgb_out, (const char*) screenbuffer, w * h * BYTES_PER_PIXEL);
	return;
}

int get_buffer_index(int x, int y) { return (y * WINDOW_WIDTH + x) * BYTES_PER_PIXEL; }

int* get_buffer_pixel(int index)
{
	int	 pixelIndex = floor((float) index / BYTES_PER_PIXEL);
	int	 y			= floor((float) pixelIndex / WINDOW_WIDTH);
	int	 x			= pixelIndex % WINDOW_WIDTH;
	int* r			= (int*) malloc(2 * sizeof(int));
	if (r == nullptr)
	{
		std::cout << "Buffer pixel r value malloc fatal error!" << std::endl;
		exit(1);
	}
	r[0] = x;
	r[1] = y;
	return r;
}

int* get_buffer_pixel_color(int x, int y)
{
	int	 index = get_buffer_index(x, y);
	int* color = (int*) malloc(BYTES_PER_PIXEL * sizeof(int));
	if (color == nullptr)
	{
		std::cout << "Buffer pixel get color malloc fatal error!" << std::endl;
		exit(1);
	}

	color[0] = screenbuffer[index + 0];
	color[1] = screenbuffer[index + 1];
	color[2] = screenbuffer[index + 2];
	color[3] = screenbuffer[index + 3];

	return color;
}

float lerp(float a, float b, float time, bool looping)
{
	return (looping) ? ((a * (1.0 - fmod(time, 1.0f))) + (b * fmod(time, 1.0f)))
					 : (a * (1.0 - time)) + b * time;
}

void lerpRGB(int* result, int* color1, int* color2, float time)
{
	result[0] = (int) lerp(color1[0], color2[0], time, false);
	result[1] = (int) lerp(color1[1], color2[1], time, false);
	result[2] = (int) lerp(color1[2], color2[2], time, false);
	result[3] = (int) lerp(color1[3], color2[3], time, false);
	return;
}

float smoothstep(float t) { return t * t * (3 - t * 2); }

static int triangle_area(int* p1, int* p2, int* p3)
{
	return abs((p1[0] * (p2[1] - p3[1]) + p2[0] * (p3[1] - p1[1]) + p3[0] * (p1[1] - p2[1])) / 2);
}
bool point_in_triangle(int x, int y, int* p1, int* p2, int* p3)
{
	int* xy = (int*) malloc(2 * sizeof(int));
	xy[0]	= x;
	xy[1]	= y;
	int a	= triangle_area(p1, p2, p3);
	int a1	= triangle_area(xy, p2, p3);
	int a2	= triangle_area(p1, xy, p3);
	int a3	= triangle_area(p1, p2, xy);
	free(xy);
	return (a == a1 + a2 + a3);
}

void fill_pixel(int x, int y, int* color)
{
	if ((x < 0 || x > WINDOW_WIDTH) || (y < 0 || y > WINDOW_HEIGHT))
		return;
	int index				= get_buffer_index(x, y);
	screenbuffer[index + 0] = color[0];	 // B
	screenbuffer[index + 1] = color[1];	 // G
	screenbuffer[index + 2] = color[2];	 // R
	screenbuffer[index + 3] = color[3];	 // A
	return;
}

void blend_pixel(int x, int y, int* color)
{
	int* dst = get_buffer_pixel_color(x, y);
	int	 blended[BYTES_PER_PIXEL];
	lerpRGB(blended, dst, color, (float) color[3] / 255);
	free(dst);
	fill_pixel(x, y, blended);
	return;
}

void fill_rectangle(int x, int y, int width, int height, const FillStyle& fillStyle)
{
	for (int y1 = y; y1 < y + height; ++y1)
	{
		for (int x1 = x; x1 < x + width; x1++)
		{
			int* color = fillStyle(x1, y1);
			blend_pixel(x1, y1, color);

			if (dynamic_cast<const RadialGradientFill*>(&fillStyle) != nullptr)
			{
				free_color(color);
			}
		}
	}
	return;
}

void fill_line(int x, int y, int x2, int y2, const FillStyle& fillStyle)
{
	bool yLonger = false;
	int	 incrementVal, endVal;
	int	 shortLen = y2 - y;
	int	 longLen  = x2 - x;
	if (abs(shortLen) > abs(longLen))
	{
		int swap = shortLen;
		shortLen = longLen;
		longLen	 = swap;
		yLonger	 = true;
	}
	endVal = longLen;
	if (longLen < 0)
	{
		incrementVal = -1;
		longLen		 = -longLen;
	}
	else
		incrementVal = 1;
	int decInc;
	if (longLen == 0)
		decInc = 0;
	else
		decInc = (shortLen << 16) / longLen;
	int j = 0;
	if (yLonger)
	{
		for (int i = 0; i != endVal; i += incrementVal)
		{
			int* color = fillStyle(x + (j >> 16), y + i);
			blend_pixel(x + (j >> 16), y + i, color);

			if (dynamic_cast<const RadialGradientFill*>(&fillStyle) != nullptr)
			{
				free_color(color);
			}

			j += decInc;
		}
	}
	else
	{
		for (int i = 0; i != endVal; i += incrementVal)
		{
			int* color = fillStyle(x + i, y + (j >> 16));
			fill_pixel(x + i, y + (j >> 16), color);

			if (dynamic_cast<const RadialGradientFill*>(&fillStyle) != nullptr)
			{
				free_color(color);
			}

			j += decInc;
		}
	}
	return;
}

void fill_circle(int centerX, int centerY, int radius, const FillStyle& fillStyle)
{
	float x1 = float(centerX) - radius, y1 = float(centerY) - radius;
	float x2 = float(centerX) + radius, y2 = float(centerY) + radius;
	for (int y = y1; y < y2; ++y)
	{
		for (int x = x1; x < x2; ++x)
		{
			float distX = (x - centerX + 0.5), distY = (y - centerY + 0.5);
			float distance = sqrt(distX * distX + distY * distY);
			if (distance <= radius)
			{
				int* rgb = fillStyle(x, y);
				blend_pixel(x, y, rgb);
				if (dynamic_cast<const RadialGradientFill*>(&fillStyle) != nullptr)
				{
					free_color(rgb);
				}
			}
		}
	}
	return;
}

void fill_triangle(int* p1, int* p2, int* p3, const FillStyle& fillStyle, bool freeptr)
{
	if ((p1 == nullptr || p2 == nullptr) || p3 == nullptr)
	{
		std::cout << "Non-fatal nullptr to vector2 in function fill_triangle." << std::endl;
		return;
	}

	int maxX = std::max(p1[0], std::max(p2[0], p3[0]));
	int minX = std::min(p1[0], std::min(p2[0], p3[0]));
	int maxY = std::max(p1[1], std::max(p2[1], p3[1]));
	int minY = std::min(p1[1], std::min(p2[1], p3[1]));

	for (int x = minX; x <= maxX; ++x)
	{
		for (int y = minY; y <= maxY; ++y)
		{
			if (point_in_triangle(x, y, p1, p2, p3))
			{
				int* rgb = fillStyle(x, y);
				blend_pixel(x, y, rgb);
				if (dynamic_cast<const RadialGradientFill*>(&fillStyle) != nullptr)
				{
					free_color(rgb);
				}
			}
		}
	}

	if (freeptr)
	{
		free(p1);
		free(p2);
		free(p3);
	}
	return;
}

int* create_color(int r, int g, int b, int a)
{
	int* color = (int*) malloc(BYTES_PER_PIXEL * sizeof(int));
	if (color == nullptr)
	{
		std::cout << "Create color malloc fatal error!" << std::endl;
		exit(1);
	}
	color[0] = b;
	color[1] = g;
	color[2] = r;
	color[3] = a;
	return color;
}

void free_color(int* color)
{
	free(color);
	return;
}
