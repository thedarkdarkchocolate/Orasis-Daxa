#pragma once

#include <bit>


#include <daxa/daxa.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

inline daxa_f32mat4x4 to_daxa(glm::mat4 const & m) { return std::bit_cast<daxa_f32mat4x4>(m); }
inline daxa_f32vec4   to_daxa(glm::vec4 const & v) { return {v.x, v.y, v.z, v.w}; }
inline daxa_f32vec3   to_daxa(glm::vec3 const & v) { return {v.x, v.y, v.z}; }
inline daxa_f32vec2   to_daxa(glm::vec2 const & v) { return {v.x, v.y}; }
