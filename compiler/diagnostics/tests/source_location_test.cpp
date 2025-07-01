/**
 * @file source_location_test.cpp
 * @brief Comprehensive test suite for SourceLocation and SourceRange
 * @author Photon Compiler Team
 * @version 1.0.0
 */

#include "photon/diagnostics/source_location.hpp"
#include <gtest/gtest.h>

using namespace photon::diagnostics;

namespace {

class SourceLocationTest : public ::testing::Test {
protected:
    void SetUp() override {
        valid_location_ = SourceLocation("test.pht", 10, 5, 42);
        invalid_location_ = SourceLocation();
    }
    
    SourceLocation valid_location_;
    SourceLocation invalid_location_;
};

} // anonymous namespace

// SourceLocation construction tests

TEST_F(SourceLocationTest, DefaultConstructorCreatesInvalidLocation) {
    SourceLocation loc;
    
    EXPECT_FALSE(loc.is_valid());
    EXPECT_EQ(loc.line(), 0u);
    EXPECT_EQ(loc.column(), 0u);
    EXPECT_EQ(loc.offset(), 0u);
    EXPECT_TRUE(loc.filename().empty());
}

TEST_F(SourceLocationTest, FullConstructorCreatesValidLocation) {
    SourceLocation loc("main.pht", 15, 8, 128);
    
    EXPECT_TRUE(loc.is_valid());
    EXPECT_EQ(loc.filename(), "main.pht");
    EXPECT_EQ(loc.line(), 15u);
    EXPECT_EQ(loc.column(), 8u);
    EXPECT_EQ(loc.offset(), 128u);
}

TEST_F(SourceLocationTest, PartialConstructorSetsZeroOffset) {
    SourceLocation loc("file.pht", 5, 3);
    
    EXPECT_TRUE(loc.is_valid());
    EXPECT_EQ(loc.filename(), "file.pht");
    EXPECT_EQ(loc.line(), 5u);
    EXPECT_EQ(loc.column(), 3u);
    EXPECT_EQ(loc.offset(), 0u);
}

// SourceLocation validation tests

TEST_F(SourceLocationTest, ValidLocationPassesValidation) {
    EXPECT_TRUE(valid_location_.is_valid());
}

TEST_F(SourceLocationTest, InvalidLocationFailsValidation) {
    EXPECT_FALSE(invalid_location_.is_valid());
}

TEST_F(SourceLocationTest, ZeroLineInvalidatesLocation) {
    SourceLocation loc("file.pht", 0, 1, 0);
    EXPECT_FALSE(loc.is_valid());
}

TEST_F(SourceLocationTest, ZeroColumnInvalidatesLocation) {
    SourceLocation loc("file.pht", 1, 0, 0);
    EXPECT_FALSE(loc.is_valid());
}

// SourceLocation accessor tests

TEST_F(SourceLocationTest, AccessorsReturnCorrectValues) {
    EXPECT_EQ(valid_location_.filename(), "test.pht");
    EXPECT_EQ(valid_location_.line(), 10u);
    EXPECT_EQ(valid_location_.column(), 5u);
    EXPECT_EQ(valid_location_.offset(), 42u);
}

// SourceLocation comparison tests

TEST_F(SourceLocationTest, EqualLocationsCompareEqual) {
    SourceLocation loc1("test.pht", 10, 5, 42);
    SourceLocation loc2("test.pht", 10, 5, 42);
    
    EXPECT_EQ(loc1, loc2);
    EXPECT_FALSE(loc1 < loc2);
    EXPECT_FALSE(loc1 > loc2);
}

TEST_F(SourceLocationTest, DifferentFilenamesCompareUnequal) {
    SourceLocation loc1("file1.pht", 10, 5, 42);
    SourceLocation loc2("file2.pht", 10, 5, 42);
    
    EXPECT_NE(loc1, loc2);
}

TEST_F(SourceLocationTest, DifferentLinesCompareCorrectly) {
    SourceLocation loc1("test.pht", 5, 5, 42);
    SourceLocation loc2("test.pht", 10, 5, 42);
    
    EXPECT_NE(loc1, loc2);
    EXPECT_LT(loc1, loc2);
    EXPECT_GT(loc2, loc1);
}

