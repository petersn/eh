// Basic rendering demo.

using namespace std;
#include <iostream>
#include <functional>
#include <random>
#include <vector>

#include <sys/time.h>
#include <stdint.h>
#include <math.h>
#include <SDL.h>

#define WINDOWED 0
#define PLAYER_WIDTH 50
#define PLAYER_HEIGHT 50
#define MOVE_ACCELERATION 1000.0
#define GRAVITY_ACCELERATION 1000.0

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global storage. Eww.
int screen_width, screen_height;
SDL_Surface* screen;
int held_array[1000];
long long frame_number = 0;
double current_time = 0.0;
random_device rd;
mt19937 engine(rd());

vector<function<void(float)>> filters;

float player_x = 0;
float player_y = 0;
float player_vx = 0;
float player_vy = 0;
int player_jumps_remaining = 1000000;
float camera_x = 0;
float camera_y = 0;

// This code blatantly stolen from: https://en.wikipedia.org/wiki/Xorshift
uint32_t x = 1, y = 0, z = 0, w = 0;
uint32_t random_u32() {
    uint32_t t = x;
    t ^= t << 11;
    t ^= t >> 8;
    x = y; y = z; z = w;
    w ^= w >> 19;
    w ^= t;
    return w;
}

int iters_to_color(double iters) {
	double angle = fmod(iters, 360);
	int x = 255 * (1 - fabs(fmod(angle/60, 2) - 1));
	#define col(a, b, c) ((a<<16) | (b<<8) | c)
	if (0 <= angle && angle < 60)    return col(255, x, 0);
	if (60 <= angle && angle < 120)  return col(x, 255, 0);
	if (120 <= angle && angle < 180) return col(0, 255, x);
	if (180 <= angle && angle < 240) return col(0, x, 255);
	if (240 <= angle && angle < 300) return col(x, 0, 255);
	return col(255, 0, x);
}

#define PIXEL_LOOP for (int y = 0; y < screen_height; y++) for (int x = 0; x < screen_width; x++)
#define PIXEL_LOOP_SKIP_FIRST_AND_LAST_LINES for (int y = 1; y < screen_height - 1; y++) for (int x = 0; x < screen_width; x++)
#define PIXEL(x, y) (((int*)screen->pixels) + ((x) + (y) * screen_width))

static inline void set_pixel(int x, int y, int color) {
	if (x >= 0 and x < screen_width and y >= 0 and y < screen_height)
		*PIXEL(x, y) = color;
}

static inline int get_pixel(int x, int y) {
	if (x >= 0 and x < screen_width and y >= 0 and y < screen_height)
		return *PIXEL(x, y);
	return 0;
}
void draw_scrolling_pattern(float param) {
	float mult = frame_number;
	PIXEL_LOOP
		*PIXEL(x, y) = x + y + frame_number;
}

void additive_low_order_bit_noise(float param) {
	PIXEL_LOOP
		*PIXEL(x, y) += random_u32() & 0x01010101;
}

void horizontal_skew(float param) {
	PIXEL_LOOP_SKIP_FIRST_AND_LAST_LINES
		*PIXEL(x, y) = *PIXEL(x + (int)((random_u32() * param * 2.3283064365386963e-10) * 10.0 - param * 5.0), y);
}

void vertical_skew(float param) {
	PIXEL_LOOP_SKIP_FIRST_AND_LAST_LINES
		*PIXEL(x, y) = get_pixel(x, y + (int)((random_u32() * param * 2.3283064365386963e-10) * 10.0 - param * 5.0));
}

void horizontal_blur(float param) {
	PIXEL_LOOP_SKIP_FIRST_AND_LAST_LINES
		*PIXEL(x, y) += *PIXEL(x + 1, y - 1);
}

void game_init_logic() {
//	filters.push_back(draw_scrolling_pattern);
	filters.push_back(additive_low_order_bit_noise);
	filters.push_back(horizontal_skew);
	filters.push_back(vertical_skew);
//	filters.push_back(horizontal_blur);

	// Set the camera initially so y = 0 is at the bottom of the screen.
	player_x = 0;
	player_y = 0;
	camera_x = 0;
	camera_y = -screen_height;
}

void draw_box(int x, int y, int w, int h, int color) {
	for (int x_offset = 0; x_offset < w; x_offset++) {
		for (int y_offset = 0; y_offset < h; y_offset++) {
			set_pixel(x + x_offset, y + y_offset, color);
		}
	}
}

