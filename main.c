/* main.c
   a simple 3D sandbox brick game
   
   Gabriel Campbell
   created 2022-07-05 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <float.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

const uint32_t vsync = 1;

double cursor_x, cursor_y;
int32_t prev_x, prev_y;
int32_t scroll_x, scroll_y;
int32_t prev_scroll_x, prev_scroll_y;
float window_width = 640, window_height = 480;
float fovy = 60;
float near = 0.1;
float far = 100;

uint8_t enable_physics_draw = 1;

typedef struct vec2 { float x,y; } vec2;
typedef struct vec3 { float x,y,z; } vec3;
typedef struct vec4 { float x,y,z,w; } vec4;

vec3 __sub_vec3(vec3 a, vec3 b);
vec3 __add_vec3(vec3 a, vec3 b);
vec3 __scale_vec3(vec3 v, float s);
typedef struct brick_t brick_t;
typedef struct collision_t collision_t;
typedef struct camera_t camera_t;
void add_brick_collider_aabb(int32_t brick_id);
uint32_t add_collider_aabb(vec3 pos, vec3 scale);

typedef struct camera_t {
	vec3 pos;
	vec4 quat;
	int32_t zoom;
} camera_t;

typedef struct player_t {
	uint32_t entity_id;
	char* name;
	camera_t camera;
	uint8_t focused;
	int32_t selected_brick_id;
} player_t;

player_t* player;


/*==================================================*/
/*				MESH DATA AND MANAGEMENT			*/
/*==================================================*/

typedef struct mesh_t {
	GLuint vbo_id, ibo_id, vao_id;
	uint32_t n_indices;
	uint32_t vtx_format;				// 0 = v3 pos, 1 = v3 pos v3 norm (default mesh), 2 = v3 pos v3 norm v2 tex
	uint8_t has_ibo;
} mesh_t;

mesh_t* meshes;
uint32_t n_meshes;

// create a mesh and return the mesh ID
uint32_t create_mesh(float* vtx_data, uint16_t* idx_data, uint32_t vbo_size, uint32_t ibo_size, uint32_t vtx_format) {
	uint32_t stride = 0;
	switch(vtx_format) {
		case 0: stride = 12; break;
		case 1: stride = 24; break;
		case 2: stride = 32; break;
		default: printf("internal error: create_mesh given invalid vtx_format\n"); exit(1);
	}

	GLuint buffers[2];
	glGenBuffers(2,buffers);
	glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, vbo_size, vtx_data, GL_STATIC_DRAW);
	if(idx_data) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,ibo_size,idx_data,GL_STATIC_DRAW);
	}
	GLuint vao_id = 0;
	glGenVertexArrays(1,&vao_id);
	glBindVertexArray(vao_id);
	glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);
	if(idx_data) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);
	switch(vtx_format) {
		case 0:			// v3 pos
			glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,0);
			glEnableVertexAttribArray(0); break;
		case 1:			// v3 pos, v3 norm
			glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,0);
			glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)12);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1); break;
		case 2:			// v3 pos, v3 norm, v2 tex coord
			glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,0);
			glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)12);
			glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)24);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2); break;
	}
	meshes = realloc(meshes, sizeof(mesh_t)*(n_meshes+1));
	meshes[n_meshes].vbo_id = buffers[0];
	meshes[n_meshes].ibo_id = buffers[1];
	meshes[n_meshes].vao_id = vao_id;
	meshes[n_meshes].n_indices = idx_data ? ibo_size/2 : vbo_size/stride;
	meshes[n_meshes].vtx_format = vtx_format;
	meshes[n_meshes].has_ibo = idx_data ? 1 : 0, meshes[n_meshes].ibo_id = buffers[1];
	return n_meshes++;
}

// create the default brick mesh (1x1x1)
void init_mesh() {
	// 12 triangles (36 indices)
	uint16_t ibo_data[] = {
		0, 	1,	2,	 2, 1, 	3,
		4, 	5,	6,	 6, 5, 	7,
		8, 	9, 	10, 10, 9, 	11,
		12, 13, 14, 14, 13, 15,
		16, 17, 18, 18, 17, 19,
		20, 21, 22, 22, 21, 23 };
	// 24 vertices (3 different vertices per corner, as each corner is shared by 3 faces)
	float vbo_data[] = {
		-0.5, -0.5, 0.5,	0, 0, 1,	0, 0,		// face 0
		0.5, -0.5, 0.5,		0, 0, 1,	0, 1,
		-0.5, 0.5, 0.5,		0, 0, 1,	1, 0,
		0.5, 0.5, 0.5,		0, 0, 1,	1, 1,
		-0.5, 0.5, 0.5,		0, 1, 0,	0, 0,		// face 1
		0.5, 0.5, 0.5,		0, 1, 0,	0, 1,
		-0.5, 0.5, -0.5,	0, 1, 0,	1, 0,
		0.5, 0.5, -0.5,		0, 1, 0,	1, 1,
		-0.5, 0.5, -0.5,	0, 0, -1,	1, 1,		// face 2
		0.5, 0.5, -0.5,		0, 0, -1,	0, 1,
		-0.5, -0.5, -0.5,	0, 0, -1,	1, 0,
		0.5, -0.5, -0.5,	0, 0, -1,	0, 0,
		-0.5, -0.5, -0.5,	0, -1, 0,	0, 0,		// face 3
		0.5, -0.5, -0.5,	0, -1, 0,	0, 1,
		-0.5, -0.5, 0.5,	0, -1, 0,	1, 0,
		0.5, -0.5, 0.5,		0, -1, 0,	1, 1,
		0.5, -0.5, 0.5,		1, 0, 0,	0, 0,		// face 4
		0.5, -0.5, -0.5,	1, 0, 0,	0, 1,
		0.5, 0.5, 0.5,		1, 0, 0,	1, 0,
		0.5, 0.5, -0.5,		1, 0, 0,	1, 1,
		-0.5, -0.5, -0.5,	-1, 0, 0,	0, 0,		// face 5
		-0.5, -0.5, 0.5,	-1, 0, 0,	0, 1,
		-0.5, 0.5, -0.5,	-1, 0, 0,	1, 0,
		-0.5, 0.5, 0.5,		-1, 0, 0,	1, 1
	};

	create_mesh(vbo_data, ibo_data, sizeof(vbo_data), sizeof(ibo_data), 2);
}

/*==================================================*/
/*				TEXTURE LOADING						*/
/*==================================================*/

GLuint* gl_textures;
uint32_t n_textures;

// return 0 on failure
GLuint load_texture_from_file(char* path) {
	uint32_t w, h, comp;
	uint8_t* image = stbi_load(path, &w, &h, &comp, 0);
	if(!image) {
		printf("internal error: failed to load texture from file %s\n", path);
		return 0;
	}
	// create texture
	GLuint tbo_id;
	glGenTextures(1,&tbo_id);
	glBindTexture(GL_TEXTURE_2D, tbo_id);
	// upload image to TBO
	if(comp == 3)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	else if(comp == 4)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	// return created texture's ID (or 0 on failure)
	gl_textures = realloc(gl_textures, sizeof(GLuint)*(n_textures+1));
	gl_textures[n_textures++] = tbo_id;
	stbi_image_free(image);
	return tbo_id;
}

/*==================================================*/
/*				WORLD DATA AND MANAGEMENT			*/
/*==================================================*/

typedef struct collision_t {
	vec3 pos;		// minimum
	vec3 dim;
	int32_t brick_id;				// -1 if not a brick collider
	uint8_t deleted;
} collision_t;

typedef struct brick_t {
	uint32_t mesh_id;
	vec3 pos, scale;
	vec4 quat, color;
	GLuint texture_ids[6];	// for each face, the ID of a texture. 0 if untextured.
	uint8_t repeat_textures[6];
	uint8_t has_gravity, has_collision;
	uint8_t deleted;
} brick_t;

typedef struct world_t {
	brick_t* bricks;
	uint32_t n_bricks;
	collision_t* colls;
	uint32_t n_colls;
	char* name;
} world_t;

world_t* world;

