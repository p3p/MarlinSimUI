#pragma once

#include <gl.h>
#include <glm/glm.hpp>

namespace renderer {

template<typename T> struct type_to_gl_enum;
template<> struct type_to_gl_enum<float> { static constexpr uint32_t value = GL_FLOAT; };
template<> struct type_to_gl_enum<glm::vec2> { static constexpr uint32_t value = GL_FLOAT_VEC2; };
template<> struct type_to_gl_enum<glm::vec3> { static constexpr uint32_t value = GL_FLOAT_VEC3; };
template<> struct type_to_gl_enum<glm::vec4> { static constexpr uint32_t value = GL_FLOAT_VEC4; };
template<> struct type_to_gl_enum<double> { static constexpr uint32_t value = GL_DOUBLE; };
template<> struct type_to_gl_enum<int32_t> { static constexpr uint32_t value = GL_INT; };
template<> struct type_to_gl_enum<glm::ivec2> { static constexpr uint32_t value = GL_INT_VEC2; };
template<> struct type_to_gl_enum<glm::ivec3> { static constexpr uint32_t value = GL_INT_VEC3; };
template<> struct type_to_gl_enum<glm::ivec4> { static constexpr uint32_t value = GL_INT_VEC4; };
template<> struct type_to_gl_enum<uint32_t> { static constexpr uint32_t value = GL_UNSIGNED_INT; };
template<> struct type_to_gl_enum<glm::uvec2> { static constexpr uint32_t value = GL_UNSIGNED_INT_VEC2; };
template<> struct type_to_gl_enum<glm::uvec3> { static constexpr uint32_t value = GL_UNSIGNED_INT_VEC3; };
template<> struct type_to_gl_enum<glm::uvec4> { static constexpr uint32_t value = GL_UNSIGNED_INT_VEC4; };
template<> struct type_to_gl_enum<bool> { static constexpr uint32_t value = GL_BOOL; };
template<> struct type_to_gl_enum<glm::bvec2> { static constexpr uint32_t value = GL_BOOL_VEC2; };
template<> struct type_to_gl_enum<glm::bvec3> { static constexpr uint32_t value = GL_BOOL_VEC3; };
template<> struct type_to_gl_enum<glm::bvec4> { static constexpr uint32_t value = GL_BOOL_VEC4; };
template<> struct type_to_gl_enum<glm::mat2> { static constexpr uint32_t value = GL_FLOAT_MAT2; };
template<> struct type_to_gl_enum<glm::mat3> { static constexpr uint32_t value = GL_FLOAT_MAT3; };
template<> struct type_to_gl_enum<glm::mat4> { static constexpr uint32_t value = GL_FLOAT_MAT4; };
template<> struct type_to_gl_enum<glm::mat2x3> { static constexpr uint32_t value = GL_FLOAT_MAT2x3; };
template<> struct type_to_gl_enum<glm::mat2x4> { static constexpr uint32_t value = GL_FLOAT_MAT2x4; };
template<> struct type_to_gl_enum<glm::mat3x2> { static constexpr uint32_t value = GL_FLOAT_MAT3x2; };
template<> struct type_to_gl_enum<glm::mat3x4> { static constexpr uint32_t value = GL_FLOAT_MAT3x4; };
template<> struct type_to_gl_enum<glm::mat4x2> { static constexpr uint32_t value = GL_FLOAT_MAT4x2; };
template<> struct type_to_gl_enum<glm::mat4x3> { static constexpr uint32_t value = GL_FLOAT_MAT4x3; };
template<> struct type_to_gl_enum<uint8_t>{ static constexpr uint32_t value = GL_UNSIGNED_BYTE; };
template<> struct type_to_gl_enum<int16_t> { static constexpr uint32_t value = GL_SHORT; };
template<> struct type_to_gl_enum<uint16_t>{ static constexpr uint32_t value = GL_UNSIGNED_SHORT; };
template<> struct type_to_gl_enum<int8_t>{ static constexpr uint32_t value = GL_BYTE; };

struct data_descriptor_element_t {
public:
  template <typename T> static constexpr data_descriptor_element_t build(){ return data_descriptor_element_t(T::length(), type_to_gl_enum<typename T::value_type>::value, sizeof(T)); };
  const uint32_t elements;
  const uint32_t gl_enum;
  const uint32_t length;
private:
  constexpr data_descriptor_element_t(const uint32_t elements, const uint32_t gl_enum, const uint32_t length) : elements{elements}, gl_enum{gl_enum}, length{length} {};
};

enum class Primitive : uint32_t {
  POINTS = GL_POINTS,
  LINE_STRIP = GL_LINE_STRIP,
  LINE_LOOP = GL_LINE_LOOP,
  LINES = GL_LINES,
  LINE_STRIP_ADJACENCY = GL_LINE_STRIP_ADJACENCY,
  LINES_ADJACENCY = GL_LINES_ADJACENCY,
  TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
  TRIANGLE_FAN = GL_TRIANGLE_FAN,
  TRIANGLES = GL_TRIANGLES,
  TRIANGLE_STRIP_ADJACENCY = GL_TRIANGLE_STRIP_ADJACENCY,
  TRIANGLES_ADJACENCY = GL_TRIANGLES_ADJACENCY
};

}