TEST_F(SourceLocationTest, DifferentColumnsCompareCorrectly) {
    SourceLocation loc1("test.pht", 10, 3, 42);
    SourceLocation loc2("test.pht", 10, 7, 42);
    
    EXPECT_NE(loc1, loc2);
    EXPECT_LT(loc1, loc2);
    EXPECT_GT(loc2, loc1);
}

// SourceLocation advance tests

TEST_F(SourceLocationTest, AdvanceCreatesNewLocation) {
    auto advanced = valid_location_.advance(5, 3, 20);
    
    EXPECT_EQ(advanced.filename(), valid_location_.filename());
    EXPECT_EQ(advanced.line(), 15u);
    EXPECT_EQ(advanced.column(), 8u);
    EXPECT_EQ(advanced.offset(), 62u);
}

TEST_F(SourceLocationTest, AdvanceZeroLeavesLocationUnchanged) {
    auto advanced = valid_location_.advance(0, 0, 0);
    
    EXPECT_EQ(advanced, valid_location_);
}

// SourceLocation string formatting tests

TEST_F(SourceLocationTest, ToStringFormatsValidLocation) {
    String result = valid_location_.to_string();
    EXPECT_EQ(result, "test.pht:10:5");
}

TEST_F(SourceLocationTest, ToStringFormatsInvalidLocation) {
    String result = invalid_location_.to_string();
    EXPECT_EQ(result, "<invalid location>");
}

TEST_F(SourceLocationTest, ToStringWithOffsetIncludesOffset) {
    String result = valid_location_.to_string(true);
    EXPECT_EQ(result, "test.pht:10:5 (offset 42)");
}

TEST_F(SourceLocationTest, ToStringWithOffsetIgnoresZeroOffset) {
    SourceLocation loc("test.pht", 10, 5, 0);
    String result = loc.to_string(true);
    EXPECT_EQ(result, "test.pht:10:5");
}

// SourceRange construction tests

class SourceRangeTest : public ::testing::Test {
protected:
    void SetUp() override {
        start_loc_ = SourceLocation("test.pht", 5, 10, 50);
        end_loc_ = SourceLocation("test.pht", 5, 20, 60);
        multiline_end_ = SourceLocation("test.pht", 8, 5, 120);
    }
    
    SourceLocation start_loc_;
    SourceLocation end_loc_;
    SourceLocation multiline_end_;
};

TEST_F(SourceRangeTest, DefaultConstructorCreatesInvalidRange) {
    SourceRange range;
    
    EXPECT_FALSE(range.is_valid());
    EXPECT_FALSE(range.start().is_valid());
    EXPECT_FALSE(range.end().is_valid());
}

TEST_F(SourceRangeTest, TwoLocationConstructorCreatesValidRange) {
    SourceRange range(start_loc_, end_loc_);
    
    EXPECT_TRUE(range.is_valid());
    EXPECT_EQ(range.start(), start_loc_);
    EXPECT_EQ(range.end(), end_loc_);
}

TEST_F(SourceRangeTest, SingleLocationConstructorCreatesPointRange) {
    SourceRange range(start_loc_);
    
    EXPECT_TRUE(range.is_valid());
    EXPECT_TRUE(range.is_point());
    EXPECT_EQ(range.start(), start_loc_);
    EXPECT_EQ(range.end(), start_loc_);
}

// SourceRange validation tests

TEST_F(SourceRangeTest, ValidRangePassesValidation) {
    SourceRange range(start_loc_, end_loc_);
    EXPECT_TRUE(range.is_valid());
}

TEST_F(SourceRangeTest, RangeWithInvalidStartFailsValidation) {
    SourceLocation invalid_start;
    SourceRange range(invalid_start, end_loc_);
    EXPECT_FALSE(range.is_valid());
}

