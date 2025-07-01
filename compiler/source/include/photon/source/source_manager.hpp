/**
 * @file source_manager.hpp
 * @brief Source file management system for the Photon compiler
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#pragma once

#include "photon/common/types.hpp"
#include "photon/memory/arena.hpp"
#include "photon/diagnostics/source_location.hpp"
#include <string_view>
#include <memory>
#include <unordered_map>
#include <vector>

namespace photon::source {

// Forward declarations
class SourceFile;
class SourceBuffer;

/// @brief Unique identifier for source files
using FileID = u32;

/// @brief Invalid file ID constant
constexpr FileID INVALID_FILE_ID = static_cast<FileID>(-1);

/**
 * @brief Error types for source management operations
 */
enum class SourceError : u32 {
    FileNotFound = 1000,
    AccessDenied = 1001,
    InvalidEncoding = 1002,
    FileTooLarge = 1003,
    MemoryMapFailed = 1004,
    InvalidUtf8 = 1005,
    CircularInclude = 1006,
    TooManyFiles = 1007
};

/**
 * @brief Source file metadata and content management
 * 
 * Represents a single source file with its content, encoding information,
 * and line mapping data for efficient source location resolution.
 */
class SourceFile {
public:
    /**
     * @brief File encoding types supported by the compiler
     */
    enum class Encoding : u8 {
        Utf8 = 0,           ///< UTF-8 encoding (default)
        Utf8WithBom = 1,    ///< UTF-8 with Byte Order Mark
        Ascii = 2           ///< ASCII encoding (subset of UTF-8)
    };

    /**
     * @brief Source file statistics
     */
    struct Statistics {
        usize byte_count = 0;        ///< Total bytes in file
        usize character_count = 0;   ///< Total Unicode characters
        usize line_count = 0;        ///< Total lines
        usize max_line_length = 0;   ///< Longest line in characters
        Encoding encoding = Encoding::Utf8;
    };

private:
    FileID file_id_;
    String filename_;
    String content_;
    Vec<usize> line_offsets_;  ///< Byte offsets for each line start
    Statistics stats_;
    bool is_memory_mapped_;
    std::unique_ptr<u8[]> mapped_memory_;
    usize mapped_size_;

public:
    /**
     * @brief Constructs a source file from content
     * @param file_id Unique file identifier
     * @param filename File path/name
     * @param content File content
     * @param arena Memory arena for allocations
     */
    SourceFile(FileID file_id, String filename, String content, 
               memory::MemoryArena<>& arena);

    /**
     * @brief Constructs a source file from memory-mapped data
     * @param file_id Unique file identifier  
     * @param filename File path/name
     * @param mapped_data Memory-mapped file data
     * @param size Size of mapped data
     * @param arena Memory arena for allocations
     */
    SourceFile(FileID file_id, String filename, 
               std::unique_ptr<u8[]> mapped_data, usize size,
               memory::MemoryArena<>& arena);

    // Non-copyable but movable
    SourceFile(const SourceFile&) = delete;
    auto operator=(const SourceFile&) -> SourceFile& = delete;
    SourceFile(SourceFile&&) noexcept = default;
    auto operator=(SourceFile&&) noexcept -> SourceFile& = default;

    /**
     * @brief Gets the file ID
     * @return Unique file identifier
     */
    [[nodiscard]] auto file_id() const noexcept -> FileID { return file_id_; }

    /**
     * @brief Gets the filename
     * @return File path/name
     */
    [[nodiscard]] auto filename() const noexcept -> StringView { return filename_; }

    /**
     * @brief Gets the file content
     * @return Complete file content as string view
     */
    [[nodiscard]] auto content() const noexcept -> StringView { return content_; }

    /**
     * @brief Gets file statistics
     * @return Statistics about the file
     */
    [[nodiscard]] auto statistics() const noexcept -> const Statistics& { return stats_; }

