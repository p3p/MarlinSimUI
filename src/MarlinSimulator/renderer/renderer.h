#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "gl.h"

namespace renderer {

struct vertex_data_t {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec4 color;

  static constexpr std::size_t elements = 3;
  static constexpr std::array<data_descriptor_element_t, vertex_data_t::elements> descriptor {
    data_descriptor_element_t::build<decltype(position)>(),
    data_descriptor_element_t::build<decltype(normal)>(),
    data_descriptor_element_t::build<decltype(color)>()
  };
};

class BufferBase {
public:
  virtual ~BufferBase() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteBuffers(1, &m_vao);
  }
  virtual void generate() = 0;
  virtual void bind() = 0;
  virtual void upload() = 0;
  virtual void render() = 0;
  GLuint m_vao = 0;
  GLuint m_vbo = 0;
  size_t m_geometry_offset = 0;
  GLuint m_storage_hint = GL_STATIC_DRAW;
  Primitive m_geometry_type = Primitive::TRIANGLES;
  bool m_dirty = true;
  bool m_generated = false;
};

template <typename ElementType> class Buffer : public BufferBase {
public:
  virtual void generate() override {
    glGenVertexArrays( 1, &m_vao );
    glGenBuffers( 1, &m_vbo );
    glBindVertexArray( m_vao );
    glBindBuffer( GL_ARRAY_BUFFER, m_vbo );

    size_t index = 0, offset = 0;
    for (auto& attrib : ElementType::descriptor) {
      glEnableVertexAttribArray(index);
      glVertexAttribPointer(index, attrib.elements, attrib.gl_enum, GL_FALSE, sizeof(ElementType), (void *)offset);
      ++index;
      offset += attrib.length;
    }
    m_generated = true;
  }

  virtual void bind() override {
    if (!m_generated) generate();
    glBindVertexArray( m_vao );
    glBindBuffer( GL_ARRAY_BUFFER, m_vbo );
  }

  virtual void upload() override {
    if (m_dirty) {
      glBufferData( GL_ARRAY_BUFFER, m_data.size() * sizeof(ElementType), &m_data[0], m_storage_hint );
      m_dirty = false;
    }
  }

  virtual void render() override {
    bind();
    upload();
    glDrawArrays( (GLenum)m_geometry_type, m_geometry_offset, m_data.size());
  }

  static std::shared_ptr<Buffer<ElementType>> create() {
    return std::shared_ptr<Buffer<ElementType>>( new Buffer<ElementType>() );
  }

  std::vector<ElementType>& data() {
    m_dirty = true;
    return m_data;
  }

  std::vector<ElementType> const & cdata() const {
    return m_data;
  }

  size_t size() {
    return m_data.size();
  }

  void add_vertex(ElementType elem) {
    m_dirty = true;
    m_data.push_back(elem);
  }

private:
  std::vector<ElementType> m_data {};
  Buffer() { }
};

class Mesh {
public:
  void render(glm::mat4 global_transform) {
    if (!m_visible) return;
    if (m_dirty) {
      build_transform();
      m_dirty = false;
    }
    if (m_shader_dirty) {
      update_shader_locations();
      m_shader_dirty = false;
    }
    glUseProgram( m_shader_program );
    global_transform = global_transform * m_transform;
    glUniformMatrix4fv( m_shader_index_mvp, 1, GL_FALSE, glm::value_ptr(global_transform));
    for (auto buffer : m_buffer) {
      buffer->render();
    }
  }

  void build_transform() {
    m_transform = glm::translate(glm::mat4(1.0), m_position);
    m_transform = m_transform * glm::mat4_cast(m_rotation);
    m_transform = glm::scale(m_transform, m_scale);
    m_transform = glm::translate(m_transform, m_origin);
  }

  template <typename VertexType> static std::shared_ptr<Mesh> create(std::shared_ptr<VertexType> buffer) {
    auto mesh = std::shared_ptr<Mesh>(new Mesh());
    mesh->m_buffer.push_back(buffer);
    return mesh;
  }

  template <typename VertexType> static std::shared_ptr<Mesh> create() {
    auto mesh = std::shared_ptr<Mesh>(new Mesh());
    mesh->m_buffer.push_back(Buffer<VertexType>::create());
    return mesh;
  }

  template <typename VertexType> std::shared_ptr<Buffer<VertexType>> buffer() {
    return std::reinterpret_pointer_cast<Buffer<VertexType>>(m_buffer.back());
  }

  template <typename VertexType> std::vector<std::shared_ptr<Buffer<VertexType>>>& buffer_vector() {
    return *reinterpret_cast<std::vector<std::shared_ptr<Buffer<VertexType>>>*>(&m_buffer);
  }

  void update_shader_locations() {
    m_shader_index_mvp = glGetUniformLocation( m_shader_program, "u_mvp" );
  }

  void set_shader_program(GLuint program) {
    m_shader_program = program;
  }

  glm::mat4 m_transform {1.0};
  glm::vec3 m_origin {0.0, 0.0, 0.0};
  glm::vec3 m_position {0.0, 0.0, 0.0};
  glm::vec3 m_scale {1.0, 1.0, 1.0};
  glm::quat m_rotation {};
  bool m_visible = true;
  bool m_dirty = true;
  bool m_shader_dirty = true;
  bool m_delete = false;
  std::vector<std::shared_ptr<BufferBase>> m_buffer {};

  GLuint m_shader_program = 0;
  GLuint m_shader_index_mvp = 0;

private:
  Mesh() {}
};

class Renderer {
public:
  void render(glm::mat4 global_transform) {
    //TODO: mutex for modifying mesh vector
    m_mesh.erase(std::remove_if(m_mesh.begin(), m_mesh.end(), [](auto& mesh) { return mesh->m_delete; }), m_mesh.end());
    for (auto& mesh : m_mesh) {
      mesh->render(global_transform);
    }
  }

  std::vector<std::shared_ptr<Mesh>> m_mesh;
  static constexpr size_t MAX_BUFFER_SIZE = 100000;
};

}