void init_world() {
	char* name = "Test World";
	world = calloc(1,sizeof(world_t));
	world->name = calloc(1,sizeof(name)+1);
	strcpy(world->name,name);
}

void add_brick(world_t* world, vec3 pos, vec4 quat, vec3 scale, vec4 color, uint32_t mesh_id,
	uint8_t has_gravity, uint8_t has_collision) {
	brick_t new_brick;
	new_brick.pos = pos;
	new_brick.quat = quat;
	new_brick.scale = scale;
	new_brick.color = color;
	new_brick.mesh_id = mesh_id;
	new_brick.has_gravity = has_gravity;
	new_brick.has_collision = has_collision;
	memset(&new_brick.texture_ids,0,sizeof(GLuint)*6);
	new_brick.texture_ids[1] = gl_textures[0];	// top
	new_brick.texture_ids[3] = gl_textures[1];	// bottom
	new_brick.repeat_textures[1] = 1;
	new_brick.repeat_textures[3] = 1;
	new_brick.deleted = 0;
	world->bricks = realloc(world->bricks, sizeof(brick_t)*(world->n_bricks+1));
	world->bricks[world->n_bricks++] = new_brick;
	if(has_collision) add_brick_collider_aabb(world->n_bricks-1);
}

void delete_brick(uint32_t brick_id) {
	if(!world->bricks[brick_id].deleted) {
		world->bricks[brick_id].deleted = 1;
		for(uint32_t i = 0; i < world->n_colls; i++)
			if(world->colls[i].brick_id == brick_id) world->colls[i].deleted = 1;
	}
}

void add_brick_texture(uint32_t brick_id, uint8_t face, GLuint texture, uint8_t repeat) {
	world->bricks[brick_id].texture_ids[face] = texture;
	world->bricks[brick_id].repeat_textures[face] = repeat;
}


/*==================================================*/
/*				ENTITIES AND MANAGEMENT				*/
/*==================================================*/

typedef struct entity_t {
	vec3 pos;
	vec4 quat;
	uint8_t is_humanoid;	// whether or not to render as a character
	uint32_t mesh_id;		// mesh to render as; ignored if is_humanoid
	uint32_t coll_id;

	// for humanoids
	uint32_t jump_state;	// 0=able to jump, 1=cannot jump; mid-jump or falling
	float fall_distance;	// distance fallen without collision
	float health;			// 0-1
	vec4 part_colors[6];	// torso, left arm, right arm, left leg, right leg, head
} entity_t;

entity_t* entities;
uint32_t n_entities;

uint32_t add_humanoid_entity(vec3 pos, vec4 quat, float health, vec4* colors) {
	entity_t new_entity;
	memset(&new_entity,0,sizeof(entity_t));
	new_entity.pos = pos;
	new_entity.quat = quat;
	new_entity.is_humanoid = 1;
	new_entity.health = health;
	new_entity.jump_state = 1;
	for(uint32_t i = 0; i < 6; i++)
		new_entity.part_colors[i] = colors[i];
	vec3 aabb_scale = {2,4,2};
	vec3 aabb_pos = new_entity.pos;
	aabb_pos.y -= 1;
	new_entity.coll_id = add_collider_aabb(aabb_pos, aabb_scale);
	entities = realloc(entities, sizeof(entity_t)*(n_entities+1));
	entities[n_entities] = new_entity;
	return n_entities++;
}


/*==================================================*/
/*				PHYSICS								*/
/*==================================================*/

// given a brick, calc + add a new collider
void add_brick_collider_aabb(int32_t brick_id) {
	brick_t brick = world->bricks[brick_id];
	if(!brick.mesh_id) {		// default mesh has a known bounding box
		vec3 half_scale = __scale_vec3(brick.scale, 0.5);
		vec3 pos = __sub_vec3(brick.pos, half_scale);
		collision_t coll = { pos, brick.scale, brick_id, 0 };
		world->colls = realloc(world->colls,sizeof(collision_t)*(world->n_colls+1));
		world->colls[world->n_colls++] = coll;
	} else printf("error in add_brick_collider_aabb: auto-calculation of bounding box only implemented for default brick mesh\n");
}
	
// calc + add a new collider (returns collider ID)
uint32_t add_collider_aabb(vec3 pos, vec3 scale) {
	vec3 half_scale = __scale_vec3(scale, 0.5);
	pos = __sub_vec3(pos, half_scale);
	collision_t coll = { pos, scale, -1, 0 };
	world->colls = realloc(world->colls,sizeof(collision_t)*(world->n_colls+1));
	world->colls[world->n_colls] = coll;
	return world->n_colls++;
}

// check for collision between a given AABB and all others
uint8_t check_collision_aabb(uint32_t coll_id) {
	collision_t coll = world->colls[coll_id];
	vec3 coll_max = __add_vec3(coll.pos,coll.dim);
	for(uint32_t i = 0; i < world->n_colls; i++) {
		if(i == coll_id || world->colls[i].deleted) continue;
		vec3 min = world->colls[i].pos;
		vec3 max = __add_vec3(min,world->colls[i].dim);
		if((coll.pos.x <= max.x && coll_max.x >= min.x)
		&& (coll.pos.y <= max.y && coll_max.y >= min.y)
		&& (coll.pos.z <= max.z && coll_max.z >= min.z))
			return 1;
	}
	return 0;
}

typedef struct intersection_t {
	float t;				// distance from origin to intersection
	uint32_t coll_id;		// ID of collider intersected with
} intersection_t;

// check if a ray collides with any collider; if so, return ID of the collider, otherwise return -1
// 'intersection' is newly allocated list of collider IDs that the ray intersects with, and set to 0 if there's no intersections
// returns number of intersections (the closest intersection, if closest_hit is non-zero)
uint32_t check_ray_intersection(vec3 ray_pos, vec3 ray_dir, intersection_t** intersections_ret, uint8_t closest_hit) {
	if(!intersections_ret) {
		printf("internal error at check_ray_intersection: intersections arg is 0.\n");
		exit(1);
	}

	uint32_t n_intersections = 0;
	intersection_t* intersections = 0;

	for(uint32_t i = 0; i < world->n_colls; i++) {
		if(world->colls[i].deleted) continue;
		vec3 bmin = world->colls[i].pos;
		vec3 bmax = __add_vec3(world->colls[i].pos,world->colls[i].dim);

		vec3 dirfrac = { 1.0f/ray_dir.x, 1.0f/ray_dir.y, 1.0f/ray_dir.z };
		float t1 = (bmin.x - ray_pos.x)*dirfrac.x;
		float t2 = (bmax.x - ray_pos.x)*dirfrac.x;
		float t3 = (bmin.y - ray_pos.y)*dirfrac.y;
		float t4 = (bmax.y - ray_pos.y)*dirfrac.y;
		float t5 = (bmin.z - ray_pos.z)*dirfrac.z;
		float t6 = (bmax.z - ray_pos.z)*dirfrac.z;

		float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
		float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

		if(tmax < 0) continue;			// intersection, but AABB is behind ray
		if(tmin > tmax) continue;		// no intersection

		// intersection; add to intersections list
		intersection_t new_intersection;
		new_intersection.t = tmin;		// length of ray until intersection
		new_intersection.coll_id = i;
		intersections = realloc(intersections, sizeof(intersection_t)*(n_intersections+1));
		intersections[n_intersections++] = new_intersection;
	}

	if(closest_hit && n_intersections) {	// return only the closest hit in the 'intersections' array (lowest 't' value)
		uint32_t min_idx = 0;
		float min_t = FLT_MAX;
		for(uint32_t i = 0; i < n_intersections; i++)
			if(intersections[i].t < min_t) {
				min_idx = i;
				min_t = intersections[i].t;
			}
		intersections[0] = intersections[min_idx];
		intersections = realloc(intersections,sizeof(intersection_t));
		n_intersections = 1;
	}

	*intersections_ret = intersections;
	return n_intersections;
}

