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

#include "resources/resources.h"

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
  path_program = renderer::ShaderProgram::loadProgram(resource::manager.get_as_cstr("data/shaders/extrusion.vs"), resource::manager.get_as_cstr("data/shaders/extrusion.fs"), resource::manager.get_as_cstr("data/shaders/extrusion.gs"));
  program = renderer::ShaderProgram::loadProgram(resource::manager.get_as_cstr("data/shaders/default.vs"), resource::manager.get_as_cstr("data/shaders/default.fs"));

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
        renderer::vertex_data_t EFFECTOR_VERTEX(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(-0.5, 0.5, -0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX(-0.5, 0.5, -0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(0.5, 0.5, 0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX(0.0, 0.0, 0.0, EFFECTOR_COLOR_1),
        EFFECTOR_VERTEX(0.5, 0.5, 0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(-0.5, 0.5, -0.5, EFFECTOR_COLOR_3),
        EFFECTOR_VERTEX(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(0.5, 0.5, -0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(-0.5, 0.5, 0.5, EFFECTOR_COLOR_2),
        EFFECTOR_VERTEX(0.5, 0.5, 0.5, EFFECTOR_COLOR_3),
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

  // Generate a subdivided plane mesh for the bed
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
      m_bed_mesh_buffer->add_vertex(BED_VERTEX(x2, y2));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX(x1, y2));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX(x1, y1));
      //    /|
      //   / |
      //  /--|
      m_bed_mesh_buffer->add_vertex(BED_VERTEX(x2, y2));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX(x1, y1));
      m_bed_mesh_buffer->add_vertex(BED_VERTEX(x2, y1));
    }
  }

  auto kin = virtual_printer.get_component<KinematicSystem>("Cartesian Kinematic System");
  if(kin == nullptr) kin = virtual_printer.get_component<KinematicSystem>("Delta Kinematic System");
  if (kin != nullptr && kin->state.effector_position.size() == extrusion.size()) {
    size_t i = 0;
    for (auto state : kin->state.effector_position) {
      glm::vec4 pos = {state.position.x, state.position.z, state.position.y * -1.0, state.position.w};
      extrusion[i].last_position = pos;
      extrusion[i].position = pos;
      ++i;
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
      m_extruder_mesh[mesh_id]->m_transform_dirty = true;
    }
    m_extruder_mesh[mesh_id]->m_visible = (follow_mode != FOLLOW_Z);
    mesh_id ++;
  }

  // TODO:  Shaders need this internalised
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
  if (!m_initialised || hotend_index >= extrusion.size()) return;
  glm::vec4 sim_pos = state.position;
  glm::vec4 position = {sim_pos.x, sim_pos.z, sim_pos.y * -1.0, sim_pos.w}; // correct for opengl coordinate system
  auto& extruder = extrusion[hotend_index];
  glm::vec3 extrude_color = state.color;

  if (extruder.mesh != nullptr && extruder.mesh->m_delete) {
    extruder.active_mesh_buffer.reset();
    extruder.mesh.reset();
  }

  if (position != extruder.position) {

    // smooths out extrusion over a minimum length to fill in gaps todo: implement an simulation to do this better
    // also use Z (Y in opengl) change to reduce minimum extrude length
    if (glm::length(glm::vec3(position) - glm::vec3(extruder.last_extrusion_check)) > m_config.extrusion_check_min_line_length || glm::abs(position.y - extruder.last_extrusion_check.y) > m_config.extrusion_check_max_vertical_deviation) {
      extruder.extruding = position.w - extruder.last_extrusion_check.w > 0.0f;
      extruder.last_extrusion_check = position;
    }

    if (extruder.active_mesh_buffer != nullptr && extruder.active_mesh_buffer->size() > 1 && extruder.active_mesh_buffer->size() < renderer::Renderer::MAX_BUFFER_SIZE) {

      if (glm::length(glm::vec3(position) - glm::vec3(extruder.last_position)) > m_config.extrusion_segment_minimum_length) { // smooth out the path so the model renders with less geometry, rendering each individual step hurts the fps
        if((points_are_collinear(position, extruder.active_mesh_buffer->cdata().end()[-3].position, extruder.active_mesh_buffer->cdata().end()[-2].position, m_config.extrusion_segment_collinearity_max_deviation) && extruder.extruding == extruder.last_extruding) || ( extruder.extruding == false && extruder.last_extruding == false)) {
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
        buffer->add_vertex(buffer->cdata().back());
      }
      // extra dummy verticies for line strip adjacency
      extruder.active_mesh_buffer->add_vertex(extruder.active_mesh_buffer->cdata().back());
      extruder.active_mesh_buffer->add_vertex(extruder.active_mesh_buffer->cdata().back());
      extruder.last_position = position;
    }
    extruder.position = position;
  }
}

bool Visualisation::points_are_collinear(const glm::vec3 a, const glm::vec3 b, const glm::vec3 c, double const threshold) const {
  return glm::abs(glm::dot(b - a, c - a) - (glm::length(b - a) * glm::length(c - a))) < threshold;
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
    if (ImGui::GetIO().MouseWheel != 0 && viewport.hovered) {
      camera.position += camera.speed * camera.direction * delta * ImGui::GetIO().MouseWheel;
    }
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
        extruder.mesh->m_delete = true;
      }
    }
  }

  ImGui::PushItemWidth(150);
  ImGui::Text("Extrude Width    ");
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(50);
  ImGui::SameLine();
  ImGui::InputFloat("##Extrude_Width", &extrude_width);
  ImGui::PopItemWidth();

  ImGui::PushItemWidth(150);
  ImGui::Text("Extrude Thickness");
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(50);
  ImGui::SameLine();
  ImGui::InputFloat("##Extrude_Thickness", &extrude_thickness);
  ImGui::PopItemWidth();

  ImGui::PushItemWidth(150);
  ImGui::Text("Extrusion Check Min");
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(100);
  ImGui::InputDouble("##Extrusion_Check_Min", &m_config.extrusion_check_min_line_length);
  ImGui::PopItemWidth();

  ImGui::PushItemWidth(150);
  ImGui::Text("Extrusion Check Vertical Max");
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(100);
  ImGui::InputDouble("##Extrusion_Check_Vertical_Max", &m_config.extrusion_check_max_vertical_deviation);
  ImGui::PopItemWidth();

  ImGui::PushItemWidth(150);
  ImGui::Text("Extrusion Segment Min Length");
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(100);
  ImGui::InputDouble("##Extrusion_Segment_Min_Length", &m_config.extrusion_segment_minimum_length);
  ImGui::PopItemWidth();

  ImGui::PushItemWidth(150);
  ImGui::Text("Extrusion Collinearity Max Deviation");
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(100);
  ImGui::InputDouble("##Extrusion_Collinearity_Max_Deviation", &m_config.extrusion_segment_collinearity_max_deviation);
  ImGui::PopItemWidth();

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
