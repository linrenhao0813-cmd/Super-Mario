#pragma once

#include <array>
#include <chrono>
#include <vector>

constexpr float BRICK_PARTICLE_SPEED = 4;
constexpr float COIN_JUMP_SPEED = -6;
constexpr float GOOMBA_SPEED = 1;
constexpr float GRAVITY = 0.25f;
constexpr float KOOPA_SHELL_SPEED = 4;
constexpr float KOOPA_SPEED = 1;
constexpr float MARIO_ACCELERATION = 0.25f;
constexpr float MARIO_JUMP_SPEED = -4;
constexpr float MARIO_WALK_SPEED = 2;
constexpr float MAX_VERTICAL_SPEED = 8;
constexpr float MUSHROOM_SPEED = 1;

constexpr unsigned char CELL_SIZE = 16;
constexpr unsigned char MARIO_BLINKING = 4;
constexpr unsigned char MARIO_DEATH_DURATION = 32;
constexpr unsigned char MARIO_GROWTH_DURATION = 64;
constexpr unsigned char MARIO_INVINCIBILITY_DURATION = 128;
constexpr unsigned char MARIO_JUMP_TIMER = 8;
constexpr unsigned char SCREEN_RESIZE = 2;

constexpr unsigned short COIN_ANIMATION_SPEED = 4;
constexpr unsigned short ENTITY_UPDATE_AREA = 64;
constexpr unsigned short GOOMBA_DEATH_DURATION = 32;
constexpr unsigned short GOOMBA_WALK_ANIMATION_SPEED = 32;
constexpr unsigned short KOOPA_GET_OUT_ANIMATION_SPEED = 4;
constexpr unsigned short KOOPA_GET_OUT_DURATION = 512;
constexpr unsigned short KOOPA_WALK_ANIMATION_SPEED = 32;
constexpr unsigned short MARIO_WALK_ANIMATION_SPEED = 4;
constexpr unsigned short QUESTION_BLOCK_ANIMATION_SPEED = 8;
constexpr unsigned short SCREEN_HEIGHT = 240;
constexpr unsigned short SCREEN_WIDTH = 320;

constexpr std::chrono::microseconds FRAME_DURATION(16667);

enum Cell
{
	ActivatedQuestionBlock,
	Brick,
	Coin,
	Empty,
	Pipe,
	QuestionBlock,
	Wall
};

struct Object
{
	float horizontal_speed;
	float vertical_speed;
	float x;
	float y;

	Object(const float i_x, const float i_y, const float i_horizontal_speed = 0, const float i_vertical_speed = 0) :
		horizontal_speed(i_horizontal_speed),
		vertical_speed(i_vertical_speed),
		x(i_x),
		y(i_y)
	{

	}
};

typedef std::vector<std::array<Cell, SCREEN_HEIGHT / CELL_SIZE>> Map;