// step the physics simulation
void physics_step() {
	float gravity_step = 0.1;
	// for each brick collider with gravity, check if going down some is possible
	for(uint32_t i = 0; i < world->n_colls; i++)
		if(world->colls[i].brick_id != -1 && world->bricks[world->colls[i].brick_id].has_gravity
		&& !world->bricks[world->colls[i].brick_id].deleted) {
			world->colls[i].pos.y -= gravity_step;
			world->bricks[world->colls[i].brick_id].pos.y -= gravity_step;
			if(check_collision_aabb(i)) {
				world->colls[i].pos.y += gravity_step;
				world->bricks[world->colls[i].brick_id].pos.y += gravity_step;
			}
		}
	// for each entity's collider, check if going down some is possible
	for(uint32_t i = 0; i < n_entities; i++) {
		entity_t* entity = &entities[i];
		world->colls[entity->coll_id].pos.y -= gravity_step;
		entity->pos.y -= gravity_step;
		entity->jump_state = 1;
		entity->fall_distance += gravity_step;
		if(check_collision_aabb(entity->coll_id)) {
			world->colls[i].pos.y += gravity_step;
			entity->pos.y += gravity_step;
			entity->jump_state = 0;						// on the ground; can jump again
			entity->fall_distance = 0;
		}
	}
	// for each brick with gravity and no collider, move down some
	for(uint32_t i = 0; i < world->n_bricks; i++)
		if(world->bricks[i].has_gravity && !world->bricks[i].has_collision && !world->bricks[i].deleted)
			world->bricks[i].pos.y -= gravity_step;
}

void translate_brick(int32_t brick_id, vec3 translation) {
	brick_t brick = world->bricks[brick_id];
	vec3 new_pos = __add_vec3(brick.pos,translation);
	if(brick.has_collision) {
		for(uint32_t i = 0; i < world->n_colls; i++)
			if(world->colls[i].brick_id == brick_id) {
				if(check_collision_aabb(i)) return;
				else break;
			}
		for(uint32_t i = 0; i < world->n_colls; i++) {
			if(world->colls[i].brick_id == brick_id) {
				world->bricks[brick_id].pos = new_pos;
				vec3 aabb_pos = __sub_vec3(new_pos,__scale_vec3(brick.scale,0.5));
				world->colls[i].pos = aabb_pos;
			}			
		}
	} else world->bricks[brick_id].pos = new_pos;
			
}

void set_brick_pos(uint32_t brick_id, vec3 new_pos) {
	brick_t brick = world->bricks[brick_id];
	world->bricks[brick_id].pos = new_pos;
	if(brick.has_collision)
		for(uint32_t i = 0; i < world->n_colls; i++)
			if(world->colls[i].brick_id == brick_id) {
				vec3 aabb_pos = __sub_vec3(new_pos,__scale_vec3(brick.scale,0.5));
				world->colls[i].pos = aabb_pos;
			}
}



/*==================================================*/
/*				 MATHS AND CAMERA					*/
/*==================================================*/

typedef struct mat4 {					// m[row | col] - stored in column-major
	float m00, m10, m20, m30;
	float m01, m11, m21, m31;
	float m02, m12, m22, m32;
	float m03, m13, m23, m33;
} mat4;

typedef camera_t camera_t;

mat4 identity() {
	mat4 id;
	id.m00 = 1.0;	id.m01 = 0.0;	id.m02 = 0.0;	id.m03 = 0.0;
	id.m10 = 0.0;	id.m11 = 1.0;	id.m12 = 0.0;	id.m13 = 0.0;
	id.m20 = 0.0;	id.m21 = 0.0;	id.m22 = 1.0;	id.m23 = 0.0;
	id.m30 = 0.0;	id.m31 = 0.0;	id.m32 = 0.0;	id.m33 = 1.0;
	return id;
}

mat4 mat4_mat4(mat4 a, mat4 b) {
	mat4 p;
	// first row
	p.m00 = a.m00*b.m00 + a.m01*b.m10 + a.m02*b.m20 + a.m03*b.m30;
	p.m01 = a.m00*b.m01 + a.m01*b.m11 + a.m02*b.m21 + a.m03*b.m31;
	p.m02 = a.m00*b.m02 + a.m01*b.m12 + a.m02*b.m22 + a.m03*b.m32;
	p.m03 = a.m00*b.m03 + a.m01*b.m13 + a.m02*b.m23 + a.m03*b.m33;
	// second row
	p.m10 = a.m10*b.m00 + a.m11*b.m10 + a.m12*b.m20 + a.m13*b.m30;
	p.m11 = a.m10*b.m01 + a.m11*b.m11 + a.m12*b.m21 + a.m13*b.m31;
	p.m12 = a.m10*b.m02 + a.m11*b.m12 + a.m12*b.m22 + a.m13*b.m32;
	p.m13 = a.m10*b.m03 + a.m11*b.m13 + a.m12*b.m23 + a.m13*b.m33;
	// third row
	p.m20 = a.m20*b.m00 + a.m21*b.m10 + a.m22*b.m20 + a.m23*b.m30;
	p.m21 = a.m20*b.m01 + a.m21*b.m11 + a.m22*b.m21 + a.m23*b.m31;
	p.m22 = a.m20*b.m02 + a.m21*b.m12 + a.m22*b.m22 + a.m23*b.m32;
	p.m23 = a.m20*b.m03 + a.m21*b.m13 + a.m22*b.m23 + a.m23*b.m33;
	// fourth row
	p.m30 = a.m30*b.m00 + a.m31*b.m10 + a.m32*b.m20 + a.m33*b.m30;
	p.m31 = a.m30*b.m01 + a.m31*b.m11 + a.m32*b.m21 + a.m33*b.m31;
	p.m32 = a.m30*b.m02 + a.m31*b.m12 + a.m32*b.m22 + a.m33*b.m32;
	p.m33 = a.m30*b.m03 + a.m31*b.m13 + a.m32*b.m23 + a.m33*b.m33;	
	return p;
}

vec4 mat4_vec4(mat4 m, vec4 v) {
	vec4 prod;
	prod.x = m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03 * v.w;
	prod.y = m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13 * v.w;
	prod.z = m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23 * v.w;
	prod.w = m.m30 * v.x + m.m31 * v.y + m.m32 * v.z + m.m33 * v.w;
	return prod;
}

// calculate a symmetrical-frustum projection matrix
mat4 perspective(float fovy, float aspect, float near, float far) {
	fovy = fovy * 0.0174533;			// from degrees to radians
	mat4 m = identity();
	float f = 1.0/tan(fovy/2.0);
	m.m00 = f/aspect;
	m.m11 = f;
	m.m22 = -((far+near)/(far-near));
	m.m23 = -((2*near*far)/(far-near));
	m.m32 = -1;
	m.m33 = 0;
	return m;
}

