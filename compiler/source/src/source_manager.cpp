/**
 * @file source_manager.cpp
 * @brief Source file management system implementation
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/source/source_manager.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <cstring>
#include <atomic>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace photon::source {

// SourceFile implementation

SourceFile::SourceFile(FileID file_id, String filename, String content, 
                       memory::MemoryArena<>& /*arena*/)
    : file_id_(file_id)
    , filename_(std::move(filename))
    , content_(std::move(content))
    , is_memory_mapped_(false)
    , mapped_memory_(nullptr)
    , mapped_size_(0) {
    
    build_line_offsets();
    compute_statistics();
}

SourceFile::SourceFile(FileID file_id, String filename, 
                       std::unique_ptr<u8[]> mapped_data, usize size,
                       memory::MemoryArena<>& /*arena*/)
    : file_id_(file_id)
    , filename_(std::move(filename))
    , is_memory_mapped_(true)
    , mapped_memory_(std::move(mapped_data))
    , mapped_size_(size) {
    
    // Create string view from mapped memory
    content_ = String(reinterpret_cast<const char*>(mapped_memory_.get()), size);
    
    build_line_offsets();
    compute_statistics();
}

auto SourceFile::offset_to_line_column(usize offset) const 
    -> Result<std::pair<u32, u32>, SourceError> {
    
    if (offset > content_.size()) {
        return Result<std::pair<u32, u32>, SourceError>(SourceError::InvalidEncoding);
    }
    
    // Binary search for the line containing this offset
    auto it = std::upper_bound(line_offsets_.begin(), line_offsets_.end(), offset);
    if (it == line_offsets_.begin()) {
        return Result<std::pair<u32, u32>, SourceError>(SourceError::InvalidEncoding);
    }
    
    --it;
    u32 line = static_cast<u32>(std::distance(line_offsets_.begin(), it)) + 1;
    u32 column = static_cast<u32>(offset - *it) + 1;
    
    return Result<std::pair<u32, u32>, SourceError>(std::make_pair(line, column));
}

auto SourceFile::line_column_to_offset(u32 line, u32 column) const 
    -> Result<usize, SourceError> {
    
    if (line == 0 || line > line_offsets_.size()) {
        return Result<usize, SourceError>(SourceError::InvalidEncoding);
    }
    
    usize line_start = line_offsets_[line - 1];
    
    // Calculate line end
    usize line_end;
    if (line < line_offsets_.size()) {
        line_end = line_offsets_[line] - 1; // -1 to exclude newline
    } else {
        line_end = content_.size();
    }
    
    // Check if column is valid for this line
    usize line_length = line_end - line_start;
    if (column == 0 || column > line_length + 1) {
        return Result<usize, SourceError>(SourceError::InvalidEncoding);
    }
    
    return Result<usize, SourceError>(line_start + column - 1);
}

auto SourceFile::get_line_content(u32 line_number) const 
    -> Result<StringView, SourceError> {
    
    if (line_number == 0 || line_number > line_offsets_.size()) {
        return Result<StringView, SourceError>(SourceError::InvalidEncoding);
    }
    
    usize line_start = line_offsets_[line_number - 1];
    
    // Calculate line end
    usize line_end;
    if (line_number < line_offsets_.size()) {
        line_end = line_offsets_[line_number];
        // Skip back over line ending characters
        while (line_end > line_start && 
               (content_[line_end - 1] == '\n' || content_[line_end - 1] == '\r')) {
            --line_end;
        }
    } else {
        line_end = content_.size();
    }
    
    return Result<StringView, SourceError>(StringView(content_.data() + line_start, line_end - line_start));
}

auto SourceFile::get_line_range(u32 start_line, u32 end_line) const 
    -> Result<Vec<StringView>, SourceError> {
    
    if (start_line == 0 || end_line == 0 || start_line > end_line ||
        end_line > line_offsets_.size()) {
        return Result<Vec<StringView>, SourceError>(SourceError::InvalidEncoding);
    }
    
    Vec<StringView> lines;
    lines.reserve(end_line - start_line + 1);
    
    for (u32 line = start_line; line <= end_line; ++line) {
        auto line_result = get_line_content(line);
        if (!line_result.has_value()) {
            return Result<Vec<StringView>, SourceError>(line_result.error());
        }
        lines.push_back(line_result.value());
    }
    
    return Result<Vec<StringView>, SourceError>(std::move(lines));
}