TEST_F(SourceRangeTest, RangeWithInvalidEndFailsValidation) {
    SourceLocation invalid_end;
    SourceRange range(start_loc_, invalid_end);
    EXPECT_FALSE(range.is_valid());
}

// SourceRange property tests

TEST_F(SourceRangeTest, FilenameReturnsStartLocationFilename) {
    SourceRange range(start_loc_, end_loc_);
    EXPECT_EQ(range.filename(), start_loc_.filename());
}

TEST_F(SourceRangeTest, ByteLengthCalculatesCorrectly) {
    SourceRange range(start_loc_, end_loc_);
    EXPECT_EQ(range.byte_length(), 10u);
}

TEST_F(SourceRangeTest, PointRangeHasZeroByteLength) {
    SourceRange range(start_loc_);
    EXPECT_EQ(range.byte_length(), 0u);
}

TEST_F(SourceRangeTest, IsPointDetectsPointRanges) {
    SourceRange point_range(start_loc_);
    SourceRange normal_range(start_loc_, end_loc_);
    
    EXPECT_TRUE(point_range.is_point());
    EXPECT_FALSE(normal_range.is_point());
}

// SourceRange contains tests

TEST_F(SourceRangeTest, ContainsDetectsLocationWithinRange) {
    SourceRange range(start_loc_, end_loc_);
    SourceLocation middle("test.pht", 5, 15, 55);
    
    EXPECT_TRUE(range.contains(middle));
}

TEST_F(SourceRangeTest, ContainsIncludesStartLocation) {
    SourceRange range(start_loc_, end_loc_);
    EXPECT_TRUE(range.contains(start_loc_));
}

TEST_F(SourceRangeTest, ContainsExcludesEndLocation) {
    SourceRange range(start_loc_, end_loc_);
    EXPECT_FALSE(range.contains(end_loc_));
}

TEST_F(SourceRangeTest, ContainsRejectsLocationBeforeRange) {
    SourceRange range(start_loc_, end_loc_);
    SourceLocation before("test.pht", 5, 5, 45);
    
    EXPECT_FALSE(range.contains(before));
}

TEST_F(SourceRangeTest, ContainsRejectsLocationAfterRange) {
    SourceRange range(start_loc_, end_loc_);
    SourceLocation after("test.pht", 5, 25, 65);
    
    EXPECT_FALSE(range.contains(after));
}

TEST_F(SourceRangeTest, ContainsRejectsDifferentFile) {
    SourceRange range(start_loc_, end_loc_);
    SourceLocation different_file("other.pht", 5, 15, 55);
    
    EXPECT_FALSE(range.contains(different_file));
}

TEST_F(SourceRangeTest, ContainsHandlesInvalidLocations) {
    SourceRange range(start_loc_, end_loc_);
    SourceLocation invalid;
    
    EXPECT_FALSE(range.contains(invalid));
}

// SourceRange overlaps tests

TEST_F(SourceRangeTest, OverlapsDetectsOverlappingRanges) {
    SourceRange range1(start_loc_, end_loc_);
    SourceLocation overlap_start("test.pht", 5, 15, 55);
    SourceLocation overlap_end("test.pht", 5, 25, 65);
    SourceRange range2(overlap_start, overlap_end);
    
    EXPECT_TRUE(range1.overlaps(range2));
    EXPECT_TRUE(range2.overlaps(range1));
}

TEST_F(SourceRangeTest, OverlapsDetectsAdjacentRanges) {
    SourceRange range1(start_loc_, end_loc_);
    SourceLocation adjacent_start = end_loc_;
    SourceLocation adjacent_end("test.pht", 5, 30, 70);
    SourceRange range2(adjacent_start, adjacent_end);
    
    EXPECT_FALSE(range1.overlaps(range2));
    EXPECT_FALSE(range2.overlaps(range1));
}

TEST_F(SourceRangeTest, OverlapsRejectsNonOverlappingRanges) {
    SourceRange range1(start_loc_, end_loc_);
    SourceLocation separate_start("test.pht", 5, 25, 65);
    SourceLocation separate_end("test.pht", 5, 35, 75);
    SourceRange range2(separate_start, separate_end);
    
    EXPECT_FALSE(range1.overlaps(range2));
    EXPECT_FALSE(range2.overlaps(range1));
}