float __dot_vec3(vec3 a, vec3 b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

float __mag_vec3(vec3 v) {
	return sqrt((v.x*v.x) + (v.y*v.y) + (v.z*v.z));
}

vec3 __normalize_vec3(vec3 v) {
	float length = sqrt((v.x*v.x) + (v.y*v.y) + (v.z*v.z));
	vec3 norm;
	norm.x = v.x/length;
	norm.y = v.y/length;
	norm.z = v.z/length;
	return norm;
}

vec4 __normalize_vec4(vec4 v) {
	float length = sqrt((v.x*v.x) + (v.y*v.y) + (v.z*v.z) + (v.w*v.w));
	vec4 norm;
	norm.x = v.x/length;
	norm.y = v.y/length;
	norm.z = v.z/length;
	norm.w = v.w/length;
	return norm;
}

vec3 __cross_vec3(vec3 a, vec3 b) {
	vec3 cross;
	cross.x = a.y*b.z - a.z*b.y;
	cross.y = a.z*b.x - a.x*b.z;
	cross.z = a.x*b.y - a.y*b.x;
	return cross;
}
	
// calculate difference of two vectors
vec3 __sub_vec3(vec3 a, vec3 b) {
	vec3 diff;
	diff.x = a.x - b.x;
	diff.y = a.y - b.y;
	diff.z = a.z - b.z;
	return diff;
}

vec3 __add_vec3(vec3 a, vec3 b) {
	vec3 sum;
	sum.x = a.x + b.x;
	sum.y = a.y + b.y;
	sum.z = a.z + b.z;
	return sum;
}

vec3 __scale_vec3(vec3 v, float s) {
	vec3 scaled = {v.x*s,v.y*s,v.z*s};
	return scaled;
}

vec4 __mult_vec4(vec4 a, vec4 b) {
	vec4 v = { a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w };
	return v;
}

vec4 __mult_quat(vec4 a, vec4 b) {
	vec4 prod = {
		 a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x,
		-a.x * b.z + a.y * b.w + a.z * b.x + a.w * b.y,
		 a.x * b.y - a.y * b.x + a.z * b.w + a.w * b.z,
		-a.x * b.x - a.y * b.y - a.z * b.z + a.w * b.w
	};
	return prod;
}

// calculate a LookAt matrix
mat4 look_at(vec3 eye, vec3 center, vec3 up) {
	vec3 f;
	f.x = center.x - eye.x;
	f.y = center.y - eye.y;
	f.z = center.z - eye.z;
	f = __normalize_vec3(f);
	
	vec3 u = __normalize_vec3(up);
	vec3 s = __cross_vec3(u,f);
	u = __cross_vec3(f,s);
	
	mat4 mat = identity();
	mat.m00 = s.x;	mat.m01 = s.y;	mat.m02 = s.z;
	mat.m10 = u.x;	mat.m11 = u.y;	mat.m12 = u.z;
	mat.m20 = f.x;	mat.m21 = f.y;	mat.m22 = f.z;
	mat.m03 = -__dot_vec3(s, eye);
	mat.m13 = -__dot_vec3(u, eye);
	mat.m23 = -__dot_vec3(f, eye);
	return mat;
}

// calculate a translation matrix
mat4 translate(vec3 translation) {
	mat4 mtranslation = identity();
	mtranslation.m03 = translation.x;
	mtranslation.m13 = translation.y;
	mtranslation.m23 = translation.z;
	return mtranslation;
}

// calculate a rotation matrix (rotation in degrees)
mat4 rotate(float angle, vec3 axis) {
	angle = fmod(angle,360.0);
	angle = angle * 0.0174533;			// from degrees to radians

	float x = axis.x;
	float y = axis.y;
	float z = axis.z;
	
	float c = cos(angle);
	float s = sin(angle);
	
	float one_sub_c = 1.0 - c;
	float zs = z*s;
	float ys = y*s;
	float xs = x*s;
	float xz = x*z;
	float yz = y*z;

	mat4 mrot = identity();
	mrot.m00 = x*x*(one_sub_c)+c;
	mrot.m01 = x*y*(one_sub_c)-zs;
	mrot.m02 = xz *(one_sub_c)+ys;
	mrot.m10 = y*x*(one_sub_c)+zs;
	mrot.m11 = y*y*(one_sub_c)+c;
	mrot.m12 = yz *(one_sub_c)-xs;
	mrot.m20 = xz *(one_sub_c)-ys;
	mrot.m21 = yz *(one_sub_c)+xs;
	mrot.m22 = z*z*(one_sub_c)+c;
	return mrot;
}

// calculate a scale matrix
mat4 scale(vec3 s) {
	mat4 mscale = identity();
	mscale.m00 = s.x;
	mscale.m11 = s.y;
	mscale.m22 = s.z;
	return mscale;
}

// convert Euler angles (given in degrees) to quaternion
vec4 euler_to_quat(vec3 angles) {
	float c1, c2, c3;
	float s1, s2, s3;

	if(angles.x >= 0) angles.x = fmod(angles.x, 360.0);
	else angles.x = 360 - fmod(-angles.x, 360);
	if(angles.y >= 0) angles.y = fmod(angles.y, 360.0);
	else angles.y = 360 - fmod(-angles.y, 360);
	if(angles.z >= 0) angles.z = fmod(angles.z, 360.0);
	else angles.z = 360 - fmod(-angles.z, 360);
	
	angles.x = angles.x * 0.0174533;
	angles.y = angles.y * 0.0174533;
	angles.z = angles.z * 0.0174533;
	
	c1 = cos(angles.y / 2.0);
	c2 = cos(angles.z / 2.0);
	c3 = cos(angles.x / 2.0);
	s1 = sin(angles.y / 2.0);
	s2 = sin(angles.z / 2.0);
	s3 = sin(angles.x / 2.0);
	
	vec4 quat;
	quat.w = c1*c2*c3 - s1*s2*s3;
	quat.x = s1*s2*c3 + c1*c2*s3;
	quat.y = s1*c2*c3 + c1*s2*s3;
	quat.z = c1*s2*c3 - s1*c2*s3;
	
	float n = sqrt(pow(quat.x, 2) + pow(quat.y, 2) + pow(quat.z, 2) + pow(quat.w, 2));
	float inv = 1.0/n;
	quat.x = quat.x * inv;
	quat.y = quat.y * inv;
	quat.z = quat.z * inv;
	quat.w = quat.w * inv;
	return quat;
}

// create quaternion representing rotation around some arbitrary axis
vec4 quat_axis_rotation(vec3 axis, float angle) {
	angle *= 0.0174533;
	float fac = sinf(angle/2.0f);
	vec4 quat;	// calc x,y,z of quaternion
	quat.x = axis.x * fac;
	quat.y = axis.y * fac;
	quat.z = axis.z * fac;
	quat.w = cosf(angle/2.0f);	// calc w value
	return __normalize_vec4(quat);
}

vec3 quat_to_euler(vec4 quat) {
	float sqw = quat.w*quat.w;
	float sqx = quat.x*quat.x;
	float sqy = quat.y*quat.y;
	float sqz = quat.z*quat.z;
	float unit = sqx + sqy + sqz + sqw;	// if normalised is one, otherwise is correction factor
	float test = quat.x*quat.y + quat.z*quat.w;
	vec3 euler;
	if(test > 0.499*unit) { // singularity at north pole
		euler.y = 2 * atan2(quat.x,quat.w);
		euler.z = 3.14159265358979323846/2;
		euler.x = 0;
		euler.y *= 57.2958; euler.z *= 57.2958;
		return euler;
	}
	if(test < -0.499*unit) { // singularity at south pole
		euler.y = -2 * atan2(quat.x,quat.w);
		euler.z = -3.14159265358979323846/2;
		euler.x = 0;
		euler.y *= 57.2958; euler.z *= 57.2958;
		return euler;
	}
	euler.y = atan2(2*quat.y*quat.w-2*quat.x*quat.z , sqx - sqy - sqz + sqw);
	euler.z = asin(2*test/unit);
	euler.x = atan2(2*quat.x*quat.w-2*quat.y*quat.z , -sqx + sqy - sqz + sqw);
	euler.x *= 57.2958; euler.y *= 57.2958; euler.z *= 57.2958;
	return euler;
}

// rotate a quaternion
vec4 rotate_quat(vec4 quat, vec3 angles) {
	vec4 v = euler_to_quat(angles);
	return __mult_quat(v,quat);
}

mat4 quat_to_mat4(vec4 quat) {
	float xx = quat.x * quat.x;
	float xy = quat.x * quat.y;
	float xz = quat.x * quat.z;
	float xw = quat.x * quat.w;
	float yy = quat.y * quat.y;
	float yz = quat.y * quat.z;
	float yw = quat.y * quat.w;
	float zz = quat.z * quat.z;
	float zw = quat.z * quat.w;
	
	mat4 rotation = identity();
	rotation.m00 = 1.0 - 2.0*yy - 2.0*zz;
	rotation.m01 = 2.0 * xy - 2.0 * zw;
	rotation.m02 = 2.0 * xz + 2.0 * yw;
	rotation.m10 = 2.0 * xy + 2.0 * zw;
	rotation.m11 = 1.0 - 2.0*xx - 2.0*zz;
	rotation.m12 = 2.0 * yz - 2.0 * xw;
	rotation.m20 = 2.0 * xz - 2.0 * yw;
	rotation.m21 = 2.0 * yz + 2.0 * xw;
	rotation.m22 = 1.0 - 2.0*xx - 2.0*yy;
	return rotation;
}

// return coords of the center point of this camera
vec3 camera_center(camera_t cam) {
	vec4 c4 = { 0,0,1,1 };
	mat4 rot_matrix = quat_to_mat4(cam.quat);
	c4 = mat4_vec4(rot_matrix, c4);
	vec3 p;
	p.x = cam.pos.x + c4.x;
	p.y = cam.pos.y + c4.y;
	p.z = cam.pos.z + c4.z;
	return p;
}

// move a camera forward (or backward)
void forward(camera_t* cam, float units) {
	if(units) {
		vec3 c3 = camera_center(*cam);
		c3.x = cam->pos.x - c3.x;
		c3.y = cam->pos.y - c3.y;
		c3.z = cam->pos.z - c3.z;
		cam->pos.x = cam->pos.x + c3.x * units;
		cam->pos.y = cam->pos.y + c3.y * units;
		cam->pos.z = cam->pos.z + c3.z * units;
	}
}

// move a camera leftward or rightward
void right(camera_t* cam, float units) {
	if(units) {
		vec4 c4; 				c4.x=1; c4.y=0; c4.z=0; c4.w=1;		// default: camera looking down -z axis
		mat4 rot_matrix = quat_to_mat4(cam->quat);
		c4 = mat4_vec4(rot_matrix, c4);
		cam->pos.x = cam->pos.x + c4.x * units;
		cam->pos.y = cam->pos.y + c4.y * units;
		cam->pos.z = cam->pos.z + c4.z * units;
	}
}

/*==================================================*/
/*				PLAYER CODE							*/
/*==================================================*/

typedef struct player_t player_t;

player_t init_player(char* name) {
	player_t new_player;
	memset(&new_player,0,sizeof(player_t));
	new_player.name = calloc(1,strlen(name)+1);
	strcpy(new_player.name,name);
	vec3 pos = {0,0,0}, rot = { 0,0,0 };
	vec4 quat = euler_to_quat(rot);
	vec4 p_colors[] = {
		{0,0,1,1},
		{1,1,0,1},
		{1,1,0,1},
		{0,1,0,1},
		{0,1,0,1},
		{1,1,0,1}
	};
	new_player.entity_id = add_humanoid_entity(pos,quat,1,p_colors);
	new_player.camera.quat = euler_to_quat(rot);
	new_player.camera.zoom = 10;
	new_player.selected_brick_id = -1;
	return new_player;
}

// sets player position and updates player's AABB
void set_player_pos(vec3 new_pos) {
	entity_t* entity = &entities[player->entity_id];
	entity->pos = new_pos;
	vec3 half_scale = __scale_vec3(world->colls[entity->coll_id].dim, 0.5);
	vec3 aabb_pos = __sub_vec3(new_pos, half_scale);
	aabb_pos.y -= 1;
	world->colls[entity->coll_id].pos = aabb_pos;
}

void translate_player(vec3 translation) {
	entity_t* entity = &entities[player->entity_id];
	entity->pos = __add_vec3(entity->pos, translation);
	world->colls[entity->coll_id].pos = __add_vec3(world->colls[entity->coll_id].pos, translation);
	if(check_collision_aabb(entity->coll_id)) {
		// check if small increase in Y would help before resetting (allows stair climbing)
		vec3 step = { 0,1.25,0 };
		entity->pos = __add_vec3(entity->pos, step);
		world->colls[entity->coll_id].pos = __add_vec3(world->colls[entity->coll_id].pos, step);
		if(!check_collision_aabb(entity->coll_id)) return;

		entity->pos = __sub_vec3(entity->pos, translation);
		entity->pos = __sub_vec3(entity->pos, step);
		world->colls[entity->coll_id].pos = __sub_vec3(world->colls[entity->coll_id].pos, translation);
		world->colls[entity->coll_id].pos = __sub_vec3(world->colls[entity->coll_id].pos, step);
	}
}


/*==================================================*/
/*				RENDERING							*/
/*==================================================*/
// render as the global 'player' variable
// a lot of the work was done in init_gl
// shader and program setting, and mesh generation (mesh 0 is our brick)

// these are the main programs:
// program_ids[0] - basic. reads only vec3 pos attribute; solid color (vtx_format >= 0).
// program_ids[1] - reads vec3 pos and vec3 norm attributes (vtx_format >= 1).

GLuint* program_ids;
uint32_t n_programs;

// create a new GL program
GLuint create_program(const char* vtx_shader_src, const char* pxl_shader_src) {
	GLuint vtx_shader = glCreateShader(GL_VERTEX_SHADER);
	GLuint pxl_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vtx_shader,1,&vtx_shader_src,0);
	glShaderSource(pxl_shader,1,&pxl_shader_src,0);
	glCompileShader(vtx_shader);
	GLint success = 0;
	glGetShaderiv(vtx_shader,GL_COMPILE_STATUS,&success);
	if(!success) {
		printf("failed to compile vertex shader.\n");
		GLint max_length = 0;
		glGetShaderiv(vtx_shader, GL_INFO_LOG_LENGTH, &max_length);
		char* info_log = calloc(1,max_length);
		glGetShaderInfoLog(vtx_shader, max_length, &max_length, &info_log[0]);
		printf("%s\n", info_log);
		exit(1);
	}
	glCompileShader(pxl_shader);
	glGetShaderiv(pxl_shader,GL_COMPILE_STATUS,&success);
	if(!success) {
		printf("failed to compile pixel shader.\n");
		GLint max_length = 0;
		glGetShaderiv(pxl_shader, GL_INFO_LOG_LENGTH, &max_length);
		char* info_log = calloc(1,max_length);
		glGetShaderInfoLog(pxl_shader, max_length, &max_length, &info_log[0]);
		printf("%s\n", info_log);
		exit(1);
	}
	program_ids = realloc(program_ids, sizeof(GLuint));
	program_ids[n_programs] = glCreateProgram();
	glAttachShader(program_ids[n_programs],vtx_shader);
	glAttachShader(program_ids[n_programs],pxl_shader);
	glLinkProgram(program_ids[n_programs]);
	glGetProgramiv(program_ids[n_programs],GL_LINK_STATUS,&success);
	if(!success) {
		printf("failed to link shaders.\n");
		exit(1);
	}
	glDetachShader(program_ids[n_programs], vtx_shader);
	glDetachShader(program_ids[n_programs], pxl_shader);
	n_programs++;
}

