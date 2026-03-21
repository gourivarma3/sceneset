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
#include <cstdlib>
#include <csignal>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "SceneSet.h"
#include "RalfPackageSupport.h"

namespace {
std::filesystem::path MakeUniqueTempPath(const std::string& prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::ostringstream name;
    name << prefix << "_" << now << "_" << tid;
    return std::filesystem::temp_directory_path() / name.str();
}
} // namespace

class SceneSetAppTestPeer {
public:
    static void SetPreinstallDirectory(SceneSetApp& app, const std::string& preinstallDirectory) {
        app.m_preinstallDirectory = preinstallDirectory;
    }

    static void SetReferenceAppId(SceneSetApp& app, const std::string& referenceAppId) {
        app.m_referenceAppId = referenceAppId;
    }

    static bool MovePackageToPreinstallDirectory(SceneSetApp& app, const std::filesystem::path& sourcePath) {
        return app.movePackageToPreinstallDirectory(sourcePath);
    }

    static bool ShouldRunInitialDownloadSweep(const SceneSetApp& app) {
        return app.shouldRunInitialDownloadSweep();
    }

    static bool ProcessDownloadedPackage(SceneSetApp& app, const std::filesystem::path& packagePath) {
        return app.processDownloadedPackage(packagePath);
    }

    static void SetMetadataExtractorForTesting(bool (*extractor)(const std::filesystem::path&, const std::filesystem::path&, std::string&, std::string&)) {
        SceneSetApp::setMetadataExtractorForTesting(extractor);
    }

    static void ResetMetadataExtractorForTesting() {
        SceneSetApp::resetMetadataExtractorForTesting();
    }
};

bool FakeExtractMetadataSuccess(const std::filesystem::path&,
                               const std::filesystem::path&,
                               std::string& appId,
                               std::string& version) {
    appId = "TestApp";
    version = "1.0.0";
    return true;
}


class SceneSetTest : public ::testing::Test {
protected:
    SceneSetTest() = default;
    virtual ~SceneSetTest() = default;

    void SetUp() override {
        // Set environment variables for testing
        setenv("THUNDER_ACCESS", "/tmp/communicator", 1);
        setenv("SCENESET_DEFAULT_APPNAME", "TestApp", 1);
    }

    void TearDown() override {
        // Clean up environment variables
        unsetenv("THUNDER_ACCESS");
        unsetenv("SCENESET_DEFAULT_APPNAME");
    }
};

// Test SceneSetApp singleton pattern
TEST_F(SceneSetTest, SingletonPattern) {
    SceneSetApp& instance1 = SceneSetApp::getInstance();
    SceneSetApp& instance2 = SceneSetApp::getInstance();

    EXPECT_EQ(&instance1, &instance2);
}

// Test SceneSetApp constructor and destructor
TEST_F(SceneSetTest, ConstructorDestructor) {
    EXPECT_NO_THROW({
        SceneSetApp app;
    });
}


// Test AppManagerEventHandler functionality
TEST_F(SceneSetTest, AppManagerEventHandlerCreation) {
    SceneSetApp app;
    EXPECT_NO_THROW({
        app.registerForAppEvents();
    });
}

// Test launching default app without environment variable
TEST_F(SceneSetTest, LaunchDefaultAppWithoutEnvVar) {
    unsetenv("SCENESET_DEFAULT_APPNAME");
    SceneSetApp app;

    bool result = app.launchDefaultApp();
    EXPECT_FALSE(result);
}


// Test initialization without Thunder connection
TEST_F(SceneSetTest, InitializationWithoutThunder) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    bool result = app.initialize();
    EXPECT_FALSE(result);
}

// Test termination signal handling
TEST_F(SceneSetTest, TerminationSignalHandling) {
    // Test static signal handler
    EXPECT_NO_THROW({
        SceneSetApp::handleTerminationSignal(SIGTERM);
    });
}

