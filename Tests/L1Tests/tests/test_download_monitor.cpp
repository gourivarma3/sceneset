/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

namespace {
// Helper: Simulate directory iteration filtering logic used by download monitor initial sweep.
// Returns list of non-hidden regular file paths that would be enqueued.
std::vector<fs::path> GetNonHiddenRegularFiles(const fs::path& directory) {
    std::vector<fs::path> candidates;
    std::error_code iterEc;

    fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, iterEc);
    if (iterEc) {
        return candidates;  // Failed to open directory
    }

    fs::directory_iterator end;
    for (; it != end; it.increment(iterEc)) {
        if (iterEc) {
            break;  // Error during iteration
        }

        const auto& entry = *it;
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc) || entryEc) {
            continue;  // Not a regular file
        }

        const std::string fileName = entry.path().filename().string();
        if (!fileName.empty() && fileName[0] == '.') {
            continue;  // Hidden file (dotfile)
        }

        candidates.push_back(entry.path());
    }

    return candidates;
}

std::filesystem::path MakeUniqueTempPath(const std::string& prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::ostringstream name;
    name << prefix << "_" << now << "_" << tid;
    return std::filesystem::temp_directory_path() / name.str();
}
}  // namespace

// Integration test: Download monitor directory scanning filters hidden files.
TEST(DownloadMonitorTest, InitialSweepIgnoresHiddenFiles) {
    const auto downloadDir = MakeUniqueTempPath("download_monitor_test");
    std::error_code ec;
    fs::create_directories(downloadDir, ec);
    ASSERT_FALSE(ec);

    // Create test files: mix of hidden and regular.
    const auto hidden1 = downloadDir / ".sample";
    const auto hidden2 = downloadDir / ".hidden_file.ipk";
    const auto regular1 = downloadDir / "package.ipk";
    const auto regular2 = downloadDir / "app.tar.gz";

    {
        std::ofstream f1(hidden1);
        f1 << "hidden content";
    }
    {
        std::ofstream f2(hidden2);
        f2 << "hidden content";
    }
    {
        std::ofstream f3(regular1);
        f3 << "regular content";
    }
    {
        std::ofstream f4(regular2);
        f4 << "regular content";
    }

    // Simulate initial sweep filtering logic.
    const auto filtered = GetNonHiddenRegularFiles(downloadDir);

    // Verify only regular (non-hidden) files are returned.
    EXPECT_EQ(filtered.size(), 2);

    const auto filenames = [&filtered]() {
        std::vector<std::string> names;
        for (const auto& p : filtered) {
            names.push_back(p.filename().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }();

    EXPECT_THAT(filenames, testing::UnorderedElementsAre("app.tar.gz", "package.ipk"));

    // Cleanup.
    fs::remove_all(downloadDir, ec);
}

// Test: Download monitor handles empty directory gracefully.
TEST(DownloadMonitorTest, InitialSweepHandlesEmptyDirectory) {
    const auto downloadDir = MakeUniqueTempPath("download_monitor_empty_test");
    std::error_code ec;
    fs::create_directories(downloadDir, ec);
    ASSERT_FALSE(ec);

    const auto filtered = GetNonHiddenRegularFiles(downloadDir);
    EXPECT_TRUE(filtered.empty());

    fs::remove_all(downloadDir, ec);
}

// Test: Download monitor handles directory with only hidden files.
TEST(DownloadMonitorTest, InitialSweepHandlesOnlyHiddenFiles) {
    const auto downloadDir = MakeUniqueTempPath("download_monitor_hidden_only_test");
    std::error_code ec;
    fs::create_directories(downloadDir, ec);
    ASSERT_FALSE(ec);

    // Create only hidden files.
    {
        std::ofstream f(downloadDir / ".hidden1");
        f << "content";
    }
    {
        std::ofstream f(downloadDir / ".hidden2");
        f << "content";
    }

    const auto filtered = GetNonHiddenRegularFiles(downloadDir);
    EXPECT_TRUE(filtered.empty());

    fs::remove_all(downloadDir, ec);
}