void init_render() {	// setup and set shader program, GL states
	// setup depth test, blending
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const char* vtx_shader_src_1 =
	"#version 330										\n"
	"layout(location=0) in vec3 vtx_pos;				\n"
	"uniform mat4 u_model, u_view, u_proj;				\n"
	"void main() {										\n"	
	"	gl_Position = u_proj * u_view * u_model * vec4(vtx_pos,1);	\n"
	"}													";

	const char* pxl_shader_src_1 =
	"#version 330										\n"
	"layout(location=0) out vec4 final;					\n"
	"uniform vec4 u_color;								\n"
	"void main() {										\n"
	"	final = u_color;\n"
	"}													";

	create_program(vtx_shader_src_1, pxl_shader_src_1);

	const char* vtx_shader_src_2 =
	"#version 330										\n"
	"layout(location=0) in vec3 vtx_pos;				\n"
	"layout(location=1) in vec3 vtx_norm;				\n"
	"out vec3 pxl_norm;									\n"
	"out vec3 pxl_pos;									\n"
	"uniform mat4 u_model, u_view, u_proj;				\n"
	"void main() {										\n"	
	"	pxl_norm = mat3(transpose(inverse(u_model))) * vtx_norm;\n"
	"	pxl_pos = vec3(u_model * vec4(vtx_pos,1.0));	\n"
	"	gl_Position = u_proj * u_view * u_model * vec4(vtx_pos,1);\n"
	"}													";

	const char* pxl_shader_src_2 =
	"#version 330										\n"
	"layout(location=0) out vec4 final;					\n"
	"in vec3 pxl_norm;									\n"
	"in vec3 pxl_pos;									\n"
	"uniform vec4 u_color;								\n"
	"void main() {										\n"
	"	vec3 light_pos = vec3(75,50,50);				\n"
	"	vec3 light_col = vec3(.6,.6,.6);				\n"
	"	vec3 norm = normalize(pxl_norm);				\n"	
	//"	vec3 light_dir = normalize(light_pos - pxl_pos);\n"			// from light position to object (point light)
	"	vec3 light_dir = normalize(-vec3(-0.2f, -1.0f, -1.5f));\n"	// directional light
	"	float diff = max(dot(norm, light_dir), 0.0);	\n"
	"	vec3 diffuse = diff * light_col;				\n"
	"	vec3 ambient = vec3(.6,.6,.6);					\n"
	"	final = vec4(ambient+diffuse,1) * u_color;\n"
	"}													";

	create_program(vtx_shader_src_2, pxl_shader_src_2);

	const char* vtx_shader_src_3 =
	"#version 330										\n"
	"layout(location=0) in vec3 vtx_pos;				\n"
	"layout(location=1) in vec3 vtx_norm;				\n"
	"layout(location=2) in vec2 vtx_tex;				\n"
	"out vec3 pxl_norm;									\n"
	"out vec3 pxl_pos;									\n"
	"out vec2 pxl_tex;									\n"
	"flat out uint face_id;								\n"
	"uniform float u_textured_faces[6];					\n"
	"uniform mat4 u_model, u_view, u_proj;				\n"
	"void main() {										\n"	
	"	pxl_norm = mat3(transpose(inverse(u_model))) * vtx_norm;\n"
	"	pxl_pos = vec3(u_model * vec4(vtx_pos,1.0));	\n"
	"	pxl_tex = vtx_tex;								\n"
	"	face_id = uint(gl_VertexID/4);					\n"
	"	if((face_id == 1u || face_id == 3u) && u_textured_faces[face_id] == 1.0) {			\n"	// top/bottom face - scale tex coords by x,z
	"		pxl_tex.x *= u_model[2][2];					\n"
	"		pxl_tex.y *= u_model[0][0];					\n"
	"	}												\n"
	"	if((face_id == 4u || face_id == 5u) && u_textured_faces[face_id] == 1.0) {			\n"	// left/right face - scale tex coords by y,z
	"		pxl_tex.x *= u_model[1][1];					\n"
	"		pxl_tex.y *= u_model[2][2];					\n"
	"	}												\n"
	"	if(face_id == 0u) {								\n" // front face - scale tex coords by x,y
	"		pxl_tex.x *= u_model[1][1];					\n"
	"		pxl_tex.y *= u_model[0][0];					\n"
	"	}												\n"
	"	if(face_id == 2u) {								\n" // front face - scale tex coords by x,y
	"		pxl_tex.x *= u_model[0][0];					\n"
	"		pxl_tex.y *= u_model[1][1];					\n"
	"	}												\n"
	"	gl_Position = u_proj * u_view * u_model * vec4(vtx_pos,1);\n"
	"}													";

	const char* pxl_shader_src_3 =
	"#version 330										\n"
	"layout(location=0) out vec4 final;					\n"
	"in vec3 pxl_norm;									\n"
	"in vec3 pxl_pos;									\n"
	"in vec2 pxl_tex;									\n"
	"flat in uint face_id;								\n"
	"uniform vec4 u_color;								\n"
	"uniform float u_textured_faces[6];					\n"
	"uniform sampler2D u_samplers[6];					\n"
	"void main() {										\n"
	"	vec3 light_col = vec3(.6,.6,.6);				\n"
	"	vec3 norm = normalize(pxl_norm);				\n"	
	"	vec3 light_dir = normalize(-vec3(-0.2f, -1.0f, -1.5f));\n"	// directional light
	"	float diff = max(dot(norm, light_dir), 0.0);	\n"
	"	vec3 diffuse = diff * light_col;				\n"
	"	vec3 ambient = vec3(.6,.6,.6);					\n"
	"	final = vec4(ambient+diffuse,1) * u_color;		\n"
	"	if(u_textured_faces[face_id]>0.0) { 			\n"
	"		vec4 sample;								\n"
	"		if(face_id == 0.) sample = texture(u_samplers[0],pxl_tex);\n"
	"		if(face_id == 1.) sample = texture(u_samplers[1],pxl_tex);\n"
	"		if(face_id == 2.) sample = texture(u_samplers[2],pxl_tex);\n"
	"		if(face_id == 3.) sample = texture(u_samplers[3],pxl_tex);\n"
	"		if(face_id == 4.) sample = texture(u_samplers[4],pxl_tex);\n"
	"		if(face_id == 5.) sample = texture(u_samplers[5],pxl_tex);\n"
	"		final = sample + (final*(1.0-sample.w));	\n"
	"	}												\n"
	"}													";

	create_program(vtx_shader_src_3, pxl_shader_src_3);
}