TEST_F(SourceRangeTest, OverlapsRejectsDifferentFiles) {
    SourceRange range1(start_loc_, end_loc_);
    SourceLocation other_start("other.pht", 5, 10, 50);
    SourceLocation other_end("other.pht", 5, 20, 60);
    SourceRange range2(other_start, other_end);
    
    EXPECT_FALSE(range1.overlaps(range2));
    EXPECT_FALSE(range2.overlaps(range1));
}

// SourceRange merge tests

TEST_F(SourceRangeTest, MergeCreatesSpanningRange) {
    SourceRange range1(start_loc_, end_loc_);
    SourceLocation range2_start("test.pht", 5, 15, 55);
    SourceLocation range2_end("test.pht", 5, 30, 70);
    SourceRange range2(range2_start, range2_end);
    
    SourceRange merged = range1.merge(range2);
    
    EXPECT_EQ(merged.start(), start_loc_);
    EXPECT_EQ(merged.end(), range2_end);
}

TEST_F(SourceRangeTest, MergeHandlesContainedRange) {
    SourceRange outer(start_loc_, end_loc_);
    SourceLocation inner_start("test.pht", 5, 12, 52);
    SourceLocation inner_end("test.pht", 5, 18, 58);
    SourceRange inner(inner_start, inner_end);
    
    SourceRange merged = outer.merge(inner);
    
    EXPECT_EQ(merged.start(), start_loc_);
    EXPECT_EQ(merged.end(), end_loc_);
}

TEST_F(SourceRangeTest, MergeWithInvalidRangeReturnsOriginal) {
    SourceRange range(start_loc_, end_loc_);
    SourceRange invalid;
    
    SourceRange merged = range.merge(invalid);
    
    EXPECT_EQ(merged.start(), start_loc_);
    EXPECT_EQ(merged.end(), end_loc_);
}

TEST_F(SourceRangeTest, MergeInvalidWithValidReturnsValid) {
    SourceRange invalid;
    SourceRange range(start_loc_, end_loc_);
    
    SourceRange merged = invalid.merge(range);
    
    EXPECT_EQ(merged.start(), start_loc_);
    EXPECT_EQ(merged.end(), end_loc_);
}

TEST_F(SourceRangeTest, MergeDifferentFilesReturnsFirst) {
    SourceRange range1(start_loc_, end_loc_);
    SourceLocation other_start("other.pht", 5, 10, 50);
    SourceLocation other_end("other.pht", 5, 20, 60);
    SourceRange range2(other_start, other_end);
    
    SourceRange merged = range1.merge(range2);
    
    EXPECT_EQ(merged.start(), start_loc_);
    EXPECT_EQ(merged.end(), end_loc_);
}

// SourceRange string formatting tests

TEST_F(SourceRangeTest, ToStringFormatsPointRange) {
    SourceRange range(start_loc_);
    String result = range.to_string();
    EXPECT_EQ(result, "test.pht:5:10");
}

TEST_F(SourceRangeTest, ToStringFormatsSameLineRange) {
    SourceRange range(start_loc_, end_loc_);
    String result = range.to_string();
    EXPECT_EQ(result, "test.pht:5:10-20");
}

TEST_F(SourceRangeTest, ToStringFormatsMultiLineRange) {
    SourceRange range(start_loc_, multiline_end_);
    String result = range.to_string();
    EXPECT_EQ(result, "test.pht:5:10-8:5");
}

TEST_F(SourceRangeTest, ToStringFormatsInvalidRange) {
    SourceRange range;
    String result = range.to_string();
    EXPECT_EQ(result, "<invalid range>");
}

TEST_F(SourceRangeTest, ToStringHandlesDifferentFiles) {
    SourceLocation other_end("other.pht", 5, 20, 60);
    SourceRange range(start_loc_, other_end);
    String result = range.to_string();
    EXPECT_EQ(result, "test.pht:5:10 to other.pht:5:20");
}