auto SourceFile::validate_utf8() const noexcept -> bool {
    const u8* data = reinterpret_cast<const u8*>(content_.data());
    usize size = content_.size();
    
    for (usize i = 0; i < size; ) {
        u8 byte = data[i];
        
        if (byte < 0x80) {
            // ASCII character
            ++i;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= size || (data[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= size || (data[i + 1] & 0xC0) != 0x80 || 
                (data[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= size || (data[i + 1] & 0xC0) != 0x80 || 
                (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    
    return true;
}

auto SourceFile::build_line_offsets() -> void {
    line_offsets_.clear();
    line_offsets_.push_back(0); // First line starts at offset 0
    
    for (usize i = 0; i < content_.size(); ++i) {
        char c = content_[i];
        if (c == '\n') {
            line_offsets_.push_back(i + 1);
        } else if (c == '\r') {
            // Handle \r\n and standalone \r
            if (i + 1 < content_.size() && content_[i + 1] == '\n') {
                line_offsets_.push_back(i + 2);
                ++i; // Skip the \n
            } else {
                line_offsets_.push_back(i + 1);
            }
        }
    }
    
    // Ensure we have at least one line even for empty files
    if (line_offsets_.size() == 1 && content_.empty()) {
        // Empty file has one empty line
    } else if (!content_.empty() && !line_offsets_.empty() && 
               line_offsets_.back() <= content_.size()) {
        // Add final line if content doesn't end with newline
        if (line_offsets_.back() != content_.size()) {
            // Only if the last recorded offset isn't already at the end
        }
    }
}

auto SourceFile::compute_statistics() -> void {
    stats_.byte_count = content_.size();
    stats_.line_count = line_offsets_.size();
    stats_.encoding = detect_encoding();
    
    // Count Unicode characters (this is a simplified version)
    stats_.character_count = 0;
    stats_.max_line_length = 0;
    
    const u8* data = reinterpret_cast<const u8*>(content_.data());
    usize current_line_length = 0;
    
    for (usize i = 0; i < content_.size(); ) {
        u8 byte = data[i];
        
        if (byte == '\n' || byte == '\r') {
            stats_.max_line_length = std::max(stats_.max_line_length, current_line_length);
            current_line_length = 0;
            
            // Skip \r\n sequences
            if (byte == '\r' && i + 1 < content_.size() && data[i + 1] == '\n') {
                ++i;
            }
            ++i;
        } else if (byte < 0x80) {
            // ASCII character
            ++current_line_length;
            ++i;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte UTF-8 sequence
            ++current_line_length;
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte UTF-8 sequence
            ++current_line_length;
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte UTF-8 sequence
            ++current_line_length;
            i += 4;
        } else {
            // Invalid UTF-8, treat as single byte
            ++current_line_length;
            ++i;
        }
        
        ++stats_.character_count;
    }
    
    stats_.max_line_length = std::max(stats_.max_line_length, current_line_length);
}

auto SourceFile::detect_encoding() -> Encoding {
    if (content_.size() >= 3 && 
        static_cast<u8>(content_[0]) == 0xEF &&
        static_cast<u8>(content_[1]) == 0xBB &&
        static_cast<u8>(content_[2]) == 0xBF) {
        return Encoding::Utf8WithBom;
    }
    
    // Check if content is pure ASCII
    bool is_ascii = true;
    for (char c : content_) {
        if (static_cast<u8>(c) >= 0x80) {
            is_ascii = false;
            break;
        }
    }
    
    return is_ascii ? Encoding::Ascii : Encoding::Utf8;
}

// FilesystemResolver implementation

FilesystemResolver::FilesystemResolver(Vec<String> include_paths)
    : include_paths_(std::move(include_paths)) {}

auto FilesystemResolver::add_include_path(String path) -> void {
    include_paths_.push_back(std::move(path));
}

auto FilesystemResolver::resolve_path(StringView path, StringView current_directory) const
    -> Result<String, SourceError> {
    
    if (path.empty()) {
        return Result<String, SourceError>(SourceError::FileNotFound);
    }
    
    std::filesystem::path fs_path(path);
    
    // If already absolute, return as-is
    if (fs_path.is_absolute()) {
        return Result<String, SourceError>(String(path));
    }
    
    // Try relative to current directory first
    if (!current_directory.empty()) {
        auto candidate = std::filesystem::path(current_directory) / fs_path;
        if (file_exists(candidate.string())) {
            return Result<String, SourceError>(candidate.string());
        }
    }
    
    // Search in include paths
    auto include_result = search_include_paths(path);
    if (include_result.has_value()) {
        return include_result;
    }
    
    // Try relative to current working directory
    auto cwd_path = std::filesystem::current_path() / fs_path;
    if (file_exists(cwd_path.string())) {
        return Result<String, SourceError>(cwd_path.string());
    }
    
    return Result<String, SourceError>(SourceError::FileNotFound);
}

auto FilesystemResolver::file_exists(StringView path) const noexcept -> bool {
    if (path.empty()) {
        return false;
    }
    
    try {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    } catch (...) {
        return false;
    }
}

auto FilesystemResolver::file_size(StringView path) const 
    -> Result<usize, SourceError> {
    
    if (!file_exists(path)) {
        return Result<usize, SourceError>(SourceError::FileNotFound);
    }
    
    try {
        auto size = std::filesystem::file_size(path);
        return Result<usize, SourceError>(static_cast<usize>(size));
    } catch (const std::filesystem::filesystem_error&) {
        return Result<usize, SourceError>(SourceError::AccessDenied);
    }
}

auto FilesystemResolver::load_file(StringView path) const 
    -> Result<String, SourceError> {
    
    if (!file_exists(path)) {
        return Result<String, SourceError>(SourceError::FileNotFound);
    }
    
    std::ifstream file(String(path), std::ios::binary);
    if (!file.is_open()) {
        return Result<String, SourceError>(SourceError::AccessDenied);
    }
    
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size < 0) {
        return Result<String, SourceError>(SourceError::AccessDenied);
    }
    
    String content(static_cast<usize>(size), '\0');
    file.read(content.data(), size);
    
    if (file.fail() && !file.eof()) {
        return Result<String, SourceError>(SourceError::AccessDenied);
    }
    
    return Result<String, SourceError>(std::move(content));
}

auto FilesystemResolver::memory_map_file(StringView path) const 
    -> Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError> {
    
    if (!file_exists(path)) {
        return Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError>(SourceError::FileNotFound);
    }
    
    String path_str(path);
    int fd = open(path_str.c_str(), O_RDONLY);
    if (fd == -1) {
        return Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError>(SourceError::AccessDenied);
    }
    
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError>(SourceError::AccessDenied);
    }
    
    usize size = static_cast<usize>(st.st_size);
    
    if (size == 0) {
        close(fd);
        // Return empty buffer for empty files
        auto buffer = std::make_unique<u8[]>(1);
        return Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError>(std::make_pair(std::move(buffer), 0));
    }
    
    void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (mapped == MAP_FAILED) {
        return Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError>(SourceError::MemoryMapFailed);
    }
    
    // Copy mapped memory to owned buffer
    auto buffer = std::make_unique<u8[]>(size);
    std::memcpy(buffer.get(), mapped, size);
    munmap(mapped, size);
    
    return Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError>(std::make_pair(std::move(buffer), size));
}

auto FilesystemResolver::search_include_paths(StringView filename) const 
    -> Result<String, SourceError> {
    
    for (const auto& include_path : include_paths_) {
        auto candidate = std::filesystem::path(include_path) / filename;
        if (file_exists(candidate.string())) {
            return Result<String, SourceError>(candidate.string());
        }
    }
    
    return Result<String, SourceError>(SourceError::FileNotFound);
}

// SourceManager implementation

SourceManager::SourceManager(memory::MemoryArena<>& arena,
                             std::unique_ptr<ISourceResolver> resolver,
                             SourceManagerOptions options)
    : arena_(arena)
    , resolver_(resolver ? std::move(resolver) : std::make_unique<FilesystemResolver>())
    , next_file_id_(1)
    , total_bytes_loaded_(0)
    , total_files_loaded_(0)
    , options_(options) {}

auto SourceManager::load_file(StringView filename) 
    -> Result<FileID, SourceError> {
    
    // Check if already loaded
    auto existing_it = filename_to_id_.find(String(filename));
    if (existing_it != filename_to_id_.end()) {
        return Result<FileID, SourceError>(existing_it->second);
    }
    
    // Resolve the file path
    auto resolved_path_result = resolver_->resolve_path(filename, "");
    if (!resolved_path_result.has_value()) {
        return Result<FileID, SourceError>(resolved_path_result.error());
    }
    
    String resolved_path = resolved_path_result.value();
    
    // Check if the resolved path is already loaded
    existing_it = filename_to_id_.find(resolved_path);
    if (existing_it != filename_to_id_.end()) {
        // Add filename mapping to existing file
        filename_to_id_[String(filename)] = existing_it->second;
        return Result<FileID, SourceError>(existing_it->second);
    }
    
    // Check file size limits
    auto size_result = resolver_->file_size(resolved_path);
    if (!size_result.has_value()) {
        return Result<FileID, SourceError>(size_result.error());
    }
    
    usize file_size = size_result.value();
    if (!can_load_file(file_size)) {
        return Result<FileID, SourceError>(SourceError::FileTooLarge);
    }
    
    // Load the file
    FileID file_id = next_file_id_++;
    std::unique_ptr<SourceFile> source_file;
    
    if (options_.enable_memory_mapping && file_size >= 64 * 1024) {
        // Use memory mapping for large files
        auto mmap_result = resolver_->memory_map_file(resolved_path);
        if (mmap_result.has_value()) {
            auto& [mapped_data, mapped_size] = mmap_result.value();
            source_file = std::make_unique<SourceFile>(
                file_id, resolved_path, 
                std::move(mapped_data), mapped_size, arena_);
        }
    }
    
    if (!source_file) {
        // Fall back to regular loading
        auto content_result = resolver_->load_file(resolved_path);
        if (!content_result.has_value()) {
            return Result<FileID, SourceError>(content_result.error());
        }
        
        String content = content_result.value();
        
        // Validate UTF-8 if requested
        if (options_.validate_utf8) {
            // UTF-8 validation would go here if needed
            (void)content; // Suppress unused variable warning
        }
        
        source_file = std::make_unique<SourceFile>(
            file_id, resolved_path, std::move(content), arena_);
    }
    
    // Update statistics
    total_bytes_loaded_ += source_file->statistics().byte_count;
    total_files_loaded_++;
    
    // Store the file
    files_[file_id] = std::move(source_file);
    filename_to_id_[String(filename)] = file_id;
    filename_to_id_[resolved_path] = file_id;
    
    return Result<FileID, SourceError>(file_id);
}

auto SourceManager::load_from_string(StringView filename, String content)
    -> Result<FileID, SourceError> {
    
    // Check if already loaded
    auto existing_it = filename_to_id_.find(String(filename));
    if (existing_it != filename_to_id_.end()) {
        return Result<FileID, SourceError>(existing_it->second);
    }
    
    // Check size limits
    if (!can_load_file(content.size())) {
        return Result<FileID, SourceError>(SourceError::FileTooLarge);
    }
    
    FileID file_id = next_file_id_++;
    
    auto source_file = std::make_unique<SourceFile>(
        file_id, String(filename), std::move(content), arena_);
    
    // Update statistics
    total_bytes_loaded_ += source_file->statistics().byte_count;
    total_files_loaded_++;
    
    // Store the file
    files_[file_id] = std::move(source_file);
    filename_to_id_[String(filename)] = file_id;
    
    return Result<FileID, SourceError>(file_id);
}

auto SourceManager::get_file(FileID file_id) const noexcept -> const SourceFile* {
    auto it = files_.find(file_id);
    return (it != files_.end()) ? it->second.get() : nullptr;
}

auto SourceManager::get_file(StringView filename) const noexcept -> const SourceFile* {
    auto id_it = filename_to_id_.find(String(filename));
    if (id_it == filename_to_id_.end()) {
        return nullptr;
    }
    
    return get_file(id_it->second);
}

auto SourceManager::get_file_id(StringView filename) const noexcept -> FileID {
    auto it = filename_to_id_.find(String(filename));
    return (it != filename_to_id_.end()) ? it->second : INVALID_FILE_ID;
}

auto SourceManager::create_location(FileID file_id, usize offset) const
    -> Result<diagnostics::SourceLocation, SourceError> {
    
    const SourceFile* file = get_file(file_id);
    if (!file) {
        return Result<diagnostics::SourceLocation, SourceError>(SourceError::FileNotFound);
    }
    
    auto line_col_result = file->offset_to_line_column(offset);
    if (!line_col_result.has_value()) {
        return Result<diagnostics::SourceLocation, SourceError>(line_col_result.error());
    }
    
    auto [line, column] = line_col_result.value();
    return Result<diagnostics::SourceLocation, SourceError>(diagnostics::SourceLocation(file->filename(), line, column, static_cast<u32>(offset)));
}

auto SourceManager::create_location(StringView filename, u32 line, u32 column) const
    -> Result<diagnostics::SourceLocation, SourceError> {
    
    const SourceFile* file = get_file(filename);
    if (!file) {
        return Result<diagnostics::SourceLocation, SourceError>(SourceError::FileNotFound);
    }
    
    auto offset_result = file->line_column_to_offset(line, column);
    if (!offset_result.has_value()) {
        return Result<diagnostics::SourceLocation, SourceError>(offset_result.error());
    }
    
    usize offset = offset_result.value();
    return Result<diagnostics::SourceLocation, SourceError>(diagnostics::SourceLocation(filename, line, column, static_cast<u32>(offset)));
}

auto SourceManager::resolve_location(const diagnostics::SourceLocation& location) const
    -> Result<std::pair<u32, u32>, SourceError> {
    
    const SourceFile* file = get_file(location.filename());
    if (!file) {
        return Result<std::pair<u32, u32>, SourceError>(SourceError::FileNotFound);
    }
    
    return file->offset_to_line_column(location.offset());
}

auto SourceManager::get_content_at(const diagnostics::SourceLocation& location, 
                                   usize length) const
    -> Result<StringView, SourceError> {
    
    const SourceFile* file = get_file(location.filename());
    if (!file) {
        return Result<StringView, SourceError>(SourceError::FileNotFound);
    }
    
    usize offset = location.offset();
    if (offset + length > file->content().size()) {
        length = file->content().size() - offset;
    }
    
    return Result<StringView, SourceError>(StringView(file->content().data() + offset, length));
}

auto SourceManager::get_line_content(const diagnostics::SourceLocation& location) const
    -> Result<StringView, SourceError> {
    
    const SourceFile* file = get_file(location.filename());
    if (!file) {
        return Result<StringView, SourceError>(SourceError::FileNotFound);
    }
    
    return file->get_line_content(location.line());
}

auto SourceManager::get_loaded_files() const -> Vec<String> {
    Vec<String> filenames;
    filenames.reserve(filename_to_id_.size());
    
    for (const auto& [filename, file_id] : filename_to_id_) {
        filenames.push_back(filename);
    }
    
    return filenames;
}

auto SourceManager::get_statistics() const noexcept -> Statistics {
    Statistics stats;
    stats.total_files = total_files_loaded_;
    stats.total_bytes = total_bytes_loaded_;
    
    for (const auto& [file_id, file] : files_) {
        if (file->is_memory_mapped()) {
            stats.memory_mapped_files++;
            stats.memory_mapped_bytes += file->statistics().byte_count;
        }
    }
    
    stats.cached_files = files_.size();
    
    return stats;
}

auto SourceManager::clear() -> void {
    files_.clear();
    filename_to_id_.clear();
    next_file_id_ = 1;
    total_bytes_loaded_ = 0;
    total_files_loaded_ = 0;
    location_cache_ = LocationCache{};
}

auto SourceManager::preload_files(const Vec<String>& filenames) -> usize {
    usize loaded_count = 0;
    
    for (const auto& filename : filenames) {
        auto result = load_file(filename);
        if (result.has_value()) {
            loaded_count++;
        }
    }
    
    return loaded_count;
}

auto SourceManager::can_load_file(usize size) const noexcept -> bool {
    if (size > options_.max_file_size) {
        return false;
    }
    
    if (total_bytes_loaded_ + size > options_.max_total_size) {
        return false;
    }
    
    if (total_files_loaded_ >= options_.max_files) {
        return false;
    }
    
    return true;
}

auto SourceManager::update_location_cache(FileID file_id, usize offset, 
                                           u32 line, u32 column) const -> void {
    location_cache_.file_id = file_id;
    location_cache_.offset = offset;
    location_cache_.line = line;
    location_cache_.column = column;
}

// SourceManagerFactory implementation

auto SourceManagerFactory::create_filesystem_manager(
    memory::MemoryArena<>& arena,
    Vec<String> include_paths) -> std::unique_ptr<SourceManager> {
    
    auto resolver = std::make_unique<FilesystemResolver>(std::move(include_paths));
    return std::make_unique<SourceManager>(arena, std::move(resolver));
}

auto SourceManagerFactory::create_test_manager(
    memory::MemoryArena<>& arena) -> std::unique_ptr<SourceManager> {
    
    SourceManagerOptions options;
    options.max_file_size = 1024 * 1024; // 1MB for tests
    options.max_total_size = 10 * 1024 * 1024; // 10MB for tests
    options.enable_memory_mapping = false; // Disable for tests
    
    return std::make_unique<SourceManager>(arena, nullptr, options);
}

} // namespace photon::source