void render(uint8_t render_entities) {
	// get all needed uniform IDs from shader program
	GLuint program_id = program_ids[2];
	glUseProgram(program_id);
	GLint model_loc = glGetUniformLocation(program_id,"u_model");
	GLint view_loc = glGetUniformLocation(program_id,"u_view");
	GLint proj_loc = glGetUniformLocation(program_id,"u_proj");
	GLint color_loc = glGetUniformLocation(program_id,"u_color");
	GLint faces_loc = glGetUniformLocation(program_id,"u_textured_faces");
	GLint samplers_loc = glGetUniformLocation(program_id,"u_samplers");

	mat4 persp = perspective(fovy, window_width/window_height, near, far);
	float mat_data[] = {
		persp.m00, persp.m10, persp.m20, persp.m30,
		persp.m01, persp.m11, persp.m21, persp.m31,
		persp.m02, persp.m12, persp.m22, persp.m32,
		persp.m03, persp.m13, persp.m23, persp.m33
	};
	glUniformMatrix4fv(proj_loc, 1, GL_FALSE, &mat_data[0]);

	vec3 center = camera_center(player->camera);
	vec3 up = { 0,1,0 };

	mat4 view;
	if(!player->focused)
		view = look_at(player->camera.pos, center, up);
	else {
		// update player camera
		// eye vector should be 10 studs away from player position, in reverse of whichever direction the camera is facing.
		player->camera.pos = entities[player->entity_id].pos;

		// circle point around the player (XZ plane)
		vec4 c4 = { 0,0,player->camera.zoom,1 };
		mat4 rot_matrix = quat_to_mat4(player->camera.quat);
		c4 = mat4_vec4(rot_matrix, c4);
		vec3 c3 = { c4.x,c4.y,c4.z };
		player->camera.pos = __add_vec3(player->camera.pos, c3);
		player->camera.pos.y += 3;

		center = camera_center(player->camera);
		view = look_at(player->camera.pos, center, up);
	}

	float view_data[] = {
		view.m00, view.m10, view.m20, view.m30,
		view.m01, view.m11, view.m21, view.m31,
		view.m02, view.m12, view.m22, view.m32,
		view.m03, view.m13, view.m23, view.m33
	};
	glUniformMatrix4fv(view_loc, 1, GL_FALSE, &view_data[0]);

	// render all entities.
	if(render_entities)
	for(uint32_t i = 0; i < n_entities; i++) {
		if(!entities[i].is_humanoid) continue;
		entity_t* entity = &entities[i];
		GLfloat faces[] = { 0,0,0,0,0,0 };
		glUniform1fv(faces_loc, 6, faces);

		mesh_t mesh = meshes[0];
		vec3 p_pos[] = {
			{0,   0,0},		// torso
			{1.5, 0,0},		// left arm
			{-1.5,0,0},		// right arm
			{-.5, -1,0},	// left leg
			{.5,  -1,0},	// right leg
			{0,   1.5,0}	// head
		};
		vec3 p_scale[] = {
			{2,2,1},
			{1,2,1},
			{1,2,1},
			{1,2,1},
			{1,2,1},
			{2,1,1}
		};

		if(entity->jump_state == 1 && entity->fall_distance > 6) {	// falling or jumping
			p_pos[1].y = .5;
			p_pos[2].y = .5;
		}

		for(uint32_t j = 0; j < 6; j++) {
			// calculate model matrix
			mat4 smat = scale(p_scale[j]);
			mat4 rmat = quat_to_mat4(entity->quat);
			// translate parts to their correct position in object space,
			// then scale, then rotate, then translate to where the player is in the world.
			mat4 tmat = translate(p_pos[j]);
			mat4 model = mat4_mat4(rmat,smat);	// scale, then rotate
			model = mat4_mat4(model,tmat);	// translate parts to where they should be
			tmat = translate(entity->pos);
			model = mat4_mat4(tmat,model);		// apply translation
			float mat_data[] = {
				model.m00, model.m10, model.m20, model.m30,
				model.m01, model.m11, model.m21, model.m31,
				model.m02, model.m12, model.m22, model.m32,
				model.m03, model.m13, model.m23, model.m33
			};

			// update uniforms
			glUniform4f(color_loc, entity->part_colors[j].x, entity->part_colors[j].y, entity->part_colors[j].z, entity->part_colors[j].w);
			glUniformMatrix4fv(model_loc, 1, GL_FALSE, &mat_data[0]);

			// submit draw call
			glBindVertexArray(mesh.vao_id);
			glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo_id);
			if(mesh.has_ibo) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ibo_id);
				glDrawElements(GL_TRIANGLES,mesh.n_indices,GL_UNSIGNED_SHORT,0);
			} else
				glDrawArrays(GL_TRIANGLES,0,mesh.n_indices);
		}
	}

	// render all bricks.
	for(uint32_t i = 0; i < world->n_bricks; i++) {
		if(world->bricks[i].deleted) continue;
		brick_t brick = world->bricks[i];
		mesh_t mesh = meshes[brick.mesh_id];

		GLfloat faces[6];
		for(uint32_t f = 0; f < 6; f++) {
			if(brick.texture_ids[f]) {
				if(brick.repeat_textures[f]) faces[f] = 1;
				else faces[f] = 2;
			} else faces[f] = 0;
		}
		glUniform1fv(faces_loc, 6, faces);
		for(uint32_t f = 0; f < 6; f++) {
			glActiveTexture(GL_TEXTURE0+f);
			glBindTexture(GL_TEXTURE_2D,brick.texture_ids[f]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		GLint units[] = { 0,1,2,3,4,5 };
		glUniform1iv(samplers_loc, 6, units);

		// calculate model matrix
		mat4 smat = scale(brick.scale);
		mat4 rmat = quat_to_mat4(brick.quat);
		mat4 tmat = translate(brick.pos);
		mat4 model = mat4_mat4(rmat,smat);	// scale, then rotate
		model = mat4_mat4(tmat,model);		// apply translation
		float mat_data[] = {
			model.m00, model.m10, model.m20, model.m30,
			model.m01, model.m11, model.m21, model.m31,
			model.m02, model.m12, model.m22, model.m32,
			model.m03, model.m13, model.m23, model.m33
		};

		// update uniforms
		glUniform4f(color_loc, brick.color.x, brick.color.y, brick.color.z, brick.color.w);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, &mat_data[0]);

		// submit draw call
		glBindVertexArray(mesh.vao_id);
		glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo_id);
		if(mesh.has_ibo) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ibo_id);
			glDrawElements(GL_TRIANGLES,mesh.n_indices,GL_UNSIGNED_SHORT,0);
		} else
			glDrawArrays(GL_TRIANGLES,0,mesh.n_indices);
	}
}

