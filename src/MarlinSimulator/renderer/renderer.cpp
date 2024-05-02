#include "renderer.h"

namespace renderer {

template<> void ShaderProgramInstance::set_uniform<float>(std::string name, float* value) {
  using gl_uniform_type = gl_uniform<type_to_gl_enum<float>::value>;
  if (m_program->m_uniforms.count(name)) {
    auto uniform                 = m_program->m_uniforms[name];
    m_uniforms[uniform.location] = shader_uniform_t {
        uniform,
        gl_uniform_type {value},
        [](shader_attr_t& uniform_desc, gl_uniform_t& uniform_data) {
          gl_uniform_type().invoke(uniform_desc.location, uniform_desc.size, std::get<gl_uniform_type>(uniform_data).value);
        },
    };
  }
}

}
