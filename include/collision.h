#ifndef COLLISION_H
#define COLLISION_H

#include "map.h"
#include "types.h"
#include <stdbool.h>

typedef struct {
    double x, y;
    double w, h;
} CollisionBox;


typedef struct {
    double x, y;
    double w, h;
} AABB;


typedef enum {
    COLLISION_NONE   = 0,
    COLLISION_LEFT   = 1 << 0,
    COLLISION_RIGHT  = 1 << 1,
    COLLISION_TOP    = 1 << 2,
    COLLISION_BOTTOM = 1 << 3,
} CollisionSide;

/* 单次轴分离解算的返回信息。 */
typedef struct {
    int sides;
    double surfaceTop;
    double surfaceBottom;
    bool onGround;
} CollisionResult;


typedef struct {
    Vec2 position;
    Vec2 velocity;
    double offX, offY;
    double width, height;
} Body;


AABB CollisionGetBodyAABB(const Body *body);


bool CollisionAABBOverlap(AABB a, AABB b);

bool CollisionIsTileSolid(
    MapData *map, int tileX, int tileY,
    double *outSurfaceTop, double *outSurfaceBottom,
    const AABB *queryAABB);

CollisionResult CollisionMoveX(Body *body, MapData *map, double dx);

CollisionResult CollisionMoveY(Body *body, MapData *map, double dy);

#endif