    /**
     * @brief Checks if file is memory-mapped
     * @return True if using memory mapping
     */
    [[nodiscard]] auto is_memory_mapped() const noexcept -> bool { return is_memory_mapped_; }

    /**
     * @brief Converts byte offset to line and column
     * @param offset Byte offset in file
     * @return Line and column information, or error if invalid offset
     */
    [[nodiscard]] auto offset_to_line_column(usize offset) const 
        -> Result<std::pair<u32, u32>, SourceError>;

    /**
     * @brief Converts line and column to byte offset
     * @param line Line number (1-based)
     * @param column Column number (1-based)
     * @return Byte offset, or error if invalid position
     */
    [[nodiscard]] auto line_column_to_offset(u32 line, u32 column) const 
        -> Result<usize, SourceError>;

    /**
     * @brief Gets content of a specific line
     * @param line_number Line number (1-based)
     * @return Line content without newline, or error if invalid line
     */
    [[nodiscard]] auto get_line_content(u32 line_number) const 
        -> Result<StringView, SourceError>;

    /**
     * @brief Gets content within a line range
     * @param start_line Start line (1-based, inclusive)
     * @param end_line End line (1-based, inclusive)
     * @return Vector of line contents
     */
    [[nodiscard]] auto get_line_range(u32 start_line, u32 end_line) const 
        -> Result<Vec<StringView>, SourceError>;

    /**
     * @brief Validates UTF-8 encoding of file content
     * @return True if valid UTF-8, false otherwise
     */
    [[nodiscard]] auto validate_utf8() const noexcept -> bool;

private:
    /**
     * @brief Builds line offset table for efficient line/column conversion
     */
    auto build_line_offsets() -> void;

    /**
     * @brief Computes file statistics
     */
    auto compute_statistics() -> void;

    /**
     * @brief Detects file encoding
     */
    auto detect_encoding() -> Encoding;
};

/**
 * @brief Interface for source file resolution
 * 
 * Abstracts the mechanism for locating and loading source files,
 * allowing for different loading strategies (filesystem, virtual, etc.).
 */
class ISourceResolver {
public:
    virtual ~ISourceResolver() = default;

    /**
     * @brief Resolves a file path to absolute path
     * @param path File path to resolve
     * @param current_directory Current working directory context
     * @return Resolved absolute path or error
     */
    [[nodiscard]] virtual auto resolve_path(StringView path, StringView current_directory) const
        -> Result<String, SourceError> = 0;

    /**
     * @brief Checks if a file exists and is readable
     * @param path File path to check
     * @return True if file exists and is accessible
     */
    [[nodiscard]] virtual auto file_exists(StringView path) const noexcept -> bool = 0;

    /**
     * @brief Gets file size in bytes
     * @param path File path
     * @return File size or error
     */
    [[nodiscard]] virtual auto file_size(StringView path) const 
        -> Result<usize, SourceError> = 0;

    /**
     * @brief Loads file content into memory
     * @param path File path
     * @return File content or error
     */
    [[nodiscard]] virtual auto load_file(StringView path) const 
        -> Result<String, SourceError> = 0;

    /**
     * @brief Memory-maps a file for efficient access
     * @param path File path
     * @return Mapped memory and size, or error
     */
    [[nodiscard]] virtual auto memory_map_file(StringView path) const 
        -> Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError> = 0;
};

/**
 * @brief Filesystem-based source resolver
 * 
 * Standard implementation that loads files from the filesystem
 * using memory mapping for large files and regular loading for small ones.
 */
class FilesystemResolver final : public ISourceResolver {
private:
    static constexpr usize MEMORY_MAP_THRESHOLD = 64 * 1024; // 64KB
    Vec<String> include_paths_;

public:
    /**
     * @brief Constructs filesystem resolver
     * @param include_paths Additional search paths for file resolution
     */
    explicit FilesystemResolver(Vec<String> include_paths = {});

