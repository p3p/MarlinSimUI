#pragma once

namespace resource {

static const char * data_shader_extrusion_gs = R"SHADERSTR(
  #version 330 core
  layout (lines_adjacency) in;
  layout (triangle_strip, max_vertices = 28) out;

  in vec3 g_normal[];
  in vec4 g_color[];
  out vec4 v_color;
  out vec3 v_normal;
  out vec4 v_position;

  uniform mat4 u_mvp;
  uniform float u_layer_thickness;// = 0.3;
  uniform float u_layer_width;// = 0.4;

  vec4 mvp_vertices[9];
  vec4 vertices[9];
  void emit(const int a, const int b, const int c, const int d) {
    gl_Position = mvp_vertices[a]; v_position = vertices[a]; EmitVertex();
    gl_Position = mvp_vertices[b]; v_position = vertices[b]; EmitVertex();
    gl_Position = mvp_vertices[c]; v_position = vertices[c]; EmitVertex();
    gl_Position = mvp_vertices[d]; v_position = vertices[d]; EmitVertex();
    EndPrimitive();
  }

  const float epsilon = 0.0004;

  void main() {
    vec3 prev = gl_in[0].gl_Position.xyz;
    vec3 start = gl_in[1].gl_Position.xyz;
    vec3 end = gl_in[2].gl_Position.xyz;
    vec3 next = gl_in[3].gl_Position.xyz;

    vec4 prev_color = g_color[1];
    vec4 active_color = g_color[2];
    vec4 next_color = g_color[3];

    vec3 forward = normalize(end - start);
    vec3 left = normalize(cross(forward, g_normal[2])); // what if formward is world_up? zero vector
    if (left == vec3(0.0)) return; //panic

    vec3 up = normalize(cross(forward, left));
    up *= sign(up); // make sure up is positive

    bool first_segment = length(start - prev) < epsilon;
    bool last_segment = length(end - next) < epsilon;
    vec3 a = normalize(start - prev);
    vec3 b = normalize(start - end);
    vec3 c = (a + b) * 0.5;
    vec3 start_lhs = normalize(c) * sign(dot(c, left));
    a = normalize(end - start);
    b = normalize(end - next);
    c = (a + b) * 0.5;
    vec3 end_lhs = normalize(c) * sign(dot(c, left));

    vec2 xz_dir_a = normalize(start.xz - prev.xz);
    vec2 xz_dir_b = normalize(end.xz - start.xz);
    vec2 xz_dir_c = normalize(next.xz - end.xz);


    // remove edge cases that will break teh following algorithm, angle more than 90 degrees or 0 (points collinear), also break on extrude state change (color)
    if(first_segment || active_color.a != prev_color.a || normalize(next - prev).y != 0.0 ||  dot(normalize(start.xz - prev.xz), normalize(end.xz - start.xz)) < epsilon || dot(normalize(start.xz - prev.xz), normalize(end.xz - start.xz)) > 1.0 - epsilon) {
      start_lhs = left;
      first_segment = true;
    }
    if(last_segment || active_color.a != next_color.a || normalize(next - prev).y != 0.0 || dot(normalize(end.xz - start.xz), normalize(next.xz - end.xz)) < epsilon || dot(normalize(end.xz - start.xz), normalize(next.xz - end.xz)) > 1.0 - epsilon) {
      end_lhs = left;
      last_segment = true;
    }

    float start_join_scale = dot(start_lhs, left);
    float end_join_scale = dot(end_lhs, left);
    start_lhs *= u_layer_width * 0.5;
    end_lhs *= u_layer_width * 0.5;

    float half_layer_width = u_layer_width / 2.0;
    vertices[0] = vec4(start - start_lhs / start_join_scale, 1.0); // top_back_left
    vertices[1] = vec4(start + start_lhs / start_join_scale, 1.0); // top_back_right
    vertices[2] = vec4(end   - end_lhs / end_join_scale, 1.0);   // top_front_left
    vertices[3] = vec4(end   + end_lhs / end_join_scale, 1.0);   // top_front_right
    vertices[4] = vec4(start - start_lhs / start_join_scale - (up * u_layer_thickness), 1.0); // bottom_back_left
    vertices[5] = vec4(start + start_lhs / start_join_scale - (up * u_layer_thickness), 1.0); // bottom_back_right
    vertices[6] = vec4(end   - end_lhs / end_join_scale - (up * u_layer_thickness), 1.0);   // bottom_front_left
    vertices[7] = vec4(end   + end_lhs / end_join_scale - (up * u_layer_thickness), 1.0);   // bottom_front_right

    mvp_vertices[0] = u_mvp * vertices[0];
    mvp_vertices[1] = u_mvp * vertices[1];
    mvp_vertices[2] = u_mvp * vertices[2];
    mvp_vertices[3] = u_mvp * vertices[3];
    mvp_vertices[4] = u_mvp * vertices[4];
    mvp_vertices[5] = u_mvp * vertices[5];
    mvp_vertices[6] = u_mvp * vertices[6];
    mvp_vertices[7] = u_mvp * vertices[7];

    vertices[8] = vec4(start - (left * half_layer_width) + (up * 1.0), 1.0);
    mvp_vertices[8] = u_mvp * vertices[8];

    v_color = active_color;
    v_normal = forward;
    if (last_segment) emit(2, 3, 6, 7); // thise should be rounded ends of path diamter, not single po
    v_normal = -forward;
    if (first_segment) emit(1, 0, 5, 4);
    v_normal = -left;
    emit(3, 1, 7, 5);
    v_normal = left;
    emit(0, 2, 4, 6);
    v_normal = up;
    emit(0, 1, 2, 3);
    v_normal = -up;
    emit(5, 4, 7, 6);

    //emit(0, 1, 8, 0); //show up normal
  })SHADERSTR";

