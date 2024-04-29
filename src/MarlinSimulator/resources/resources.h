#pragma once

#include <filesystem>
#include <map>
#include <vector>

namespace resource {

class Resource {
public:
  Resource(const char * buffer = nullptr) : m_buffer { buffer } {}
  virtual ~Resource() {}
  const char * m_buffer = nullptr;
  virtual bool reload() { return false; };
};

class FileResource : public Resource {
public:
  FileResource(std::filesystem::path path);
  ~FileResource() { }

  void load();
  virtual bool reload() override;

  std::vector<char> m_data;
  std::filesystem::path m_path;
  std::filesystem::file_time_type m_modified;
};

class ResourceManager {
public:
  ResourceManager();
  const char* get_as_cstr(std::filesystem::path path);
private:
  std::map<std::filesystem::path, std::shared_ptr<Resource>> m_resource = {};
  std::map<std::filesystem::path, std::shared_ptr<Resource>> m_loaded_resource = {};
};

extern ResourceManager manager;

}
