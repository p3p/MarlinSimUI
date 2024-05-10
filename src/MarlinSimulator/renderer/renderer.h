#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../resources/resources.h"
#include "gl.h"

namespace renderer {

using mesh_id_t = size_t;
class Mesh;
extern mesh_id_t next_mesh_index;
extern std::map<mesh_id_t, std::shared_ptr<Mesh>> global_mesh_list;
extern std::mutex global_mesh_list_mutex;
inline constexpr size_t MAX_BUFFER_SIZE = 100000;

void gl_defer_call(std::function<void(void)> fn);
void render(glm::mat4 global_transform);
void destroy();

struct vertex_data_t {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec4 color;

  static constexpr std::size_t elements = 3;
  static constexpr std::array<data_descriptor_element_t, vertex_data_t::elements> descriptor {
      data_descriptor_element_t::build<decltype(position)>(),
      data_descriptor_element_t::build<decltype(normal)>(),
      data_descriptor_element_t::build<decltype(color)>(),
  };
};

struct shader_attr_t {
  uint32_t location;
  std::string name;
  uint32_t gl_type;
  int32_t size;
  bool active = true;
};

class ShaderProgram {
public:
  ShaderProgram(GLuint program_id) : m_program_id {program_id} { }

  ~ShaderProgram() {
    auto id = m_program_id;
    gl_defer_call([id]() {
      glDeleteShader(id);
    });
  }

  static GLuint load_program(char const* vertex_string, char const* fragment_string, char const* geometry_string = nullptr) {
    GLuint vertex_shader = 0, fragment_shader = 0, geometry_shader = 0;
    if (vertex_string != nullptr) {
      vertex_shader = load_shader(GL_VERTEX_SHADER, vertex_string);
    }
    if (fragment_string != nullptr) {
      fragment_shader = load_shader(GL_FRAGMENT_SHADER, fragment_string);
    }
    if (geometry_string != nullptr) {
      geometry_shader = load_shader(GL_GEOMETRY_SHADER, geometry_string);
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    if (geometry_shader) glAttachShader(shader_program, geometry_shader);

    glLinkProgram(shader_program);
    glUseProgram(shader_program);

    if (vertex_shader) glDeleteShader(vertex_shader);
    if (fragment_shader) glDeleteShader(fragment_shader);
    if (geometry_shader) glDeleteShader(geometry_shader);

    return shader_program;
  }

  static GLuint load_shader(GLuint shader_type, char const* shader_string) {
    GLuint shader_id = glCreateShader(shader_type);
    int length       = strlen(shader_string);
    glShaderSource(shader_id, 1, (GLchar const**)&shader_string, &length);
    glCompileShader(shader_id);

    GLint status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      GLint maxLength = 0;
      glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &maxLength);
      std::vector<GLchar> errorLog(maxLength + 1);
      errorLog[maxLength] = 0;
      glGetShaderInfoLog(shader_id, maxLength, &maxLength, errorLog.data());
      printf("%s\n", errorLog.data());
      glDeleteShader(shader_id);
      return 0;
    }
    return shader_id;
  }

  static std::shared_ptr<ShaderProgram> create(
      const std::filesystem::path vertex_path, const std::filesystem::path fragment_path, const std::filesystem::path geometry_path = {}
  ) {
    auto program_id = load_program(
        resource::ResourceManager::get_as_cstr(vertex_path),
        resource::ResourceManager::get_as_cstr(fragment_path),
        resource::ResourceManager::get_as_cstr(geometry_path)
    );
    if (program_id == 0) return nullptr; // default shader?
    auto program = std::make_shared<ShaderProgram>(program_id);
    if (program) program->build_cache();
    return program;
  }