void render_physics() {
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	// get all needed uniform IDs from shader program
	glUseProgram(program_ids[0]);
	GLint model_loc = glGetUniformLocation(program_ids[0],"u_model");
	GLint view_loc = glGetUniformLocation(program_ids[0],"u_view");
	GLint proj_loc = glGetUniformLocation(program_ids[0],"u_proj");
	GLint color_loc = glGetUniformLocation(program_ids[0],"u_color");

	mat4 persp = perspective(fovy, window_width/window_height, near, far);
	float mat_data[] = {
		persp.m00, persp.m10, persp.m20, persp.m30,
		persp.m01, persp.m11, persp.m21, persp.m31,
		persp.m02, persp.m12, persp.m22, persp.m32,
		persp.m03, persp.m13, persp.m23, persp.m33
	};
	glUniformMatrix4fv(proj_loc, 1, GL_FALSE, &mat_data[0]);

	vec3 center = camera_center(player->camera);
	vec3 up = { 0,1,0 };
	mat4 view = look_at(player->camera.pos, center, up);
	float view_data[] = {
		view.m00, view.m10, view.m20, view.m30,
		view.m01, view.m11, view.m21, view.m31,
		view.m02, view.m12, view.m22, view.m32,
		view.m03, view.m13, view.m23, view.m33
	};
	glUniformMatrix4fv(view_loc, 1, GL_FALSE, &view_data[0]);

	// render all colliders
	for(uint32_t i = 0; i < world->n_colls; i++) {
		if(world->colls[i].deleted) continue;
		collision_t coll = world->colls[i];
		mesh_t mesh = meshes[0];

		// calculate model matrix
		mat4 smat = scale(coll.dim);
		mat4 tmat = translate(__add_vec3(coll.pos,__scale_vec3(coll.dim,0.5)));		// how much to translate (re-set to origin)
		mat4 model = mat4_mat4(tmat,smat);	// scale, then translate
		float mat_data[] = {
			model.m00, model.m10, model.m20, model.m30,
			model.m01, model.m11, model.m21, model.m31,
			model.m02, model.m12, model.m22, model.m32,
			model.m03, model.m13, model.m23, model.m33
		};

		// update uniforms
		glUniform4f(color_loc, 1,1,1,1);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, &mat_data[0]);

		// submit draw call
		glBindVertexArray(mesh.vao_id);
		glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo_id);
		if(mesh.has_ibo) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ibo_id);
			glDrawElements(GL_TRIANGLES,mesh.n_indices,GL_UNSIGNED_SHORT,0);
		} else
			glDrawArrays(GL_TRIANGLES,0,mesh.n_indices);
	}
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}


/*==================================================*/
/*				INPUT HANDLING						*/
/*==================================================*/
// affects the camera in global 'player' variable

uint8_t kbd[500];
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	switch(key) {		// respond to keypress
		case GLFW_KEY_W: key = 0; break;
		case GLFW_KEY_S: key = 1; break;
		case GLFW_KEY_D: key = 2; break;
		case GLFW_KEY_A: key = 3; break;
		case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT:
			key = 4; break;
		case GLFW_KEY_SPACE: key = 5; break;
		case GLFW_KEY_LEFT: key = 6; break;
		case GLFW_KEY_RIGHT: key = 7; break;
		case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL:
			key = 8; break;
		case GLFW_KEY_V: key = 9; break;
		case GLFW_KEY_R: key = 10; break;
		default: return;
	}
	if(action == GLFW_PRESS) {
		if(key == 8) enable_physics_draw = !enable_physics_draw;
		if(key == 9) player->focused = !player->focused;
		if(key == 10) { vec3 p = {0,0,0}; set_player_pos(p); }
		kbd[key] = 1;
	} else if(action == GLFW_RELEASE) kbd[key] = 0;
}