    /**
     * @brief Adds an include path for file resolution
     * @param path Path to add to include search
     */
    auto add_include_path(String path) -> void;

    // ISourceResolver implementation
    [[nodiscard]] auto resolve_path(StringView path, StringView current_directory) const
        -> Result<String, SourceError> override;

    [[nodiscard]] auto file_exists(StringView path) const noexcept -> bool override;

    [[nodiscard]] auto file_size(StringView path) const 
        -> Result<usize, SourceError> override;

    [[nodiscard]] auto load_file(StringView path) const 
        -> Result<String, SourceError> override;

    [[nodiscard]] auto memory_map_file(StringView path) const 
        -> Result<std::pair<std::unique_ptr<u8[]>, usize>, SourceError> override;

private:
    /**
     * @brief Searches for file in include paths
     * @param filename File to search for
     * @return Full path if found, error otherwise
     */
    [[nodiscard]] auto search_include_paths(StringView filename) const 
        -> Result<String, SourceError>;
};

/**
 * @brief Configuration options for source manager
 */
struct SourceManagerOptions {
    usize max_file_size = 64 * 1024 * 1024;  ///< Maximum file size (64MB)
    usize max_total_size = 1024 * 1024 * 1024; ///< Maximum total loaded size (1GB)
    usize max_files = 10000;                 ///< Maximum number of files
    bool enable_memory_mapping = true;      ///< Use memory mapping for large files
    bool validate_utf8 = true;              ///< Validate UTF-8 encoding on load
    bool cache_line_offsets = true;         ///< Cache line offset tables
};

/**
 * @brief Central source file management system
 * 
 * Manages all source files used during compilation, providing efficient
 * access to file content, source locations, and file metadata. Supports
 * incremental loading and caching for optimal performance.
 */
class SourceManager {
private:
    memory::MemoryArena<>& arena_;
    std::unique_ptr<ISourceResolver> resolver_;
    std::unordered_map<String, FileID> filename_to_id_;
    std::unordered_map<FileID, std::unique_ptr<SourceFile>> files_;
    FileID next_file_id_;
    usize total_bytes_loaded_;
    usize total_files_loaded_;

    // Cache for recently accessed source locations
    struct LocationCache {
        FileID file_id = INVALID_FILE_ID;
        usize offset = 0;
        u32 line = 0;
        u32 column = 0;
    };
    mutable LocationCache location_cache_;

public:

    /**
     * @brief Constructs source manager
     * @param arena Memory arena for allocations
     * @param resolver Source file resolver (takes ownership)
     * @param options Configuration options
     */
    explicit SourceManager(memory::MemoryArena<>& arena,
                          std::unique_ptr<ISourceResolver> resolver = nullptr,
                          SourceManagerOptions options = {});

    // Non-copyable but movable
    SourceManager(const SourceManager&) = delete;
    auto operator=(const SourceManager&) -> SourceManager& = delete;
    SourceManager(SourceManager&&) noexcept = delete;
    auto operator=(SourceManager&&) noexcept -> SourceManager& = delete;

    /**
     * @brief Loads a source file
     * @param filename Path to source file
     * @return File ID or error
     */
    [[nodiscard]] auto load_file(StringView filename) 
        -> Result<FileID, SourceError>;

    /**
     * @brief Loads source from string content
     * @param filename Virtual filename for the content
     * @param content Source content
     * @return File ID or error
     */
    [[nodiscard]] auto load_from_string(StringView filename, String content)
        -> Result<FileID, SourceError>;

    /**
     * @brief Gets source file by ID
     * @param file_id File identifier
     * @return Source file reference or null if not found
     */
    [[nodiscard]] auto get_file(FileID file_id) const noexcept -> const SourceFile*;

    /**
     * @brief Gets source file by filename
     * @param filename File path
     * @return Source file reference or null if not found
     */
    [[nodiscard]] auto get_file(StringView filename) const noexcept -> const SourceFile*;