void main_loop() {
	struct timeval last_frame_start;
	gettimeofday(&last_frame_start, NULL);
	double dt = 0.0;
	double time_averaged_dt = 0.0;

	while (1) {
		// We begin each loop by getting events.
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
				// SDL_QUIT corresponds to someone hitting the X on the window decoration, so we close the program.
				case SDL_QUIT:
					SDL_Quit();
					exit(0);
					break;
				case SDL_KEYDOWN:
					// This corresponds to a user hitting escape, so we close the program. (keycode 27 = esc)
					if (ev.key.keysym.sym == 27) {
						SDL_Quit();
						exit(0);
					}
					if (ev.key.keysym.sym < 1000)
						held_array[ev.key.keysym.sym] = 1;

					// Jumping.
					if (ev.key.keysym.sym == SDLK_UP) {
						if (player_jumps_remaining) {
							player_jumps_remaining -= 1;
							player_vy = -800.0;
						}
					}
					if (ev.key.keysym.sym == SDLK_DOWN) {
						if (player_jumps_remaining) {
							player_jumps_remaining -= 1;
							player_vy = 800.0;
						}
					}
					break;
				case SDL_KEYUP:
					if (ev.key.keysym.sym < 1000)
						held_array[ev.key.keysym.sym] = 0;
					break;
			}
		}

		// ========== Do all game calculuations ==========
		struct timeval stop, result;
		gettimeofday(&stop, NULL);
		timersub(&stop, &last_frame_start, &result);
		dt = result.tv_sec + result.tv_usec * 1e-6;
		current_time += dt; // TODO: Do I really want to integrate up like this with all the error that gives?
		time_averaged_dt = 0.99 * time_averaged_dt + 0.01 * dt;
		last_frame_start = stop;
		// Print out a framerate indicator.
		if (frame_number % 500 == 0)
			cout << "FPS: " << 1.0 / time_averaged_dt << " " << dt << endl;

		frame_number++;
		if (held_array[SDLK_LEFT]) {
			float boost_factor = player_vx < 0 ? 1.0 : 3.0;
			player_vx -= boost_factor * MOVE_ACCELERATION * dt;
		}
		if (held_array[SDLK_RIGHT]) {
			float boost_factor = player_vx > 0 ? 1.0 : 3.0;
			player_vx += boost_factor * MOVE_ACCELERATION * dt;
		}
		player_vy += GRAVITY_ACCELERATION * dt;

		// Keep the player from sinking through the floor.
		if (player_y > -PLAYER_HEIGHT/2) {
			player_y = -PLAYER_HEIGHT/2;
			if (player_vy > 0)
				player_vy = 0;
		}
//		if (player_y > - screen_height / 2)
//			player_vy -= GRAVITY_ACCELERATION * dt;

		player_vx *= pow(0.9, dt);

		player_x += player_vx * dt;
		player_y += player_vy * dt;

		// We must make these calls, to avoid violating SDL's rules.
		if (SDL_MUSTLOCK(screen))
	        SDL_LockSurface(screen);

		// Draw the player.
		draw_box(player_x - camera_x - PLAYER_WIDTH/2, player_y - camera_y - PLAYER_HEIGHT/2, PLAYER_WIDTH, PLAYER_HEIGHT, iters_to_color(current_time * 50.0));

		// We now apply all the filters.
		for (int i = 0; i < filters.size(); i++) {
			auto f = filters[i];
			f(0.5 + 0.5 * sinf(current_time * 3.0 + 0*i*M_PI));
		}

		// We must make these calls, to avoid violating SDL's rules.
		if (SDL_MUSTLOCK(screen))
	        SDL_UnlockSurface(screen);

		// This call actually makes our changes visible, by flipping the double buffer.
		SDL_Flip(screen);
	}
}

int main(int argc, char** argv) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		cerr << "Unable to SDL_Init." << endl;
		return 1;
	}
	// This call lets us get the screen resolution, to be completely fullscreen.
	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	if (info == NULL) {
		cerr << "Unabe to get video info: " << SDL_GetError() << endl;
		return 1;
	}
	// Choose the maximum possible screen resolution to be our target resolution.
	// Of course, if you want a smaller window, set these to other values.
	screen_width  = info->current_w;
	screen_height = info->current_h;
//	screen_width = 960;
//	screen_height = 540;
	if (WINDOWED) {
		screen_width = 640;
		screen_height = 480;
	}
	int video_flags = 0;
	// These guys aren't super critical, but change video performance/behavior on some systems.
	video_flags |= SDL_GL_DOUBLEBUFFER;
	video_flags |= SDL_HWPALETTE;
	// Comment this one out if you don't want fullscreen!
	if (not WINDOWED)
		video_flags |= SDL_FULLSCREEN;
	screen = SDL_SetVideoMode(screen_width, screen_height, 32, video_flags);
	if (screen == NULL) {
		cerr << "Couldn't set video mode: " << SDL_GetError() << endl;
		SDL_Quit();
		return 1;
	}
	SDL_ShowCursor(SDL_DISABLE);
	for (int i = 0; i < 1000; i++)
		held_array[i] = 0;
	// This is logic that runs once.
	game_init_logic();
	// Drop into the main loop.
	main_loop();
	return 0;
}

