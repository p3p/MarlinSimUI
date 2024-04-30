#include "resources.h"
#include "resource_data.h"

#include <fstream>

namespace resource {
  std::map<std::filesystem::path, std::shared_ptr<Resource>> ResourceManager::s_loaded_resource = {};
  std::map<std::filesystem::path, std::shared_ptr<Resource>> ResourceManager::s_embedded_resource = {
    {{"null"}, std::make_shared<Resource>(nullptr)},
    {{"data/shaders/extrusion.gs"}, std::make_shared<Resource>(data_shader_extrusion_gs)},
    {{"data/shaders/extrusion.vs"}, std::make_shared<Resource>(data_shader_extrusion_vs)},
    {{"data/shaders/extrusion.fs"}, std::make_shared<Resource>(data_shader_extrusion_fs)},
    {{"data/shaders/default.vs"}, std::make_shared<Resource>(data_shader_default_vs)},
    {{"data/shaders/default.fs"}, std::make_shared<Resource>(data_shader_default_fs)},
  } ;

  FileResource::FileResource(std::filesystem::path path) : m_path{path} {
    load();
  }

  void FileResource::load() {
    std::ifstream file(m_path, std::ios::binary | std::ios::ate);
    m_modified = std::filesystem::last_write_time(m_path);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    m_data.reserve(size + 1);
    if(file.read(m_data.data(), size)) {
      m_buffer = m_data.data();
      m_data[size] = 0;
    } else {
      m_data.clear();
      m_buffer = nullptr;
    }
  }

  bool FileResource::reload() {
    if (m_modified < std::filesystem::last_write_time(m_path)) {
      load();
      return true;
    }
    return false;
  }

  const char* ResourceManager::get_as_cstr(std::filesystem::path path) {
    if (!path.empty() && std::filesystem::exists(std::filesystem::current_path() / path)) {
      if (s_loaded_resource.count(path)) {
        s_loaded_resource[path]->reload();
        return s_loaded_resource[path]->m_buffer;
      }
      auto file_buffer = std::make_shared<FileResource>(path);
      s_loaded_resource[path] = file_buffer;
      return file_buffer->m_buffer;
    }
    return s_embedded_resource.count(path) > 0 ? s_embedded_resource[path]->m_buffer : nullptr;
  }

}