  void build_cache() {
    GLint size {};  // size of the variable
    GLenum type {}; // type of the variable (float, vec3 or mat4, etc)

    const GLsizei bufSize = 64; // maximum name length //GL_ACTIVE_UNIFORM_MAX_LENGTH
    GLchar name[bufSize];       // variable name in GLSL
    GLsizei length;             // name length

    GLint attribute_count;
    for (auto& attr : m_attributes) {
      attr.second.active = false; // disable all current attributes for reload
    }
    glGetProgramiv(m_program_id, GL_ACTIVE_ATTRIBUTES, &attribute_count);
    for (GLint i = 0; i < attribute_count; i++) {
      glGetActiveAttrib(m_program_id, (GLuint)i, bufSize, &length, &size, &type, (char*)name);
      m_attributes[name] = shader_attr_t {(uint32_t)i, name, type, size};
    }

    GLint uniform_count;
    for (auto& uniform : m_uniforms) {
      uniform.second.active = false; // disable all current uniforms for reload
    }
    glGetProgramiv(m_program_id, GL_ACTIVE_UNIFORMS, &uniform_count);
    for (GLint i = 0; i < uniform_count; i++) {
      glGetActiveUniform(m_program_id, (GLuint)i, bufSize, &length, &size, &type, (char*)name);
      m_uniforms[name] = shader_attr_t {(uint32_t)i, name, type, size};
    }
  }

  GLuint m_program_id {};
  std::map<std::string, shader_attr_t> m_attributes {};
  std::map<std::string, shader_attr_t> m_uniforms {};
};

class ShaderProgramInstance {
public:
  ShaderProgramInstance(std::shared_ptr<ShaderProgram> program) : m_program {program} { }

  struct shader_uniform_t {
    shader_attr_t desc;
    gl_uniform_t uniform_data;
    std::function<void(shader_attr_t& uniform_desc, gl_uniform_t& uniform_data)> gl_uniform_call;
  };

  void bind_immediate() {
    glUseProgram(m_program->m_program_id);
    for (auto& [location, uniform] : m_uniforms) {
      uniform.gl_uniform_call(uniform.desc, uniform.uniform_data);
    }
  }

  template<typename T> void set_uniform(std::string name, T* value) {
    using gl_uniform_type        = gl_uniform<type_to_gl_enum<T>::value>;
    auto uniform                 = m_program->m_uniforms[name];
    m_uniforms[uniform.location] = shader_uniform_t {
        uniform,
        gl_uniform_type {value},
        [](shader_attr_t& uniform_desc, gl_uniform_t& uniform_data) {
          if constexpr (gl_uniform_type::is_matrix) {
            gl_uniform_type().invoke(uniform_desc.location, uniform_desc.size, GL_FALSE, glm::value_ptr(*std::get<gl_uniform_type>(uniform_data).value));
          } else if constexpr (std::is_same_v<T, unsigned int> || std::is_same_v<T, int> || std::is_same_v<T, bool> || std::is_same_v<T, float>) {
            gl_uniform_type().invoke(uniform_desc.location, uniform_desc.size, &(*std::get<gl_uniform_type>(uniform_data).value));
          } else {
            gl_uniform_type().invoke(uniform_desc.location, uniform_desc.size, glm::value_ptr(*std::get<gl_uniform_type>(uniform_data).value));
          }
        },
    };
    return;
  }

  std::shared_ptr<ShaderProgram> m_program {};
  std::map<uint32_t, shader_uniform_t> m_uniforms;
};

class BufferBase {
public:
  virtual ~BufferBase() { }

  virtual void destroy()  = 0;
  virtual void generate() = 0;
  virtual bool bind()     = 0;
  virtual void upload()   = 0;
  virtual void render()   = 0;

  GLuint m_vao                      = 0;
  GLuint m_vbo                      = 0;
  size_t m_geometry_offset          = 0;
  GLuint m_storage_hint             = GL_STATIC_DRAW;
  GeometryPrimitive m_geometry_type = GeometryPrimitive::TRIANGLES;
  bool m_dirty                      = true;
  bool m_generated                  = false;
};

