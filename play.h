#ifndef PLAY_H
#define PLAY_H

#include "bank.h"

/* World-space actor. x,y is the 32x32 sprite's top-left.
 * Terrain collision uses the sprite mask: colour 0 is empty
 * (same rule as hw_hit_bg / C64 mask sprites). `mask` may name a
 * tighter pattern while `pat` is what gets drawn. */

enum { ACTOR_SPRITE = 0 };

typedef struct {
    float x, y, vx, vy;
    float spawn_x, spawn_y;
    int grounded;
    int pat;  /* drawn */
    int mask; /* collision pattern; -1 = same as pat */
} Actor;

void actor_spawn(Actor *a, Bank *b);
void actor_platform(Actor *a, const Bank *b, float dt); /* gravity + jump */
void actor_topdown(Actor *a, const Bank *b, float dt);  /* 4-way, no gravity */
void actor_draw(const Actor *a);
void play_cam_follow(const Bank *b, const Actor *a);

#endif
