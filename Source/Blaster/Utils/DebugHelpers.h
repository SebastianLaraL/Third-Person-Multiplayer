#pragma once

// Macro for drawing debug shapes in development or test configuration. 
// They are stripped out in Shipping builds.

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#define DRAW_DEBUG_SPHERE(World, Location, Radius, Segments, Color, Persistent, LifeTime, DepthPriority, Thickness) \
	DrawDebugSphere(World, Location, Radius, Segments, Color, Persistent, LifeTime, DepthPriority, Thickness)

#define DRAW_DEBUG_BOX(World, Center, Box, Rotation, Color, Persistent) \
	DrawDebugBox(World, Center, Box, Rotation, Color, Persistent)

#define DRAW_DEBUG_CAPSULE(World, Center, HalfHeight, Radius, Rotation, Color, PersistentLines, LifeTime, DepthPriority, Thickness) \
	DrawDebugCapsule(World, Center, HalfHeight, Radius, Rotation, Color, PersistentLines, LifeTime, DepthPriority, Thickness)

#else

#define DRAW_DEBUG_SPHERE(World, Location, Radius, Segments, Color, Persistent, LifeTime, DepthPriority, Thickness)
#define DRAW_DEBUG_BOX(World, Center, Box, Rotation, Color, Persistent)
#define DRAW_DEBUG_CAPSULE(World, Center, HalfHeight, Radius, Rotation, Color, PersistentLines, LifeTime, DepthPriority, Thickness)

#endif