uint8_t mouse_buttons[3];
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	switch(button) {
		case GLFW_MOUSE_BUTTON_LEFT: button = 0; break;
		case GLFW_MOUSE_BUTTON_RIGHT: button = 1; break;
		case GLFW_MOUSE_BUTTON_MIDDLE: button = 2; break;
		default: return;
	}
	if(action == GLFW_PRESS) {
		mouse_buttons[button] = 1;

		if(!player->focused && mouse_buttons[0]) {	// left click - double click to delete a brick
			vec4 d4 = { 0,0,-1,1 };
			mat4 rot_matrix = quat_to_mat4(player->camera.quat);
			d4 = mat4_vec4(rot_matrix, d4);
			vec3 dir = { d4.x, d4.y, d4.z };
			
			intersection_t* ints = 0;
			uint32_t n_ints = check_ray_intersection(player->camera.pos, dir, &ints, 1);

			if(n_ints == 1) {
				int32_t brick_id = world->colls[ints[0].coll_id].brick_id;
				if(brick_id != -1) {
					vec4 c = { 1,1,1,1 };
					if(brick_id == player->selected_brick_id)
						delete_brick(brick_id);
					else player->selected_brick_id = brick_id;
				}
			} else player->selected_brick_id = -1;
		}

		if(!player->focused && mouse_buttons[1]) {	// right click - add a brick 10 studs in front of camera
			vec4 d4 = { 0,0,-1,1 };
			mat4 rot_matrix = quat_to_mat4(player->camera.quat);
			d4 = mat4_vec4(rot_matrix, d4);
			vec3 dir = { d4.x, d4.y, d4.z };
			vec3 pos = player->camera.pos;
			pos = __add_vec3(pos,__scale_vec3(dir,5));	// calculate position of new brick
			pos.x = round(pos.x);
			pos.y = round(pos.y);
			pos.z = round(pos.z);
			vec3 rot = { 0,0,0 };
			vec4 quat = euler_to_quat(rot);
			vec3 scale = { 1,1,1 };
			vec4 color = { 0.5,0.5,0.5,1 };
			add_brick(world, pos, quat, scale, color, 0, 0,1);
		}
	} else if(action == GLFW_RELEASE) mouse_buttons[button] = 0;
}

void process_input() {
	if(scroll_y != prev_scroll_y && player->focused)
		player->camera.zoom = fminf(fmaxf(player->camera.zoom+(prev_scroll_y-scroll_y),5),50);
	if(cursor_x != prev_x) {
		vec3 rot = {0,0,0};
		rot.y = (prev_x-cursor_x)*.05;
		player->camera.quat = rotate_quat(player->camera.quat,rot);
	}
	
	if(cursor_y != prev_y) {
		vec4 p = {1,0,0,1};
		mat4 rot_matrix = quat_to_mat4(player->camera.quat);
		p = mat4_vec4(rot_matrix, p);
		vec3 horiz = { p.x,p.y,p.z };
		vec4 axis = quat_axis_rotation(horiz,(prev_y-cursor_y)*.05);
		vec3 euler = quat_to_euler(player->camera.quat);
		if(euler.x+(prev_y-cursor_y)*.05 < 50 && euler.x+(prev_y-cursor_y)*.05 > -50)
			player->camera.quat = __mult_quat(axis,player->camera.quat); 
	}
	prev_x = cursor_x, prev_y = cursor_y;
	prev_scroll_y = scroll_y;
	vec3 rot = {0,0,0};
	if(!player->focused) {
		if(kbd[0]) forward(&player->camera,.25);
		if(kbd[1]) forward(&player->camera,-.25);
		if(kbd[2]) right(&player->camera,.25);
		if(kbd[3]) right(&player->camera,-.25);
		if(kbd[4]) player->camera.pos.y -= .25;
		if(kbd[5]) player->camera.pos.y += .25;
		if(kbd[6]) rot.y = 1; player->camera.quat = rotate_quat(player->camera.quat,rot);
		if(kbd[7]) rot.y = -1; player->camera.quat = rotate_quat(player->camera.quat,rot);
	} else {
		vec3 v = {0,0,0};
		if(kbd[0] || kbd[1] || kbd[2] || kbd[3]) {
			vec4 c4 = { 0,0,1,1 };
			mat4 rot_matrix = quat_to_mat4(player->camera.quat);
			c4 = mat4_vec4(rot_matrix, c4);
			vec3 fwd = { c4.x,0,c4.z };

			c4.x=1, c4.y=0, c4.z=0, c4.w=1;
			rot_matrix = quat_to_mat4(player->camera.quat);
			c4 = mat4_vec4(rot_matrix, c4);
			vec3 side = { c4.x,0,c4.z };

			float fwd_scale = kbd[1]-kbd[0];
			float side_scale = kbd[2]-kbd[3];

			if(fwd_scale || side_scale) {
				fwd = __scale_vec3(fwd, fwd_scale);
				side = __scale_vec3(side, side_scale);
				vec3 dir = __normalize_vec3(__add_vec3(fwd,side));
				vec3 walk = __scale_vec3(dir,.1);
				translate_player(walk);

				float angle = dir.z ? atan(dir.x/dir.z) : 0;
				angle *= 57.2958;
				vec3 player_rot = { 0,angle,0 };
				entities[player->entity_id].quat = euler_to_quat(player_rot);
			}
		}
		if(kbd[5] && !entities[player->entity_id].jump_state) {
			v.y = 4; translate_player(v);
			entities[player->entity_id].jump_state = 1;
		}

		if(kbd[6]) { v.y = 1; player->camera.quat = rotate_quat(player->camera.quat,v); }
		if(kbd[7]) { v.y = -1; player->camera.quat = rotate_quat(player->camera.quat,v); }
	}
	
}

void window_size_callback(GLFWwindow* window, int width, int height) {
	window_width = width;
	window_height = height;
	glViewport(0,0,width,height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	scroll_x += xoffset;
	scroll_y += yoffset;
}

void cursor_pos_callback(GLFWwindow* window, double x_pos, double y_pos) {
	cursor_x += x_pos;
	cursor_y += y_pos;
	glfwSetCursorPos(window, 0, 0);
}



/*==================================================*/
/*				MAIN LOOP AND SETUP					*/
/*==================================================*/

GLFWwindow* window;

void init_gl() {
	if(!player) {
		printf("internal error at init_gl: player not set.\n");
		exit(1);
	}

	char* prefix = "FreeBuild - ";
	char* window_title = calloc(1,strlen(prefix)+strlen(player->name)+1);
	strcpy(window_title,prefix);
	strcpy(window_title+strlen(prefix),player->name);


	if(!glfwInit()) {
		printf("glfwInit() failed. :(\n");
		exit(1);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	window = glfwCreateWindow(window_width, window_height, window_title, NULL, NULL);
	if(!window) {
		printf("glfwCreateWindow() failed to create window. :(\n");
		exit(1);
	}
	glfwMakeContextCurrent(window);
	glfwSetCursorPos(window, 0, 0);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window,cursor_pos_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
}

int main() {
	init_world();
	player_t local_player = init_player("test_player");
	player = &local_player;
	init_gl();
	init_mesh();
	init_render();

	load_texture_from_file("top.png");			// texture 0 - brick top
	load_texture_from_file("bottom.png");		// texture 1 - brick bottom

	vec3 pos = { 0,-10,0 };
	vec3 rot = { 0,0,0 };
	vec4 quat = euler_to_quat(rot);
	vec3 scale = { 20,1,20 };
	vec4 color = { 0,0.6,0,1 };
	add_brick(world, pos, quat, scale, color, 0, 0,1);

	vec3 pos2 = { 0,-11,0 };
	vec3 rot2 = { 0,0,0 };
	vec4 quat2 = euler_to_quat(rot2);
	vec3 scale2 = { 30,1,30 };
	vec4 color2 = { 0,0.2,0,1 };
	add_brick(world, pos2, quat2, scale2, color2, 0, 0,1);

	vec3 pos3 = { 0,-5,0 };
	vec3 rot3 = { 0,0,0 };
	vec4 quat3 = euler_to_quat(rot3);
	vec3 scale3 = { 1,1,1 };
	vec4 color3 = { .4,.4,.8,.5 };
	add_brick(world, pos3, quat3, scale3, color3, 0, 1,1);

	vec3 cam_rot = { -30,0,0 };
	player->camera.quat = euler_to_quat(cam_rot);
	player->camera.pos.z = 10;
	player->focused = 1;

	glfwPollEvents();

	float frame = 0;
	while(!glfwWindowShouldClose(window)) {
		glClearColor(0.0,0.2,0.4,1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		process_input();
		render(1);
		if(enable_physics_draw) render_physics();
		physics_step();

		vec3 move = {0,0,cos(frame*0.05)*0.1};
		translate_brick(2,move);

		glfwSwapBuffers(window);
		
		struct timespec ts;
		ts.tv_sec = 0; ts.tv_nsec = 16000000;
		nanosleep(&ts,&ts);

		glfwPollEvents();
		frame++;
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
