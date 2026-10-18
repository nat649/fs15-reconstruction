#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>
#include "file_stream.h"
#include "buffered_file.h"

namespace fs15platform {
// Authored read-only OS transport around the recovered stream operations.
// Windows uses the inspected kernel APIs; Linux uses regular-file POSIX I/O.
// Does not claim Windows sharing/locking, UTF-16 namespace or full VFS parity.
class ReadOnlyFile {
public:
    explicit ReadOnlyFile(const std::filesystem::path& path);
    ~ReadOnlyFile();
    ReadOnlyFile(const ReadOnlyFile&)=delete;
    ReadOnlyFile& operator=(const ReadOnlyFile&)=delete;
    uint32_t read(void* destination,uint32_t count);
    bool seek(uint32_t position);
    uint32_t size() const;
    uint32_t tell() const;
    bool eof() const {return record_[0x10]!=0;}
    uint64_t full_size() const;
private:
    friend class BufferedReadOnlyFile;
    std::array<unsigned char,0x38> record_{};
    FS15_FileServices services_{};
};
// Authored ownership around the recovered default-disk buffered callbacks.
class BufferedReadOnlyFile {
public:
    explicit BufferedReadOnlyFile(const std::filesystem::path& path);
    uint32_t read(void* destination,uint32_t count);
    uint32_t seek(uint32_t offset,int origin);
    uint32_t status() const;
    uint64_t full_size() const {return file_.full_size();}
private:
    ReadOnlyFile file_;
    std::array<unsigned char,FS15_BUFFERED_FILE_BYTES> record_{};
};
std::vector<uint8_t> read_file_bounded(const std::filesystem::path& path,uint64_t maximum);
}