static const char * data_shader_extrusion_vs = R"SHADERSTR(
  #version 330 core
  in vec3 i_position;
  in vec3 i_normal;
  in vec4 i_color;
  out vec4 g_color;
  out vec3 g_normal;
  void main() {
      g_color = i_color;
      g_normal = i_normal;
      gl_Position = vec4( i_position, 1.0 );
  })SHADERSTR";

static const char * data_shader_extrusion_fs = R"SHADERSTR(
  #version 330 core
  in vec4 v_color;
  out vec4 o_color;
  in vec3 v_normal;
  in vec4 v_position;

  uniform vec3 u_view_position;
  void main() {
      if(v_color.a < 0.1) discard;

      float ambient_level = 0.1;
      vec3 ambient_color = vec3(1.0, 0.86, 0.66);
      vec3 ambient = ambient_color * ambient_level;

      vec3 light_position = vec3(0,300,0);
      vec3 norm = normalize(v_normal);
      vec3 light_direction = light_position - v_position.xyz;
      float d = length(light_direction);
      float attenuation = 1.0 / ( 1.0 + 0.005 * d); // simplication of 1.0/(1.0 + c1*d + c2*d^2)
      light_direction = normalize(light_direction);
      vec3 diffuse_color = ambient_color;
      float diff = max(dot(norm, light_direction), 0.0);
      vec3 diffuse = diff * diffuse_color;

      float specular_strength = 0.5;
      vec3 view_direction = normalize(u_view_position - v_position.xyz);
      vec3 reflect_direction = reflect(-light_direction, norm);

      float spec = pow(max(dot(view_direction, reflect_direction), 0.0), 32);
      vec3 specular = specular_strength * spec * diffuse_color;

      if(v_color.a < 0.1) {
        o_color = vec4(vec3(0.0, 0.0, 1.0) * (ambient + ((diffuse + specular) * attenuation)), v_color.a);
      } else {
        o_color = vec4(v_color.rgb * (ambient + ((diffuse + specular) * attenuation)), v_color.a);
      }
  })SHADERSTR";

static const char * data_shader_default_vs = R"SHADERSTR(
  #version 330 core
  in vec3 i_position;
  in vec3 i_normal;
  in vec4 i_color;
  out vec4 v_color;
  out vec3 v_normal;
  out vec3 v_position;
  uniform mat4 u_mvp;
  void main() {
      v_color = i_color;
      v_normal = i_normal;
      v_position = i_position;
      gl_Position = u_mvp * vec4( i_position, 1.0 );
  })SHADERSTR";

static const char * data_shader_default_fs = R"SHADERSTR(
  #version 330 core
  in vec4 v_color;
  in vec3 v_normal;
  in vec3 v_position;
  out vec4 o_color;
  void main() {
      if(v_color.a < 0.1) {
        //discard;
        o_color = vec4(0.0, 0.0, 1.0, 1.0);
      } else {
        o_color = v_color;
      }
  })SHADERSTR";

}