template<typename ElementType> class Buffer : public BufferBase {
public:
  virtual void generate() override {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    size_t index = 0, offset = 0;
    for (auto& attrib : ElementType::descriptor) {
      glEnableVertexAttribArray(index);
      glVertexAttribPointer(index, attrib.elements, attrib.gl_enum, GL_FALSE, sizeof(ElementType), (void*)offset);
      ++index;
      offset += attrib.length;
    }
    m_generated = true;
  }

  virtual void destroy() override {
    GLuint vbo = m_vbo;
    GLuint vao = m_vao;
    if (m_vbo) gl_defer_call([vbo]() { glDeleteBuffers(1, &vbo); });
    if (m_vao) gl_defer_call([vao]() { glDeleteBuffers(1, &vao); });
    m_vbo = m_vao = 0;
  }

  ~Buffer() {
    destroy();
  }

  virtual bool bind() override {
    if (!m_generated) generate();
    if (m_vao == 0 || m_vbo == 0) return false;
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    return true;
  }

  virtual void upload() override {
    if (m_dirty && m_data.size() > 0) {
      glBufferData(GL_ARRAY_BUFFER, m_data.size() * sizeof(ElementType), &m_data[0], m_storage_hint);
      m_dirty = false;
    }
  }

  virtual void render() override {
    if (bind()) {
      upload();
      glDrawArrays((GLenum)m_geometry_type, m_geometry_offset, m_data.size());
    }
  }

  static std::shared_ptr<Buffer<ElementType>> create() {
    return std::shared_ptr<Buffer<ElementType>>(new Buffer<ElementType>());
  }

  std::vector<ElementType>& data() {
    m_dirty = true;
    return m_data;
  }

  std::vector<ElementType> const& cdata() const {
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
  Buffer() { }

  std::vector<ElementType> m_data {};
};


class Mesh {
public:
  void render(glm::mat4 global_transform) {
    if ((!m_visible) || m_shader_instance == nullptr) return;
    if (m_transform_dirty) {
      build_transform();
      m_transform_dirty = false;
    }
    if (m_shader_dirty) {
      m_shader_instance->set_uniform("u_mvp", &m_view_projection_transform);
      m_shader_dirty = false;
    }
    m_view_projection_transform = global_transform * m_transform;
    m_shader_instance->bind_immediate();
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

  static std::shared_ptr<Mesh> create(std::shared_ptr<BufferBase> buffer) {
    auto mesh = std::shared_ptr<Mesh>(new Mesh());
    mesh->m_buffer.push_back(buffer);
    return mesh;
  }

  static std::shared_ptr<Mesh> create() {
    return std::shared_ptr<Mesh>(new Mesh());
  }

  template<typename VertexType> std::shared_ptr<Buffer<VertexType>> buffer() {
    return m_buffer.size() ? std::reinterpret_pointer_cast<Buffer<VertexType>>(m_buffer.back()) : nullptr;
  }

  template<typename VertexType> std::vector<std::shared_ptr<Buffer<VertexType>>>& buffer_vector() {
    return *reinterpret_cast<std::vector<std::shared_ptr<Buffer<VertexType>>>*>(&m_buffer);
  }

  void set_shader_program(std::shared_ptr<ShaderProgram> program) {
    m_shader_instance = std::make_shared<ShaderProgramInstance>(program);
  }

  void set_shader_program(std::shared_ptr<ShaderProgramInstance> program_instance) {
    m_shader_instance = program_instance;
  }

  glm::mat4 m_view_projection_transform {1.0};
  glm::mat4 m_transform {1.0};
  glm::vec3 m_origin {0.0, 0.0, 0.0};
  glm::vec3 m_position {0.0, 0.0, 0.0};
  glm::vec3 m_scale {1.0, 1.0, 1.0};
  glm::quat m_rotation {};
  bool m_visible         = true;
  bool m_transform_dirty = true;
  bool m_shader_dirty    = true;
  bool m_delete          = false;
  std::vector<std::shared_ptr<BufferBase>> m_buffer {};
  std::shared_ptr<ShaderProgramInstance> m_shader_instance = 0;

private:
  Mesh() { }
};

template<typename... Args> mesh_id_t create_mesh(Args... args) {
  std::scoped_lock global_lock(global_mesh_list_mutex);
  global_mesh_list.emplace(next_mesh_index, Mesh::create(std::forward(args)...));
  return next_mesh_index++;
}

std::shared_ptr<Mesh> get_mesh_by_id(mesh_id_t resource_index);
void destroy_mesh(mesh_id_t resource_index);
void render_mesh(mesh_id_t resource_index);
void render_list_is_ready();
}
