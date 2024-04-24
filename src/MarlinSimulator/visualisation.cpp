#include "visualisation.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <vector>
#include <array>
#include <imgui_internal.h>
#include <implot.h>

#include "src/inc/MarlinConfig.h"
#include "src/module/motion.h"

static GLfloat * SetBedVertexAndAdvance(GLfloat * dest, GLfloat x, GLfloat y) {
  const GLfloat new_vertex[VERTEX_FLOAT_COUNT] = { BED_VERTEX(x, y) };
  memcpy(dest, new_vertex, sizeof(new_vertex));
  return dest + VERTEX_FLOAT_COUNT;
}

Visualisation::Visualisation(VirtualPrinter& virtual_printer) : virtual_printer(virtual_printer) {
  virtual_printer.on_kinematic_update = [this](kinematic_state state){
    for (size_t i = 0; i < state.effector_position.size(); ++i) {
      this->set_head_position(i, state.effector_position[i]);
    }
  };

  for (int i = 0; i < EXTRUDERS; ++i) {
    extrusion.push_back({});
  }
}

Visualisation::~Visualisation() {
  destroy();
}

void Visualisation::create() {
  path_program = ShaderProgram::loadProgram(renderer::path_vertex_shader, renderer::path_fragment_shader, renderer::geometry_shader);
  program = ShaderProgram::loadProgram(renderer::vertex_shader, renderer::fragment_shader);

  framebuffer = new opengl_util::MsaaFrameBuffer();
  if (!((opengl_util::MsaaFrameBuffer*)framebuffer)->create(100, 100, 4)) {
    fprintf(stderr, "Failed to initialise MSAA Framebuffer falling back to TextureFramebuffer\n");
    delete framebuffer;
    framebuffer = new opengl_util::TextureFrameBuffer();
    if (!((opengl_util::TextureFrameBuffer*)framebuffer)->create(100,100)) {
      fprintf(stderr, "Unable to initialise a Framebuffer\n");
    }
  }

  camera = { {37.0f, 121.0f, 129.0f}, {-192.0f, -25.0, 0.0f}, {0.0f, 1.0f, 0.0f}, float(100) / float(100), glm::radians(45.0f), 0.1f, 2000.0f};
  camera.generate();

  if (EXTRUDERS > 0) {
    auto mesh = renderer::Mesh::create<renderer::vertex_data_t>();
    auto buffer = mesh->buffer<renderer::vertex_data_t>();
    buffer->data() = {
        renderer::vertex_data_t EFFECTOR_VERTEX2(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX2(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(-0.5, 0.5, -0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX2(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX2(-0.5, 0.5, -0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX2(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX2(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(0.5, 0.5, 0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX2(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX2(0.5, 0.5, 0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX2(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(-0.5, 0.5, -0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX2(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX2(0.5, 0.5, 0.5, EFFECTOR_COLOR_3),
    };

    m_extruder_mesh.push_back(mesh);
    for (int i = 1; i < EXTRUDERS; ++i) {
      m_extruder_mesh.push_back(renderer::Mesh::create(buffer));
    }
  }

  for (auto m : m_extruder_mesh) {
    m->set_shader_program(program);
    m->m_scale = effector_scale;
    m_renderer.m_mesh.push_back(m);
  }

  m_bed_mesh = renderer::Mesh::create<renderer::vertex_data_t>();
  m_bed_mesh->set_shader_program(program);
  m_bed_mesh_buffer = m_bed_mesh->buffer<renderer::vertex_data_t>();
  m_renderer.m_mesh.push_back(m_bed_mesh);

  m_bed_mesh_buffer->data().reserve(BED_NUM_VERTEXES_PER_AXIS * BED_NUM_VERTEXES_PER_AXIS);
  // Calculate the number of divisions (line segments) along each axis.
  const GLfloat x_div = GLfloat(build_plate_dimension.x) / (BED_NUM_VERTEXES_PER_AXIS - 1);
  const GLfloat y_div = GLfloat(-build_plate_dimension.y) / (BED_NUM_VERTEXES_PER_AXIS - 1);

  for (int row = 0; row < BED_NUM_VERTEXES_PER_AXIS - 1; ++row) {
    for (int col = 0; col < BED_NUM_VERTEXES_PER_AXIS - 1; ++col) {
      // For each division, calculate the coordinates of the four corners.
      GLfloat x1 = col * x_div;
      GLfloat x2 = (col + 1) * x_div;
      GLfloat y1 = row * y_div;
      GLfloat y2 = (row + 1) * y_div;

      // Create two triangles from the four corners.

      // |--/
      // | /
      // |/
      m_bed_mesh_buffer->add_vertex(BED_VERTEX2(x2, y2));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX2(x1, y2));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX2(x1, y1));
      //    /|
      //   / |
      //  /--|
      m_bed_mesh_buffer->add_vertex(BED_VERTEX2(x2, y2));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX2(x1, y1));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX2(x2, y1));
    }
  }

  m_initialised = true;

}

void Visualisation::process_event(SDL_Event& e) { }
void Visualisation::gpio_event_handler(GpioEvent& event) { }
void Visualisation::on_position_update() { }

using millisec = std::chrono::duration<float, std::milli>;
void Visualisation::update() {
  // auto now = clock.now();
  // float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_update).count();
  // last_update = now;
  auto effector_pos = extrusion[0].position;

  switch (follow_mode) {
    case FOLLOW_Z:  camera.position = glm::vec3(effector_pos.x, camera.position.y, effector_pos.z); break;
    case FOLLOW_XY: camera.position = glm::vec3(camera.position.x, effector_pos.y + follow_offset.y, camera.position.z); break;
    default: break;
  }
  camera.update_view();

  auto print_bed = virtual_printer.get_component<PrintBed>("Print Bed");

  if (print_bed->dirty) {
    GLfloat max_abs_z = 0.0f;
    // todo: move vertex generation
    for (auto& v : m_bed_mesh_buffer->data()) {
      GLfloat z = print_bed->calculate_z({v.position.x, -v.position.z});
      v.position.y = z;
      max_abs_z = std::max(max_abs_z, abs(z));
    }

    for (auto&v : m_bed_mesh_buffer->data()) {
      GLfloat r = 0.0, g = 0.0, b = 0.0;

      if (print_bed->gradient_enabled) {
        GLfloat z = v.position.y;
        GLfloat *dest = z < 0.0 ? &r : &b;
        GLfloat gradient_range = std::max(1.0f, max_abs_z);

        *dest = std::min(1.0f, abs(z) / gradient_range);
        g = 0.5f - *dest;
      } else {
        // default color
        r = g = b = 0.5f;
      }

      v.color.r = r;
      v.color.g = g;
      v.color.b = b;
    }
    print_bed->dirty = false;
  }

  // update the position of the extruder mesh for visualisation
  size_t mesh_id = 0;
  for (auto& ext : extrusion ) {
    auto pos = glm::vec3(ext.position.x, ext.position.y, ext.position.z);
    if (m_extruder_mesh[mesh_id]->m_position != pos) {
      m_extruder_mesh[mesh_id]->m_position = pos;
      m_extruder_mesh[mesh_id]->m_dirty = true;
    }
    m_extruder_mesh[mesh_id]->m_visible = (follow_mode != FOLLOW_Z);
    mesh_id ++;
  }

  glUseProgram( path_program );
  glUniform1f( glGetUniformLocation( path_program, "u_layer_thickness" ), extrude_thickness);
  glUniform1f( glGetUniformLocation( path_program, "u_layer_width" ), extrude_width);
  glUniform3fv( glGetUniformLocation( path_program, "u_view_position" ), 1, glm::value_ptr(camera.position));
  m_renderer.render(camera.proj * camera.view);
}

void Visualisation::destroy() {
  if(framebuffer != nullptr) {
    framebuffer->release();
    delete framebuffer;
  }
}

void Visualisation::set_head_position(size_t hotend_index, extruder_state state) {
  if (!m_initialised) return;
  glm::vec4 sim_pos = state.position;
  glm::vec4 position = {sim_pos.x, sim_pos.z, sim_pos.y * -1.0, sim_pos.w}; // correct for opengl coordinate system
  if (hotend_index >= extrusion.size()) return;
  auto& extruder = extrusion[hotend_index];
  glm::vec3 extrude_color = state.color;

  if (position != extruder.position) {

    if (glm::length(glm::vec3(position) - glm::vec3(extruder.last_extrusion_check)) > 0.5f) { // smooths out extrusion over a minimum length to fill in gaps todo: implement an simulation to do this better
      extruder.extruding = position.w - extruder.last_extrusion_check.w > 0.0f;
      extruder.last_extrusion_check = position;
    }

    if (extruder.active_mesh_buffer != nullptr && extruder.active_mesh_buffer->size() > 1 && extruder.active_mesh_buffer->size() < renderer::Renderer::MAX_BUFFER_SIZE) {

      if (glm::length(glm::vec3(position) - glm::vec3(extruder.last_position)) > 0.05f) { // smooth out the path so the model renders with less geometry, rendering each individual step hurts the fps
        if((points_are_collinear(position, extruder.active_mesh_buffer->cdata().end()[-3].position, extruder.active_mesh_buffer->cdata().end()[-2].position) && extruder.extruding == extruder.last_extruding) || ( extruder.extruding == false &&  extruder.last_extruding == false)) {
          // collinear and extrusion state has not changed so we can just change the current point.
          extruder.active_mesh_buffer->data().end()[-2].position = position;
          extruder.active_mesh_buffer->data().end()[-1].position = position;
        } else { // new point is not collinear with current path add new point
          extruder.active_mesh_buffer->data().end()[-1] = {position, {0.0, 1.0, 0.0}, {extrude_color, extruder.extruding}};
          extruder.active_mesh_buffer->add_vertex(extruder.active_mesh_buffer->cdata().back());
        }
        extruder.last_position = position;
        extruder.last_extruding = extruder.extruding;
      }

    } else { // need to change geometry buffer
      if (extruder.active_mesh_buffer == nullptr) {
        auto mesh = renderer::Mesh::create<renderer::vertex_data_t>();
        mesh->set_shader_program(path_program);
        extruder.active_mesh_buffer = mesh->buffer<renderer::vertex_data_t>();
        extruder.active_mesh_buffer->data().reserve(renderer::Renderer::MAX_BUFFER_SIZE);
        extruder.active_mesh_buffer->m_geometry_type = renderer::Primitive::LINE_STRIP_ADJACENCY;
        extruder.mesh = mesh;
        extruder.last_extrusion_check = position;
        m_renderer.m_mesh.push_back(mesh);

        extruder.active_mesh_buffer->data().push_back({position, {0.0, 1.0, 0.0}, {extrude_color, 0.0}});
      } else {
        renderer::vertex_data_t last_vertex = extruder.active_mesh_buffer->cdata().back();
        auto buffer = renderer::Buffer<renderer::vertex_data_t>::create();
        buffer->data().reserve(renderer::Renderer::MAX_BUFFER_SIZE);
        buffer->m_geometry_type = renderer::Primitive::LINE_STRIP_ADJACENCY;
        extruder.active_mesh_buffer = buffer;
        extruder.mesh->buffer_vector<renderer::vertex_data_t>().push_back(buffer);

        buffer->add_vertex(last_vertex);
        buffer->add_vertex({position, {0.0, 1.0, 0.0}, {extrude_color, extruder.extruding}});
      }
      // extra dummy verticies for line strip adjacency
      extruder.active_mesh_buffer->add_vertex(extruder.active_mesh_buffer->cdata().back());
      extruder.active_mesh_buffer->add_vertex(extruder.active_mesh_buffer->cdata().back());
      extruder.last_position = position;
    }
    extruder.position = position;
  }
}

bool Visualisation::points_are_collinear(glm::vec3 a, glm::vec3 b, glm::vec3 c) {
  return glm::length(glm::dot(b - a, c - a) - (glm::length(b - a) * glm::length(c - a))) < 0.0002; // could be increased to further reduce rendered geometry
}

void Visualisation::ui_viewport_callback(UiWindow* window) {
  auto now = clock.now();
  float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now- last_update).count();
  last_update = now;

  Viewport& viewport = *((Viewport*)window);
  auto& ex = extrusion[0];

  if (viewport.dirty) {
    framebuffer->update(viewport.viewport_size.x, viewport.viewport_size.y);
    viewport.texture_id = framebuffer->texture_id();
    camera.update_aspect_ratio(viewport.viewport_size.x / viewport.viewport_size.y);
  }

  if (viewport.focused) {
    if (ImGui::IsKeyDown(SDL_SCANCODE_W)) {
      camera.position += camera.speed * camera.direction * delta;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_S)) {
      camera.position -= camera.speed * camera.direction * delta;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_A)) {
      camera.position -= glm::normalize(glm::cross(camera.direction, camera.up)) * camera.speed * delta;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_D)) {
      camera.position += glm::normalize(glm::cross(camera.direction, camera.up)) * camera.speed * delta;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_SPACE)) {
      camera.position += camera.world_up * camera.speed * delta;
    }
    if (ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
      camera.position -= camera.world_up * camera.speed * delta;
    }
    if (ImGui::IsKeyPressed(SDL_SCANCODE_F)) {
      follow_mode = follow_mode == FOLLOW_Z ? FOLLOW_NONE : FOLLOW_Z;
      if (follow_mode != FOLLOW_NONE) {
        camera.position = glm::vec3(ex.position.x, ex.position.y + 10.0, ex.position.z);
        camera.rotation.y = -89.99999;
      }
    }
    if (ImGui::IsKeyPressed(SDL_SCANCODE_G)) {
      follow_mode = follow_mode == FOLLOW_XY ? FOLLOW_NONE : FOLLOW_XY;
      if (follow_mode != FOLLOW_NONE)
        follow_offset = camera.position - glm::vec3(ex.position);
    }
    if (ImGui::IsKeyPressed(SDL_SCANCODE_F1)) {
      for (auto& mesh : m_renderer.m_mesh) {
        mesh->m_visible = false;
      }
    }
    if (ImGui::IsKeyPressed(SDL_SCANCODE_F2)) {
      for (auto& mesh : m_renderer.m_mesh) {
        mesh->m_visible = true;
      }
    }
    // if (ImGui::IsKeyPressed(SDL_SCANCODE_F4)) {
    //   for (auto& extruder : extrusion) {
    //     extruder.active_path_block = nullptr;
    //     extruder.full_path.clear();
    //   }
    // }
    if (ImGui::GetIO().MouseWheel != 0 && viewport.hovered) {
      camera.position += camera.speed * camera.direction * delta * ImGui::GetIO().MouseWheel;
    }
  }

  bool last_mouse_captured = mouse_captured;
  if (ImGui::IsMouseDown(0) && viewport.hovered) {
    mouse_captured = true;
  } else if (!ImGui::IsMouseDown(0)) {
    mouse_captured = false;
  }

  if (mouse_captured && !last_mouse_captured) {
    ImVec2 mouse_pos = ImGui::GetMousePos();
    mouse_lock_pos = {mouse_pos.x, mouse_pos.y};
    SDL_SetWindowGrab(SDL_GL_GetCurrentWindow(), SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
  } else if (!mouse_captured && last_mouse_captured) {
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowGrab(SDL_GL_GetCurrentWindow(), SDL_FALSE);
    SDL_WarpMouseInWindow(SDL_GL_GetCurrentWindow(), mouse_lock_pos.x, mouse_lock_pos.y);
  } else if (mouse_captured) {
    camera.rotation.x -= ImGui::GetIO().MouseDelta.x * 0.2;
    camera.rotation.y -= ImGui::GetIO().MouseDelta.y * 0.2;
    if (camera.rotation.y > 89.0f) camera.rotation.y = 89.0f;
    else if (camera.rotation.y < -89.0f) camera.rotation.y = -89.0f;
  }
};


// Test graph

struct ScrollingData {
    int MaxSize;
    int Offset;
    ImVector<ImPlotPoint> Data;
    ScrollingData() {
        MaxSize = 2000;
        Offset  = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImPlotPoint(x,y));
        else {
            Data[Offset] = ImPlotPoint(x,y);
            Offset =  (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset  = 0;
        }
    }
};

void Visualisation::ui_info_callback(UiWindow* w) {
  if (ImGui::Button("Clear Print Area")) {
    for (auto& extruder : extrusion) {
      if (extruder.mesh != nullptr) {
        extruder.active_mesh_buffer.reset();
        extruder.mesh->m_delete = true;
        extruder.mesh.reset();
      }
    }
  }
  ImGui::PushItemWidth(150); ImGui::Text("Extrude Width    ");  ImGui::PopItemWidth(); ImGui::PushItemWidth(50); ImGui::SameLine(); ImGui::InputFloat("##Extrude_Width", &extrude_width); ImGui::PopItemWidth();
  ImGui::PushItemWidth(150); ImGui::Text("Extrude Thickness");  ImGui::PopItemWidth(); ImGui::PushItemWidth(50); ImGui::SameLine(); ImGui::InputFloat("##Extrude_Thickness", &extrude_thickness); ImGui::PopItemWidth();

  size_t count = 0;
  for (auto& extruder : extrusion) {
    if (extruder.mesh != nullptr) {
      bool vis = extruder.mesh->m_visible;
      std::string label = "Extrusion ";
      label += std::to_string(count);
      if (ImGui::Checkbox(label.c_str(), &vis)) {
        extruder.mesh->m_visible = vis;
      }
    }
    ++count;
  }
}
