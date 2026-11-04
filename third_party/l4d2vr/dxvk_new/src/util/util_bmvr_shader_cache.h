#pragma once
#include <windows.h>
#include "sha1/sha1_util.h"
#include "util_env.h"
#include "util_string.h"
#include "log/log.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace dxvk {
// Cache only the deterministic compatibility optimizer output, never Vulkan
// handles. Revision and every input SPIR-V word/specialization value are hashed.
class BmvrShaderCache {
public:
  static std::filesystem::path directory() {
    auto name = env::getEnvVar("BMVR_SHADER_CACHE_PATH");
    if (name.empty()) name = "VR/cache";
    return std::filesystem::path(str::topath(name.c_str()));
  }
  static std::string key(const uint32_t* code, size_t count,
      const std::unordered_map<uint32_t, std::vector<uint32_t>>& values) {
    std::vector<uint32_t> input{12, uint32_t(count)};
    input.insert(input.end(), code, code + count);
    // Canonical order: unordered_map iteration must not affect cache identity.
    std::map<uint32_t,std::vector<uint32_t>> sorted(values.begin(),values.end());
    input.push_back(uint32_t(sorted.size()));
    for (const auto& entry : sorted) {
      input.push_back(entry.first); input.push_back(uint32_t(entry.second.size()));
      input.insert(input.end(),entry.second.begin(),entry.second.end());
    }
    return Sha1Hash::compute(input.data(), input.size()*4).toString();
  }
  bool load(const std::string& key, std::vector<uint32_t>& words) {
    {
      std::lock_guard<std::mutex> lock(m_lock);
      auto entry=m_words.find(key);
      if (entry!=m_words.end()) { words=entry->second; note(m_memoryHits,"memory"); return true; }
    }
    const auto path=directory()/(key+".spv12");
    std::ifstream file(path,std::ios::binary);
    uint32_t magic=0,count=0; Sha1Digest digest{};
    file.read(reinterpret_cast<char*>(&magic),4);
    file.read(reinterpret_cast<char*>(&count),4);
    file.read(reinterpret_cast<char*>(digest.data()),digest.size());
    if (!file || magic!=0x4253500c || count<5 || count>2*1024*1024) return false;
    std::error_code error;
    if (std::filesystem::file_size(path,error)!=28+size_t(count)*4 || error) return false;
    words.resize(count);
    file.read(reinterpret_cast<char*>(words.data()),size_t(count)*4);
    if (!file || words[0]!=0x07230203 || Sha1Hash::compute(words.data(),words.size()*4)!=Sha1Hash(digest)) {
      words.clear(); return false;
    }
    remember(key,words); note(m_diskHits,"disk"); return true;
  }
  void store(const std::string& key, const std::vector<uint32_t>& words) {
    remember(key,words); note(m_compiled,"compiled");
    std::error_code error; const auto dir=directory();
    std::filesystem::create_directories(dir,error);
    if (error) { warnWrite(); return; }
    const auto target=dir/(key+".spv12");
    const auto temp=dir/(key+"."+std::to_string(GetCurrentProcessId())+"."+
      std::to_string(GetCurrentThreadId())+".tmp");
    {
      std::ofstream file(temp,std::ios::binary|std::ios::trunc);
      const uint32_t magic=0x4253500c,count=uint32_t(words.size());
      const auto digest=Sha1Hash::compute(words.data(),words.size()*4).digest();
      file.write(reinterpret_cast<const char*>(&magic),4);
      file.write(reinterpret_cast<const char*>(&count),4);
      file.write(reinterpret_cast<const char*>(digest.data()),digest.size());
      file.write(reinterpret_cast<const char*>(words.data()),words.size()*4);
      file.close();
      if (!file) { std::filesystem::remove(temp,error); warnWrite(); return; }
    }
    // Atomic replacement also repairs a corrupt entry rejected by load().
    // Readers see the complete previous or complete new file.
    if (!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING)) {
      std::filesystem::remove(temp,error); warnWrite();
    }
  }
private:
  void remember(const std::string& key,const std::vector<uint32_t>& words) {
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_words.find(key)!=m_words.end()) return;
    const size_t bytes=words.size()*4;
    if (bytes>64*1024*1024) return;
    if (m_bytes+bytes>64*1024*1024) {m_words.clear();m_bytes=0;}
    m_words.emplace(key,words);m_bytes+=bytes;
  }
  static void note(std::atomic<unsigned>& counter,const char* kind) {
    const unsigned n=counter.fetch_add(1,std::memory_order_relaxed)+1;
    if (n==1 || n==128 || n==512 || n==2048)
      Logger::warn(str::format("[BMVR cache v12] ",kind," entries=",n));
  }
  static void warnWrite() {
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) Logger::warn("[BMVR cache v12] disk cache unavailable; using memory cache");
  }
  std::mutex m_lock;
  std::unordered_map<std::string,std::vector<uint32_t>> m_words;
  size_t m_bytes=0;
  std::atomic<unsigned> m_memoryHits{0},m_diskHits{0},m_compiled{0};
};
}
