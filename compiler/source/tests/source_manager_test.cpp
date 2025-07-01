/**
 * @file source_manager_test.cpp
 * @brief Comprehensive test suite for source management system
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/source/source_manager.hpp"
#include "photon/memory/arena.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>

using namespace photon::source;
using namespace photon::memory;

namespace {
    class SourceManagerTest : public ::testing::Test {
    protected:
        void SetUp() override {
            arena_ = std::make_unique<MemoryArena<> >();

            // Create test directory and files
            test_dir_ = std::filesystem::temp_directory_path() / "photon_test";
            std::filesystem::create_directories(test_dir_);

            // Create test files
            create_test_file("simple.pht", "fn main() {\n    emit(\"hello\")\n}");
            create_test_file("empty.pht", "");
            create_test_file("utf8.pht", "fn test() {\n    let π = 3.14159\n    let emoji = \"🚀\"\n}");
            create_test_file("large.pht", generate_large_content(100000));
            create_test_file("windows_endings.pht", "line1\r\nline2\r\nline3\r\n");
            create_test_file("mixed_endings.pht", "line1\nline2\r\nline3\rline4\n");
        }

        void TearDown() override {
            // Clean up test files
            std::filesystem::remove_all(test_dir_);
        }

        void create_test_file(const std::string &filename, const std::string &content) {
            auto path = test_dir_ / filename;
            std::ofstream file(path);
            file << content;
            file.close();
        }

        std::string generate_large_content(size_t size) {
            std::string content;
            content.reserve(size);
            for (size_t i = 0; i < size; ++i) {
                content += (i % 100 == 99) ? '\n' : 'a';
            }
            return content;
        }

        std::string get_test_file_path(const std::string &filename) {
            return (test_dir_ / filename).string();
        }

        std::unique_ptr<MemoryArena<> > arena_;
        std::filesystem::path test_dir_;
    };
} // anonymous namespace

// SourceFile tests

TEST_F(SourceManagerTest, SourceFileConstructionFromContent) {
    std::string content = "fn main() {\n    emit(\"test\")\n}";
    SourceFile file(1, "test.pht", content, *arena_);

    EXPECT_EQ(file.file_id(), 1u);
    EXPECT_EQ(file.filename(), "test.pht");
    EXPECT_EQ(file.content(), content);
    EXPECT_FALSE(file.is_memory_mapped());

    const auto &stats = file.statistics();
    EXPECT_EQ(stats.byte_count, content.size());
    EXPECT_EQ(stats.line_count, 3u); // 3 lines including empty line at end
    EXPECT_GT(stats.character_count, 0u);
}

TEST_F(SourceManagerTest, SourceFileEmptyContent) {
    SourceFile file(1, "empty.pht", "", *arena_);

    EXPECT_EQ(file.content(), "");
    const auto &stats = file.statistics();
    EXPECT_EQ(stats.byte_count, 0u);
    EXPECT_EQ(stats.line_count, 1u); // Empty file has 1 line
    EXPECT_EQ(stats.character_count, 0u);
}

TEST_F(SourceManagerTest, SourceFileOffsetToLineColumn) {
    std::string content = "line1\nline2\nline3";
    SourceFile file(1, "test.pht", content, *arena_);

    // Test offset at start of file
    auto result = file.offset_to_line_column(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().first, 1u); // line
    EXPECT_EQ(result.value().second, 1u); // column

    // Test offset at start of second line
    result = file.offset_to_line_column(6); // After "line1\n"
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().first, 2u);
    EXPECT_EQ(result.value().second, 1u);

    // Test offset in middle of line
    result = file.offset_to_line_column(2); // "n" in "line1"
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().first, 1u);
    EXPECT_EQ(result.value().second, 3u);

    // Test invalid offset
    result = file.offset_to_line_column(1000);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SourceError::InvalidEncoding); // Using existing error for now
}

TEST_F(SourceManagerTest, SourceFileLineColumnToOffset) {
    std::string content = "line1\nline2\nline3";
    SourceFile file(1, "test.pht", content, *arena_);

    // Test start of file
    auto result = file.line_column_to_offset(1, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0u);

    // Test start of second line
    result = file.line_column_to_offset(2, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 6u);

    // Test middle of first line
    result = file.line_column_to_offset(1, 3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 2u);

    // Test invalid line
    result = file.line_column_to_offset(10, 1);
    EXPECT_FALSE(result.has_value());

    // Test invalid column
    result = file.line_column_to_offset(1, 100);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SourceManagerTest, SourceFileGetLineContent) {
    std::string content = "first line\nsecond line\nthird line";
    SourceFile file(1, "test.pht", content, *arena_);

    // Test valid lines
    auto result = file.get_line_content(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "first line");

    result = file.get_line_content(2);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "second line");

    result = file.get_line_content(3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "third line");

    // Test invalid line
    result = file.get_line_content(0);
    EXPECT_FALSE(result.has_value());

    result = file.get_line_content(10);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SourceManagerTest, SourceFileGetLineRange) {
    std::string content = "line1\nline2\nline3\nline4";
    SourceFile file(1, "test.pht", content, *arena_);

    // Test valid range
    auto result = file.get_line_range(2, 3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0], "line2");
    EXPECT_EQ(result.value()[1], "line3");

    // Test single line range
    result = file.get_line_range(1, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0], "line1");

    // Test invalid range
    result = file.get_line_range(3, 1); // end < start
    EXPECT_FALSE(result.has_value());

    result = file.get_line_range(0, 2); // invalid start
    EXPECT_FALSE(result.has_value());

    result = file.get_line_range(1, 10); // invalid end
    EXPECT_FALSE(result.has_value());
}

TEST_F(SourceManagerTest, SourceFileUtf8Validation) {
    // Valid UTF-8
    std::string valid_utf8 = "Hello 世界 🌍";
    SourceFile valid_file(1, "valid.pht", valid_utf8, *arena_);
    EXPECT_TRUE(valid_file.validate_utf8());

    // ASCII (subset of UTF-8)
    std::string ascii = "Hello World";
    SourceFile ascii_file(2, "ascii.pht", ascii, *arena_);
    EXPECT_TRUE(ascii_file.validate_utf8());

    // Invalid UTF-8 sequences would need to be tested with binary data
    // For now, test that validation doesn't crash on normal content
    std::string normal = "fn main() { }";
    SourceFile normal_file(3, "normal.pht", normal, *arena_);
    EXPECT_TRUE(normal_file.validate_utf8());
}

TEST_F(SourceManagerTest, SourceFileWindowsLineEndings) {
    std::string content = "line1\r\nline2\r\nline3\r\n";
    SourceFile file(1, "windows.pht", content, *arena_);

    const auto &stats = file.statistics();
    EXPECT_EQ(stats.line_count, 4u); // 3 lines + empty line at end

    // Test line content extraction (should handle \r\n properly)
    auto result = file.get_line_content(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "line1");

    result = file.get_line_content(2);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "line2");
}

TEST_F(SourceManagerTest, SourceFileMixedLineEndings) {
    std::string content = "line1\nline2\r\nline3\rline4\n";
    SourceFile file(1, "mixed.pht", content, *arena_);

    const auto &stats = file.statistics();
    EXPECT_GT(stats.line_count, 3u); // Should handle mixed endings

    // Should handle the first line correctly
    auto result = file.get_line_content(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "line1");
}

// FilesystemResolver tests

TEST_F(SourceManagerTest, FilesystemResolverFileExists) {
    FilesystemResolver resolver;

    std::string test_file = get_test_file_path("simple.pht");
    EXPECT_TRUE(resolver.file_exists(test_file));

    EXPECT_FALSE(resolver.file_exists("nonexistent_file.pht"));
    EXPECT_FALSE(resolver.file_exists(""));
}

TEST_F(SourceManagerTest, FilesystemResolverFileSize) {
    FilesystemResolver resolver;

    std::string test_file = get_test_file_path("simple.pht");
    auto result = resolver.file_size(test_file);
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0u);

    // Test empty file
    std::string empty_file = get_test_file_path("empty.pht");
    result = resolver.file_size(empty_file);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0u);

    // Test nonexistent file
    result = resolver.file_size("nonexistent.pht");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SourceError::FileNotFound);
}

TEST_F(SourceManagerTest, FilesystemResolverLoadFile) {
    FilesystemResolver resolver;

    std::string test_file = get_test_file_path("simple.pht");
    auto result = resolver.load_file(test_file);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "fn main() {\n    emit(\"hello\")\n}");

    // Test empty file
    std::string empty_file = get_test_file_path("empty.pht");
    result = resolver.load_file(empty_file);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");

    // Test nonexistent file
    result = resolver.load_file("nonexistent.pht");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SourceError::FileNotFound);
}

TEST_F(SourceManagerTest, FilesystemResolverMemoryMapFile) {
    FilesystemResolver resolver;

    // Test large file that should be memory mapped
    std::string large_file = get_test_file_path("large.pht");
    auto result = resolver.memory_map_file(large_file);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().first.get(), nullptr);
    EXPECT_GT(result.value().second, 0u);

    // Test small file
    std::string small_file = get_test_file_path("simple.pht");
    result = resolver.memory_map_file(small_file);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().first.get(), nullptr);
    EXPECT_GT(result.value().second, 0u);

    // Test nonexistent file
    result = resolver.memory_map_file("nonexistent.pht");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SourceError::FileNotFound);
}

TEST_F(SourceManagerTest, FilesystemResolverResolvePath) {
    FilesystemResolver resolver;

    // Test absolute path
    std::string test_file = get_test_file_path("simple.pht");
    auto result = resolver.resolve_path(test_file, "");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), test_file);

    // Test relative path resolution
    result = resolver.resolve_path("simple.pht", test_dir_.string());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), test_file);

    // Test empty path
    result = resolver.resolve_path("", "");
    EXPECT_FALSE(result.has_value());
}

TEST_F(SourceManagerTest, FilesystemResolverIncludePaths) {
    std::vector<std::string> include_paths = {test_dir_.string()};
    FilesystemResolver resolver(include_paths);

    // Should find file in include path
    auto result = resolver.resolve_path("simple.pht", "");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), get_test_file_path("simple.pht"));

    // Add another include path
    resolver.add_include_path("/nonexistent/path");

    // Should still find file in first include path
    result = resolver.resolve_path("simple.pht", "");
    ASSERT_TRUE(result.has_value());
}

// SourceManager tests

TEST_F(SourceManagerTest, SourceManagerConstruction) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    auto stats = manager.get_statistics();
    EXPECT_EQ(stats.total_files, 0u);
    EXPECT_EQ(stats.total_bytes, 0u);
    EXPECT_EQ(stats.memory_mapped_files, 0u);
    EXPECT_EQ(stats.cached_files, 0u);
}

TEST_F(SourceManagerTest, SourceManagerLoadFile) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string test_file = get_test_file_path("simple.pht");
    auto result = manager.load_file(test_file);
    ASSERT_TRUE(result.has_value());

    FileID file_id = result.value();
    EXPECT_NE(file_id, INVALID_FILE_ID);

    // Should be able to get the file
    const SourceFile *file = manager.get_file(file_id);
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->filename(), test_file);
    EXPECT_EQ(file->content(), "fn main() {\n    emit(\"hello\")\n}");

    // Statistics should be updated
    auto stats = manager.get_statistics();
    EXPECT_EQ(stats.total_files, 1u);
    EXPECT_GT(stats.total_bytes, 0u);
}

TEST_F(SourceManagerTest, SourceManagerLoadSameFileTwice) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string test_file = get_test_file_path("simple.pht");

    auto result1 = manager.load_file(test_file);
    ASSERT_TRUE(result1.has_value());

    auto result2 = manager.load_file(test_file);
    ASSERT_TRUE(result2.has_value());

    // Should return same file ID
    EXPECT_EQ(result1.value(), result2.value());

    // Should only count as one file
    auto stats = manager.get_statistics();
    EXPECT_EQ(stats.total_files, 1u);
}

TEST_F(SourceManagerTest, SourceManagerLoadFromString) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string content = "fn test() { emit(\"from string\") }";
    auto result = manager.load_from_string("virtual.pht", content);
    ASSERT_TRUE(result.has_value());

    FileID file_id = result.value();
    const SourceFile *file = manager.get_file(file_id);
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->filename(), "virtual.pht");
    EXPECT_EQ(file->content(), content);
}

TEST_F(SourceManagerTest, SourceManagerGetFileByFilename) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string test_file = get_test_file_path("simple.pht");
    auto result = manager.load_file(test_file);
    ASSERT_TRUE(result.has_value());

    // Should be able to get by filename
    const SourceFile *file = manager.get_file(test_file);
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->filename(), test_file);

    // Should return null for nonexistent file
    const SourceFile *null_file = manager.get_file("nonexistent.pht");
    EXPECT_EQ(null_file, nullptr);
}

TEST_F(SourceManagerTest, SourceManagerGetFileId) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string test_file = get_test_file_path("simple.pht");
    auto result = manager.load_file(test_file);
    ASSERT_TRUE(result.has_value());

    FileID expected_id = result.value();
    FileID actual_id = manager.get_file_id(test_file);
    EXPECT_EQ(actual_id, expected_id);

    // Should return invalid ID for nonexistent file
    FileID invalid_id = manager.get_file_id("nonexistent.pht");
    EXPECT_EQ(invalid_id, INVALID_FILE_ID);
}

TEST_F(SourceManagerTest, SourceManagerCreateLocationFromFileId) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string test_file = get_test_file_path("simple.pht");
    auto file_result = manager.load_file(test_file);
    ASSERT_TRUE(file_result.has_value());

    FileID file_id = file_result.value();
    auto loc_result = manager.create_location(file_id, 0);
    ASSERT_TRUE(loc_result.has_value());

    auto location = loc_result.value();
    EXPECT_EQ(location.filename(), test_file);
    EXPECT_EQ(location.offset(), 0u);

    // Test invalid file ID
    loc_result = manager.create_location(INVALID_FILE_ID, 0);
    EXPECT_FALSE(loc_result.has_value());
}

TEST_F(SourceManagerTest, SourceManagerCreateLocationFromFilename) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string test_file = get_test_file_path("simple.pht");
    auto file_result = manager.load_file(test_file);
    ASSERT_TRUE(file_result.has_value());

    auto loc_result = manager.create_location(test_file, 1, 1);
    ASSERT_TRUE(loc_result.has_value());

    auto location = loc_result.value();
    EXPECT_EQ(location.filename(), test_file);
    EXPECT_EQ(location.line(), 1u);
    EXPECT_EQ(location.column(), 1u);

    // Test nonexistent file
    loc_result = manager.create_location("nonexistent.pht", 1, 1);
    EXPECT_FALSE(loc_result.has_value());
}

TEST_F(SourceManagerTest, SourceManagerResolveLocation) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string content = "line1\nline2\nline3";
    auto file_result = manager.load_from_string("test.pht", content);
    ASSERT_TRUE(file_result.has_value());

    FileID file_id = file_result.value();
    auto loc_result = manager.create_location(file_id, 6); // Start of line2
    ASSERT_TRUE(loc_result.has_value());

    auto resolve_result = manager.resolve_location(loc_result.value());
    ASSERT_TRUE(resolve_result.has_value());
    EXPECT_EQ(resolve_result.value().first, 2u); // line
    EXPECT_EQ(resolve_result.value().second, 1u); // column
}

TEST_F(SourceManagerTest, SourceManagerGetContentAt) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string content = "fn main() {}";
    auto file_result = manager.load_from_string("test.pht", content);
    ASSERT_TRUE(file_result.has_value());

    FileID file_id = file_result.value();
    auto loc_result = manager.create_location(file_id, 0);
    ASSERT_TRUE(loc_result.has_value());

    auto content_result = manager.get_content_at(loc_result.value(), 2);
    ASSERT_TRUE(content_result.has_value());
    EXPECT_EQ(content_result.value(), "fn");

    // Test single character
    content_result = manager.get_content_at(loc_result.value(), 1);
    ASSERT_TRUE(content_result.has_value());
    EXPECT_EQ(content_result.value(), "f");
}

TEST_F(SourceManagerTest, SourceManagerGetLineContent) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string content = "first line\nsecond line\nthird line";
    auto file_result = manager.load_from_string("test.pht", content);
    ASSERT_TRUE(file_result.has_value());

    FileID file_id = file_result.value();
    auto loc_result = manager.create_location(file_id, 11); // Start of second line
    ASSERT_TRUE(loc_result.has_value());

    auto line_result = manager.get_line_content(loc_result.value());
    ASSERT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "second line");
}

TEST_F(SourceManagerTest, SourceManagerGetLoadedFiles) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    // Initially empty
    auto files = manager.get_loaded_files();
    EXPECT_TRUE(files.empty());

    // Load some files
    std::string test_file1 = get_test_file_path("simple.pht");
    std::string test_file2 = get_test_file_path("empty.pht");

    (void)manager.load_file(test_file1);
    (void)manager.load_from_string("virtual.pht", "content");
    (void)manager.load_file(test_file2);

    files = manager.get_loaded_files();
    EXPECT_EQ(files.size(), 3u);

    // Should contain all loaded files
    EXPECT_NE(std::find(files.begin(), files.end(), test_file1), files.end());
    EXPECT_NE(std::find(files.begin(), files.end(), test_file2), files.end());
    EXPECT_NE(std::find(files.begin(), files.end(), "virtual.pht"), files.end());
}

TEST_F(SourceManagerTest, SourceManagerClear) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    // Load some files
    std::string test_file = get_test_file_path("simple.pht");
    (void)manager.load_file(test_file);
    (void)manager.load_from_string("virtual.pht", "content");

    auto stats = manager.get_statistics();
    EXPECT_EQ(stats.total_files, 2u);

    // Clear should reset everything
    manager.clear();

    stats = manager.get_statistics();
    EXPECT_EQ(stats.total_files, 0u);
    EXPECT_EQ(stats.total_bytes, 0u);

    auto files = manager.get_loaded_files();
    EXPECT_TRUE(files.empty());

    // Should not be able to get previously loaded files
    const SourceFile *file = manager.get_file(test_file);
    EXPECT_EQ(file, nullptr);
}

TEST_F(SourceManagerTest, SourceManagerPreloadFiles) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::vector<std::string> files_to_preload = {
        get_test_file_path("simple.pht"),
        get_test_file_path("empty.pht"),
        "nonexistent.pht" // Should fail to load
    };

    size_t preloaded = manager.preload_files(files_to_preload);
    EXPECT_EQ(preloaded, 2u); // Should successfully preload 2 files

    auto stats = manager.get_statistics();
    EXPECT_EQ(stats.total_files, 2u);
}

TEST_F(SourceManagerTest, SourceManagerErrorHandling) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    // Test file not found
    auto result = manager.load_file("nonexistent_file.pht");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SourceError::FileNotFound);

    // Test empty filename
    result = manager.load_file("");
    EXPECT_FALSE(result.has_value());

    // Test creating location for invalid file
    auto loc_result = manager.create_location(INVALID_FILE_ID, 0);
    EXPECT_FALSE(loc_result.has_value());
}

// SourceManagerFactory tests

TEST_F(SourceManagerTest, SourceManagerFactoryFilesystemManager) {
    std::vector<std::string> include_paths = {test_dir_.string()};
    auto manager = SourceManagerFactory::create_filesystem_manager(*arena_, include_paths);

    ASSERT_NE(manager, nullptr);

    // Should be able to load files from include path
    auto result = manager->load_file("simple.pht");
    ASSERT_TRUE(result.has_value());

    const SourceFile *file = manager->get_file(result.value());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->content(), "fn main() {\n    emit(\"hello\")\n}");
}

TEST_F(SourceManagerTest, SourceManagerFactoryTestManager) {
    auto manager = SourceManagerFactory::create_test_manager(*arena_);

    ASSERT_NE(manager, nullptr);

    // Should be able to load from string
    std::string content = "test content";
    auto result = manager->load_from_string("test.pht", content);
    ASSERT_TRUE(result.has_value());

    const SourceFile *file = manager->get_file(result.value());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->content(), content);
}

// Performance and edge case tests

TEST_F(SourceManagerTest, SourceManagerLargeFile) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string large_file = get_test_file_path("large.pht");
    auto result = manager.load_file(large_file);
    ASSERT_TRUE(result.has_value());

    const SourceFile *file = manager.get_file(result.value());
    ASSERT_NE(file, nullptr);

    // Large file might be memory mapped
    EXPECT_GT(file->content().size(), 50000u);

    const auto &stats = file->statistics();
    EXPECT_GT(stats.byte_count, 50000u);
    EXPECT_GT(stats.line_count, 500u);
}

TEST_F(SourceManagerTest, SourceManagerUtf8Files) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string utf8_file = get_test_file_path("utf8.pht");
    auto result = manager.load_file(utf8_file);
    ASSERT_TRUE(result.has_value());

    const SourceFile *file = manager.get_file(result.value());
    ASSERT_NE(file, nullptr);

    // Should contain Unicode characters
    std::string content = std::string(file->content());
    EXPECT_NE(content.find("π"), std::string::npos);
    EXPECT_NE(content.find("🚀"), std::string::npos);

    // Should validate as proper UTF-8
    EXPECT_TRUE(file->validate_utf8());
}

TEST_F(SourceManagerTest, SourceManagerMixedLineEndings) {
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string mixed_file = get_test_file_path("mixed_endings.pht");
    auto result = manager.load_file(mixed_file);
    ASSERT_TRUE(result.has_value());

    const SourceFile *file = manager.get_file(result.value());
    ASSERT_NE(file, nullptr);

    // Should handle mixed line endings gracefully
    const auto &stats = file->statistics();
    EXPECT_GT(stats.line_count, 1u);

    // Should be able to get individual lines
    auto line_result = file->get_line_content(1);
    ASSERT_TRUE(line_result.has_value());
    EXPECT_EQ(line_result.value(), "line1");
}

TEST_F(SourceManagerTest, SourceManagerConcurrentAccess) {
    // Basic test for thread safety (would need more sophisticated testing)
    auto resolver = std::make_unique<FilesystemResolver>();
    SourceManager manager(*arena_, std::move(resolver));

    std::string content = "fn test() {}";
    auto result = manager.load_from_string("concurrent.pht", content);
    ASSERT_TRUE(result.has_value());

    FileID file_id = result.value();

    // Multiple threads accessing the same file should be safe
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&manager, file_id, &success_count]() {
            const SourceFile *file = manager.get_file(file_id);
            if (file != nullptr && !file->content().empty()) {
                success_count.fetch_add(1);
            }
        });
    }

    for (auto &thread: threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), 4);
}