    /**
     * @brief Gets file ID for filename
     * @param filename File path
     * @return File ID or INVALID_FILE_ID if not found
     */
    [[nodiscard]] auto get_file_id(StringView filename) const noexcept -> FileID;

    /**
     * @brief Creates a source location from file ID and offset
     * @param file_id File identifier
     * @param offset Byte offset in file
     * @return Source location or error
     */
    [[nodiscard]] auto create_location(FileID file_id, usize offset) const
        -> Result<diagnostics::SourceLocation, SourceError>;

    /**
     * @brief Creates a source location from filename and position
     * @param filename File path
     * @param line Line number (1-based)
     * @param column Column number (1-based)
     * @return Source location or error
     */
    [[nodiscard]] auto create_location(StringView filename, u32 line, u32 column) const
        -> Result<diagnostics::SourceLocation, SourceError>;

    /**
     * @brief Resolves source location to line and column
     * @param location Source location to resolve
     * @return Line and column, or error if invalid
     */
    [[nodiscard]] auto resolve_location(const diagnostics::SourceLocation& location) const
        -> Result<std::pair<u32, u32>, SourceError>;

    /**
     * @brief Gets content at source location
     * @param location Source location
     * @param length Number of characters to extract
     * @return Content string view or error
     */
    [[nodiscard]] auto get_content_at(const diagnostics::SourceLocation& location, 
                                     usize length = 1) const
        -> Result<StringView, SourceError>;

    /**
     * @brief Gets line content for source location
     * @param location Source location
     * @return Line content or error
     */
    [[nodiscard]] auto get_line_content(const diagnostics::SourceLocation& location) const
        -> Result<StringView, SourceError>;

    /**
     * @brief Gets all loaded filenames
     * @return Vector of loaded file paths
     */
    [[nodiscard]] auto get_loaded_files() const -> Vec<String>;

    /**
     * @brief Gets source manager statistics
     * @return Statistics about loaded files and memory usage
     */
    struct Statistics {
        usize total_files = 0;
        usize total_bytes = 0;
        usize memory_mapped_files = 0;
        usize memory_mapped_bytes = 0;
        usize cached_files = 0;
    };

    [[nodiscard]] auto get_statistics() const noexcept -> Statistics;

    /**
     * @brief Clears all loaded files and resets state
     */
    auto clear() -> void;

    /**
     * @brief Preloads commonly used files for faster access
     * @param filenames Files to preload
     * @return Number of files successfully preloaded
     */
    auto preload_files(const Vec<String>& filenames) -> usize;

private:
    SourceManagerOptions options_;

    /**
     * @brief Validates file can be loaded based on size limits
     * @param size File size to check
     * @return True if file can be loaded
     */
    [[nodiscard]] auto can_load_file(usize size) const noexcept -> bool;

    /**
     * @brief Updates location cache for faster subsequent lookups
     * @param file_id File ID
     * @param offset Offset
     * @param line Line number
     * @param column Column number
     */
    auto update_location_cache(FileID file_id, usize offset, u32 line, u32 column) const -> void;
};

/**
 * @brief Factory for creating source managers with standard configurations
 */
class SourceManagerFactory {
public:
    /**
     * @brief Creates a standard filesystem-based source manager
     * @param arena Memory arena
     * @param include_paths Additional include search paths
     * @return Configured source manager
     */
    [[nodiscard]] static auto create_filesystem_manager(
        memory::MemoryArena<>& arena,
        Vec<String> include_paths = {}) -> std::unique_ptr<SourceManager>;

    /**
     * @brief Creates a source manager for testing with in-memory sources
     * @param arena Memory arena
     * @return Test-configured source manager
     */
    [[nodiscard]] static auto create_test_manager(
        memory::MemoryArena<>& arena) -> std::unique_ptr<SourceManager>;
};

} // namespace photon::source