// Test thread safety and concurrent operations
TEST_F(SceneSetTest, ThreadSafety) {
    SceneSetApp& app = SceneSetApp::getInstance();

    // Test that multiple threads can access getInstance safely
    std::vector<std::thread> threads;
    std::vector<SceneSetApp*> instances(10);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = &SceneSetApp::getInstance();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All instances should be the same
    for (int i = 1; i < 10; ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// Test onTerminate functionality
TEST_F(SceneSetTest, OnTerminate) {
    SceneSetApp app;
    EXPECT_NO_THROW({
        app.onTerminate();
    });
}

// Test unregister functionality
TEST_F(SceneSetTest, UnregisterForAppEvents) {
    SceneSetApp app;
    bool result = app.unRegisterForAppEvents();
    EXPECT_FALSE(result);
}

TEST_F(SceneSetTest, ExtractPackageMetadataReturnsFalseForMissingCertDirectory) {
    std::string appId;
    std::string version;

    const std::filesystem::path missingCertDir = MakeUniqueTempPath("sceneset_missing_cert_dir");
    const std::filesystem::path fakePackage = missingCertDir / "fake.bolt";

    std::error_code ec;
    std::filesystem::remove_all(missingCertDir, ec);

    EXPECT_NO_THROW({
        const bool result = ralf_support::ExtractPackageMetadata(fakePackage, missingCertDir, appId, version);
        EXPECT_FALSE(result);
    });

    std::filesystem::remove_all(missingCertDir, ec);
}

TEST_F(SceneSetTest, ExtractPackageMetadataReturnsFalseWhenCertPathIsNotDirectory) {
    std::string appId;
    std::string version;

    const std::filesystem::path certPathFile = MakeUniqueTempPath("sceneset_cert_path_file");
    const std::filesystem::path fakePackage = certPathFile.parent_path() / "fake.bolt";

    {
        std::ofstream file(certPathFile);
        file << "not a certificate directory" << std::endl;
    }

    EXPECT_NO_THROW({
        const bool result = ralf_support::ExtractPackageMetadata(fakePackage, certPathFile, appId, version);
        EXPECT_FALSE(result);
    });

    std::error_code ec;
    std::filesystem::remove_all(certPathFile, ec);
}

TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryOverwritesExistingFile) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_stage_overwrite");
    const auto srcDir = rootDir / "download";
    const auto dstDir = rootDir / "preinstall";
    const auto srcFile = srcDir / "bundle.bolt";
    const auto dstFile = dstDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    std::filesystem::create_directories(dstDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "new_payload";
    }
    {
        std::ofstream destination(dstFile);
        destination << "old_payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_FALSE(std::filesystem::exists(srcFile));
    EXPECT_TRUE(std::filesystem::exists(dstFile));

    std::ifstream finalFile(dstFile);
    std::string finalContent;
    std::getline(finalFile, finalContent);
    EXPECT_EQ(finalContent, "new_payload");

    std::filesystem::remove_all(rootDir, ec);
}

TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryReturnsFalseForMissingSource) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_stage_missing_source");
    const auto dstDir = rootDir / "preinstall";
    const auto missingSource = rootDir / "download" / "missing.pkg";

    std::error_code ec;
    std::filesystem::create_directories(dstDir, ec);
    ASSERT_FALSE(ec);

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, missingSource);
    EXPECT_FALSE(result);

    std::filesystem::remove_all(rootDir, ec);
}

TEST_F(SceneSetTest, InitialDownloadSweepDecisionHonorsEnvironmentFlag) {
    SceneSetApp app;

    unsetenv("SCENESET_INITIAL_DOWNLOAD_SWEEP");
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "1", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "off", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));
}

TEST_F(SceneSetTest, ProcessDownloadedPackageStagesReferenceBundleWithInjectedMetadata) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_process_downloaded_package");
    const auto srcDir = rootDir / "download";
    const auto dstDir = rootDir / "preinstall";
    const auto srcFile = srcDir / "bundle.bolt";
    const auto dstFile = dstDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    std::filesystem::create_directories(dstDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    SceneSetAppTestPeer::SetReferenceAppId(app, "TestApp");
    SceneSetAppTestPeer::SetMetadataExtractorForTesting(&FakeExtractMetadataSuccess);

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_FALSE(std::filesystem::exists(srcFile));
    EXPECT_TRUE(std::filesystem::exists(dstFile));

    SceneSetAppTestPeer::ResetMetadataExtractorForTesting();
    std::filesystem::remove_all(rootDir, ec);
}
