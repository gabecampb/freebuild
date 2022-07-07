# FreeBuild

A 3D brick building sandbox game.

## Building

This project aims to be self-contained C code. The only third-party dependency is GLFW.

To build the project on Linux/BSD, run the following:

`cc main.c -o bin -lGL -lglfw -lm`

Then simply run the game with `./bin`!

## Controls

The demo scene has the following controls:

Left/right - look left/right

WASD - movement

Ctrl - toggle physics wireframe view

V - toggle noclip mode

R - reset player position to (0,0,0)

Space - jump (noclip disabled) or descend (noclip enabled)

Shift - ascend (noclip enabled)

## Features

✓ Interactive player character & camera

✓ AABB collisions system

✓ Gravity, jumping, stair climbing

_ Model texturing

_ Entities

_ HUD

_ Building

_ World saving

_ Multiplayer
