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
    static const std::string& GetReferenceAppId(const SceneSetApp& app) {
        return app.m_referenceAppId;
    }

    static void SetPreinstallDirectory(SceneSetApp& app, const std::string& preinstallDirectory) {
        app.m_preinstallDirectory = preinstallDirectory;
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

    static void SetMetadataExtractorForTesting(bool (*extractor)(const std::filesystem::path&, std::string&, std::string&)) {
        SceneSetApp::setMetadataExtractorForTesting(extractor);
    }

    static void ResetMetadataExtractorForTesting() {
        SceneSetApp::resetMetadataExtractorForTesting();
    }

    static std::string GetInstalledReferenceAppVersion(const SceneSetApp& app) {
        return app.getInstalledReferenceAppVersion();
    }

    static void SetFactoryAppsCopiedMarker(SceneSetApp& app, const std::string& markerPath) {
        app.m_factoryAppsCopiedMarkerOverride = markerPath;
    }

    static void SetFactoryAppPath(SceneSetApp& app, const std::string& factoryAppPath) {
        app.m_factoryAppPathOverride = factoryAppPath;
    }

    static void SetWaitingForStartupPreinstallCompletion(SceneSetApp& app, bool value) {
        app.m_waitingForStartupPreinstallCompletion.store(value);
    }

    static bool GetWaitingForStartupPreinstallCompletion(const SceneSetApp& app) {
        return app.m_waitingForStartupPreinstallCompletion.load();
    }

    static void SetStartupPreinstallHasFailure(SceneSetApp& app, bool value) {
        app.m_startupPreinstallHasFailure.store(value);
    }

    static void SetIsActive(SceneSetApp& app, bool value) {
        app.m_isActive.store(value);
    }

    static void CallCompleteStartupAfterPreinstall(SceneSetApp& app) {
        app.completeStartupAfterPreinstall();
    }

    static bool GetStartupPreinstallHasFailure(const SceneSetApp& app) {
        return app.m_startupPreinstallHasFailure.load();
    }

    static void CallResetStartupPreinstallStatusTracking(SceneSetApp& app) {
        app.resetStartupPreinstallStatusTracking();
    }

    static void CallRecordStartupPreinstallStatus(SceneSetApp& app, const std::string& json) {
        app.recordStartupPreinstallStatus(json);
    }

    static bool CallIsStartupPreinstallSucceed(const SceneSetApp& app) {
        return app.isStartupPreinstallSucceed();
    }

    static bool GetAppLaunched(const SceneSetApp& app) {
        return app.m_appLaunched.load();
    }

    static void SetAppLaunched(SceneSetApp& app, bool value) {
        app.m_appLaunched.store(value);
    }

    static void CallCheckAndLaunchIfAlreadyInstalled(SceneSetApp& app) {
        app.checkAndLaunchIfAlreadyInstalled();
    }

    static void CallStartLaunchThread(SceneSetApp& app) {
        app.startLaunchThread();
    }

    static void SetDownloadDirectory(SceneSetApp& app, const std::string& downloadDirectory) {
        app.m_downloadDirectory = downloadDirectory;
    }

    static void CallStartDownloadMonitorThread(SceneSetApp& app) {
        app.startDownloadMonitorThread();
    }

    static void CallStartPreinstallCompletionThread(SceneSetApp& app) {
        app.startPreinstallCompletionThread();
    }

    static void CallMonitorDownloadDirectory(SceneSetApp& app) {
        app.monitorDownloadDirectory();
    }

    static bool CallFetchPluginConfigValue(const SceneSetApp& app,
                                           const std::string& callsign,
                                           const std::string& configKey,
                                           std::string& value) {
        return app.fetchPluginConfigValue(callsign, configKey, value);
    }

    static void CallResolveDynamicDirectories(SceneSetApp& app) {
        app.resolveDynamicDirectories();
    }

    static const std::string& GetDownloadDirectory(const SceneSetApp& app) {
        return app.m_downloadDirectory;
    }

    static const std::string& GetPreinstallDirectory(const SceneSetApp& app) {
        return app.m_preinstallDirectory;
    }
};

// Thread-local storage for the reference app id to be returned by the fake extractor
thread_local std::string g_fakeExtractorRefAppId;

bool FakeExtractMetadataSuccess(const std::filesystem::path&,
                               std::string& appId,
                               std::string& version) {
    appId = g_fakeExtractorRefAppId;
    version = "1.0.0";
    return true;
}

class MetadataExtractorResetGuard {
public:
    MetadataExtractorResetGuard() = default;
    ~MetadataExtractorResetGuard() {
        SceneSetAppTestPeer::ResetMetadataExtractorForTesting();
    }
    MetadataExtractorResetGuard(const MetadataExtractorResetGuard&) = delete;
    MetadataExtractorResetGuard& operator=(const MetadataExtractorResetGuard&) = delete;
};


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
        unsetenv("SCENESET_INITIAL_DOWNLOAD_SWEEP");
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
    EXPECT_TRUE(result);
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
    const std::string& referenceAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (referenceAppId.empty()) {
        GTEST_SKIP() << "SceneSet reference app id is empty; SCENESET_DEFAULT_APPNAME is not configured for this build.";
    }
    g_fakeExtractorRefAppId = referenceAppId;
    SceneSetAppTestPeer::SetMetadataExtractorForTesting(&FakeExtractMetadataSuccess);
    MetadataExtractorResetGuard metadataExtractorResetGuard;

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_FALSE(std::filesystem::exists(srcFile));
    EXPECT_TRUE(std::filesystem::exists(dstFile));

    std::filesystem::remove_all(rootDir, ec);
}

// Test unRegisterForPreinstallEvents returns true even when preinstall manager is not initialized
TEST_F(SceneSetTest, UnregisterForPreinstallEventsReturnsTrue) {
    SceneSetApp app;
    bool result = app.unRegisterForPreinstallEvents();
    EXPECT_TRUE(result);
}

// Test unRegisterForPackageInstallerEvents returns true even when installer is not initialized
TEST_F(SceneSetTest, UnregisterForPackageInstallerEventsReturnsTrue) {
    SceneSetApp app;
    bool result = app.unRegisterForPackageInstallerEvents();
    EXPECT_TRUE(result);
}

// Test registerForPreinstallEvents returns false when preinstall manager is null
TEST_F(SceneSetTest, RegisterForPreinstallEventsWithNullManagerReturnsFalse) {
    SceneSetApp app;
    bool result = app.registerForPreinstallEvents();
    EXPECT_FALSE(result);
}

// Test registerForPackageInstallerEvents returns false when package installer is null
TEST_F(SceneSetTest, RegisterForPackageInstallerEventsWithNullInstallerReturnsFalse) {
    SceneSetApp app;
    bool result = app.registerForPackageInstallerEvents();
    EXPECT_FALSE(result);
}

// Test killReferenceApp returns false when reference app ID is empty
TEST_F(SceneSetTest, KillReferenceAppWithEmptyIdReturnsFalse) {
    SceneSetApp app;
    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (!refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is not empty in this build; skipping.";
    }
    bool result = app.killReferenceApp();
    EXPECT_FALSE(result);
}

// Test killReferenceApp returns false when AppManager is null but app ID is set
TEST_F(SceneSetTest, KillReferenceAppWithNullManagerReturnsFalse) {
    SceneSetApp app;
    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is empty in this build; cannot test null manager path.";
    }
    // m_appManager is null since initialize() was not called
    bool result = app.killReferenceApp();
    EXPECT_FALSE(result);
}

// Test startPreinstall returns false when preinstall manager is not initialized
TEST_F(SceneSetTest, StartPreinstallWithNullManagerReturnsFalse) {
    SceneSetApp app;
    bool result = app.startPreinstall(false);
    EXPECT_FALSE(result);
}

// Test startPreinstall with forceInstall=true returns false when preinstall manager is not initialized
TEST_F(SceneSetTest, StartPreinstallForceInstallWithNullManagerReturnsFalse) {
    SceneSetApp app;
    bool result = app.startPreinstall(true);
    EXPECT_FALSE(result);
}

// Test isReferenceAppInstalled returns false when reference app ID is empty
TEST_F(SceneSetTest, IsReferenceAppInstalledWithEmptyIdReturnsFalse) {
    SceneSetApp app;
    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (!refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is not empty in this build; skipping.";
    }
    bool result = app.isReferenceAppInstalled();
    EXPECT_FALSE(result);
}

// Test isReferenceAppInstalled returns false when AppManager is null
TEST_F(SceneSetTest, IsReferenceAppInstalledWithNullManagerReturnsFalse) {
    SceneSetApp app;
    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is empty in this build; cannot test null manager path.";
    }
    // m_appManager is null since initialize() was not called
    bool result = app.isReferenceAppInstalled();
    EXPECT_FALSE(result);
}

// Test processDownloadedPackage returns false when metadata extraction fails
TEST_F(SceneSetTest, ProcessDownloadedPackageReturnsFalseOnMetadataFailure) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_process_meta_fail");
    const auto srcFile = rootDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(rootDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetMetadataExtractorForTesting(
        [](const std::filesystem::path&, std::string&, std::string&) -> bool { return false; });
    MetadataExtractorResetGuard metadataExtractorResetGuard;

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_FALSE(result);

    std::filesystem::remove_all(rootDir, ec);
}

// Test processDownloadedPackage returns false when extracted app ID does not match reference ID
TEST_F(SceneSetTest, ProcessDownloadedPackageReturnsFalseWhenAppIdDoesNotMatchReference) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_process_appid_mismatch");
    const auto srcFile = rootDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(rootDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    g_fakeExtractorRefAppId = "com.example.different_app_not_reference";
    SceneSetAppTestPeer::SetMetadataExtractorForTesting(&FakeExtractMetadataSuccess);
    MetadataExtractorResetGuard metadataExtractorResetGuard;

    const std::string& referenceAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (referenceAppId == g_fakeExtractorRefAppId) {
        GTEST_SKIP() << "Reference app ID unexpectedly matches test value; cannot test mismatch path.";
    }

    const bool result = SceneSetAppTestPeer::ProcessDownloadedPackage(app, srcFile);
    EXPECT_FALSE(result);
    // Source file must not have been moved
    EXPECT_TRUE(std::filesystem::exists(srcFile));

    std::filesystem::remove_all(rootDir, ec);
}

// Test MovePackageToPreinstallDirectory creates the destination directory when it does not exist
TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryCreatesNewDestinationDirectory) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_move_creates_dst");
    const auto srcDir = rootDir / "download";
    const auto dstDir = rootDir / "preinstall_new";
    const auto srcFile = srcDir / "bundle.bolt";
    const auto dstFile = dstDir / "bundle.bolt";

    std::error_code ec;
    std::filesystem::create_directories(srcDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream source(srcFile);
        source << "payload";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, dstDir.string());
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, srcFile);
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(dstDir));
    EXPECT_TRUE(std::filesystem::exists(dstFile));
    EXPECT_FALSE(std::filesystem::exists(srcFile));

    std::filesystem::remove_all(rootDir, ec);
}

// Test MovePackageToPreinstallDirectory returns false when preinstall directory path is empty
TEST_F(SceneSetTest, MovePackageToPreinstallDirectoryReturnsFalseWithEmptyPreinstallDir) {
    SceneSetApp app;
    const auto tempFile = MakeUniqueTempPath("sceneset_dummy_bundle");
    SceneSetAppTestPeer::SetPreinstallDirectory(app, "");
    const bool result = SceneSetAppTestPeer::MovePackageToPreinstallDirectory(app, tempFile);
    EXPECT_FALSE(result);
}

// Test cleanupPreinstallFolder handles a nonexistent directory without throwing
TEST_F(SceneSetTest, CleanupPreinstallFolderHandlesNonexistentDirectoryGracefully) {
    SceneSetApp app;
    const auto nonexistentDir = MakeUniqueTempPath("sceneset_nonexistent_preinstall");

    std::error_code ec;
    std::filesystem::remove_all(nonexistentDir, ec);

    SceneSetAppTestPeer::SetPreinstallDirectory(app, nonexistentDir.string());
    EXPECT_NO_THROW({
        app.cleanupPreinstallFolder();
    });
}

// Test cleanupPreinstallFolder removes files from an existing preinstall directory
TEST_F(SceneSetTest, CleanupPreinstallFolderRemovesFilesFromDirectory) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_cleanup_preinstall");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream file1(preinstallDir / "app1.bolt");
        file1 << "data1";
        std::ofstream file2(preinstallDir / "app2.bolt");
        file2 << "data2";
    }
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app2.bolt"));

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    EXPECT_NO_THROW({
        app.cleanupPreinstallFolder();
    });

    EXPECT_FALSE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    EXPECT_FALSE(std::filesystem::exists(preinstallDir / "app2.bolt"));

    std::filesystem::remove_all(preinstallDir, ec);
}

// Test releaseComInterfaces does not throw when all interface pointers are null
TEST_F(SceneSetTest, ReleaseComInterfacesDoesNotThrowWhenNullInterfaces) {
    SceneSetApp app;
    EXPECT_NO_THROW({
        app.releaseComInterfaces();
    });
}

// Test additional truthy and falsy string values for the initial download sweep env flag
TEST_F(SceneSetTest, InitialDownloadSweepHandlesAdditionalEnvFlagValues) {
    SceneSetApp app;

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "true", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "yes", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "on", 1);
    EXPECT_TRUE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "false", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "no", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));

    setenv("SCENESET_INITIAL_DOWNLOAD_SWEEP", "0", 1);
    EXPECT_FALSE(SceneSetAppTestPeer::ShouldRunInitialDownloadSweep(app));
}

// Test getInstalledReferenceAppVersion returns empty when reference app ID is empty
TEST_F(SceneSetTest, GetInstalledReferenceAppVersionReturnsEmptyWhenReferenceAppIdIsEmpty) {
    SceneSetApp app;

    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (!refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is not empty in this build; skipping empty-ID path.";
    }

    const std::string version = SceneSetAppTestPeer::GetInstalledReferenceAppVersion(app);
    EXPECT_TRUE(version.empty());
}

// Test getInstalledReferenceAppVersion returns empty when AppManager is null (not initialized)
TEST_F(SceneSetTest, GetInstalledReferenceAppVersionReturnsEmptyWhenAppManagerIsNull) {
    SceneSetApp app;

    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is empty in this build/runtime configuration; "
                     << "cannot test null-manager path.";
    }

    // m_appManager is null because initialize() was never called
    const std::string version = SceneSetAppTestPeer::GetInstalledReferenceAppVersion(app);
    EXPECT_TRUE(version.empty());
}

// --- isFactoryAppsCopied tests ---

// Test isFactoryAppsCopied returns false when marker file does not exist
TEST_F(SceneSetTest, IsFactoryAppsCopiedReturnsFalseWhenMarkerDoesNotExist) {
    SceneSetApp app;
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::remove(markerPath, ec);

    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());
    EXPECT_FALSE(app.isFactoryAppsCopied());
}

// Test isFactoryAppsCopied returns true when marker file exists
TEST_F(SceneSetTest, IsFactoryAppsCopiedReturnsTrueWhenMarkerExists) {
    SceneSetApp app;
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    {
        std::ofstream marker(markerPath);
        marker << "Factory apps copied on first boot";
    }

    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());
    EXPECT_TRUE(app.isFactoryAppsCopied());

    std::error_code ec;
    std::filesystem::remove(markerPath, ec);
}

// --- markFactoryAppsCopied tests ---

// Test markFactoryAppsCopied creates the marker file
TEST_F(SceneSetTest, MarkFactoryAppsCopiedCreatesMarkerFile) {
    SceneSetApp app;
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::remove(markerPath, ec);

    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());
    app.markFactoryAppsCopied();

    EXPECT_TRUE(std::filesystem::exists(markerPath));

    std::filesystem::remove(markerPath, ec);
}

// Test markFactoryAppsCopied causes subsequent isFactoryAppsCopied to return true
TEST_F(SceneSetTest, MarkFactoryAppsCopiedCausesIsFactoryAppsCopiedToReturnTrue) {
    SceneSetApp app;
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::remove(markerPath, ec);

    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());
    EXPECT_FALSE(app.isFactoryAppsCopied());

    app.markFactoryAppsCopied();
    EXPECT_TRUE(app.isFactoryAppsCopied());

    std::filesystem::remove(markerPath, ec);
}

// --- copyFactoryAppsToPreinstall tests ---

// Test copyFactoryAppsToPreinstall returns false when factory app source does not exist
TEST_F(SceneSetTest, CopyFactoryAppsToPreinstallReturnsFalseWhenSourceDoesNotExist) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_factory_copy_no_src");
    const auto nonExistentSource = rootDir / "factory_apps";
    const auto preinstallDir = rootDir / "preinstall";
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::remove_all(nonExistentSource, ec);
    std::filesystem::remove(markerPath, ec);

    SceneSetAppTestPeer::SetFactoryAppPath(app, nonExistentSource.string());
    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());

    EXPECT_FALSE(app.copyFactoryAppsToPreinstall());
    EXPECT_FALSE(std::filesystem::exists(markerPath));

    std::filesystem::remove_all(rootDir, ec);
    std::filesystem::remove(markerPath, ec);
}

// Test copyFactoryAppsToPreinstall returns true when source directory is empty
TEST_F(SceneSetTest, CopyFactoryAppsToPreinstallReturnsTrueWhenSourceIsEmpty) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_factory_copy_empty_src");
    const auto sourceDir = rootDir / "factory_apps";
    const auto preinstallDir = rootDir / "preinstall";
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::create_directories(sourceDir, ec);
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);
    std::filesystem::remove(markerPath, ec);

    SceneSetAppTestPeer::SetFactoryAppPath(app, sourceDir.string());
    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());

    EXPECT_TRUE(app.copyFactoryAppsToPreinstall());
    EXPECT_TRUE(std::filesystem::exists(markerPath));

    std::filesystem::remove_all(rootDir, ec);
    std::filesystem::remove(markerPath, ec);
}

// Test copyFactoryAppsToPreinstall copies files to the preinstall directory
TEST_F(SceneSetTest, CopyFactoryAppsToPreinstallCopiesFilesToPreinstallDirectory) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_factory_copy_files");
    const auto sourceDir = rootDir / "factory_apps";
    const auto preinstallDir = rootDir / "preinstall";
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::create_directories(sourceDir, ec);
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);
    std::filesystem::remove(markerPath, ec);

    {
        std::ofstream app1(sourceDir / "app1.bolt");
        app1 << "payload1";
        std::ofstream app2(sourceDir / "app2.bolt");
        app2 << "payload2";
    }

    SceneSetAppTestPeer::SetFactoryAppPath(app, sourceDir.string());
    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());

    EXPECT_TRUE(app.copyFactoryAppsToPreinstall());
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app2.bolt"));
    EXPECT_TRUE(std::filesystem::exists(markerPath));

    std::filesystem::remove_all(rootDir, ec);
    std::filesystem::remove(markerPath, ec);
}

// Test copyFactoryAppsToPreinstall creates the preinstall directory when it does not exist
TEST_F(SceneSetTest, CopyFactoryAppsToPreinstallCreatesPreinstallDirectoryWhenMissing) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_factory_copy_create_dst");
    const auto sourceDir = rootDir / "factory_apps";
    const auto preinstallDir = rootDir / "new_preinstall";
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::create_directories(sourceDir, ec);
    ASSERT_FALSE(ec);
    std::filesystem::remove_all(preinstallDir, ec);
    std::filesystem::remove(markerPath, ec);

    {
        std::ofstream bundle(sourceDir / "bundle.bolt");
        bundle << "data";
    }

    SceneSetAppTestPeer::SetFactoryAppPath(app, sourceDir.string());
    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());

    EXPECT_TRUE(app.copyFactoryAppsToPreinstall());
    EXPECT_TRUE(std::filesystem::exists(preinstallDir));
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "bundle.bolt"));
    EXPECT_TRUE(std::filesystem::exists(markerPath));

    std::filesystem::remove_all(rootDir, ec);
    std::filesystem::remove(markerPath, ec);
}

// Test copyFactoryAppsToPreinstall overwrites existing files in the preinstall directory
TEST_F(SceneSetTest, CopyFactoryAppsToPreinstallOverwritesExistingFiles) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_factory_copy_overwrite");
    const auto sourceDir = rootDir / "factory_apps";
    const auto preinstallDir = rootDir / "preinstall";
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::create_directories(sourceDir, ec);
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);
    std::filesystem::remove(markerPath, ec);

    {
        std::ofstream srcFile(sourceDir / "bundle.bolt");
        srcFile << "new_content";
    }
    {
        std::ofstream existingFile(preinstallDir / "bundle.bolt");
        existingFile << "old_content";
    }

    SceneSetAppTestPeer::SetFactoryAppPath(app, sourceDir.string());
    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());

    EXPECT_TRUE(app.copyFactoryAppsToPreinstall());

    std::ifstream finalFile(preinstallDir / "bundle.bolt");
    std::string finalContent;
    std::getline(finalFile, finalContent);
    EXPECT_EQ(finalContent, "new_content");

    std::filesystem::remove_all(rootDir, ec);
    std::filesystem::remove(markerPath, ec);
}

// Test copyFactoryAppsToPreinstall skips subdirectories (only copies regular files)
TEST_F(SceneSetTest, CopyFactoryAppsToPreinstallSkipsSubdirectories) {
    SceneSetApp app;
    const auto rootDir = MakeUniqueTempPath("sceneset_factory_copy_skip_dirs");
    const auto sourceDir = rootDir / "factory_apps";
    const auto preinstallDir = rootDir / "preinstall";
    const auto markerPath = MakeUniqueTempPath("sceneset_factory_marker");

    std::error_code ec;
    std::filesystem::create_directories(sourceDir / "subdir", ec);
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);
    std::filesystem::remove(markerPath, ec);

    {
        std::ofstream regularFile(sourceDir / "app.bolt");
        regularFile << "payload";
    }

    SceneSetAppTestPeer::SetFactoryAppPath(app, sourceDir.string());
    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetFactoryAppsCopiedMarker(app, markerPath.string());

    EXPECT_TRUE(app.copyFactoryAppsToPreinstall());
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app.bolt"));
    EXPECT_FALSE(std::filesystem::exists(preinstallDir / "subdir"));

    std::filesystem::remove_all(rootDir, ec);
    std::filesystem::remove(markerPath, ec);
}

// --- completeStartupAfterPreinstall tests ---

// Test completeStartupAfterPreinstall does nothing when not waiting (idempotent guard)
TEST_F(SceneSetTest, CompleteStartupAfterPreinstallDoesNothingWhenNotWaiting) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_complete_not_waiting");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream f(preinstallDir / "app.bolt");
        f << "data";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    // m_waitingForStartupPreinstallCompletion is false by default — guard should fire
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, false);
    SceneSetAppTestPeer::SetIsActive(app, true);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCompleteStartupAfterPreinstall(app);
    });

    // Preinstall folder must be untouched because the call returned early
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app.bolt"));

    std::filesystem::remove_all(preinstallDir, ec);
}

// Test completeStartupAfterPreinstall does nothing when inactive (shutdown started)
TEST_F(SceneSetTest, CompleteStartupAfterPreinstallDoesNothingWhenInactive) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_complete_inactive");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream f(preinstallDir / "app.bolt");
        f << "data";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, true);
    SceneSetAppTestPeer::SetIsActive(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCompleteStartupAfterPreinstall(app);
    });

    // Preinstall folder must be untouched because the call returned early
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app.bolt"));
    // Flag must have been consumed (set to false) by the compare-exchange
    EXPECT_FALSE(SceneSetAppTestPeer::GetWaitingForStartupPreinstallCompletion(app));

    std::filesystem::remove_all(preinstallDir, ec);
}

// Test completeStartupAfterPreinstall cleans up the preinstall folder on success
TEST_F(SceneSetTest, CompleteStartupAfterPreinstallCleansUpPreinstallFolderOnSuccess) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_complete_success");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream f1(preinstallDir / "app1.bolt");
        f1 << "data1";
        std::ofstream f2(preinstallDir / "app2.bolt");
        f2 << "data2";
    }
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app2.bolt"));

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, true);
    SceneSetAppTestPeer::SetIsActive(app, true);
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCompleteStartupAfterPreinstall(app);
    });

    // Preinstall folder files must have been removed on success
    EXPECT_FALSE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    EXPECT_FALSE(std::filesystem::exists(preinstallDir / "app2.bolt"));
    EXPECT_FALSE(SceneSetAppTestPeer::GetWaitingForStartupPreinstallCompletion(app));

    std::filesystem::remove_all(preinstallDir, ec);
}

// Test completeStartupAfterPreinstall preserves the preinstall folder on failure
TEST_F(SceneSetTest, CompleteStartupAfterPreinstallPreservesPreinstallFolderOnFailure) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_complete_failure");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream f1(preinstallDir / "app1.bolt");
        f1 << "data1";
        std::ofstream f2(preinstallDir / "app2.bolt");
        f2 << "data2";
    }
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app2.bolt"));

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, true);
    SceneSetAppTestPeer::SetIsActive(app, true);
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, true);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCompleteStartupAfterPreinstall(app);
    });

    // Preinstall folder must be untouched on failure
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app1.bolt"));
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app2.bolt"));
    EXPECT_FALSE(SceneSetAppTestPeer::GetWaitingForStartupPreinstallCompletion(app));

    std::filesystem::remove_all(preinstallDir, ec);
}

// Test completeStartupAfterPreinstall is idempotent — second call is a no-op
TEST_F(SceneSetTest, CompleteStartupAfterPreinstallIsIdempotentOnSecondCall) {
    SceneSetApp app;
    const auto preinstallDir = MakeUniqueTempPath("sceneset_complete_idempotent");

    std::error_code ec;
    std::filesystem::create_directories(preinstallDir, ec);
    ASSERT_FALSE(ec);

    {
        std::ofstream f(preinstallDir / "app.bolt");
        f << "data";
    }

    SceneSetAppTestPeer::SetPreinstallDirectory(app, preinstallDir.string());
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, true);
    SceneSetAppTestPeer::SetIsActive(app, true);
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    // First call — should clean up
    SceneSetAppTestPeer::CallCompleteStartupAfterPreinstall(app);
    EXPECT_FALSE(SceneSetAppTestPeer::GetWaitingForStartupPreinstallCompletion(app));

    // Re-create the file to detect whether a second cleanup attempt happens
    {
        std::ofstream f(preinstallDir / "app.bolt");
        f << "data";
    }
    ASSERT_TRUE(std::filesystem::exists(preinstallDir / "app.bolt"));

    // Second call — must be a no-op (waiting flag is already false)
    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCompleteStartupAfterPreinstall(app);
    });
    EXPECT_TRUE(std::filesystem::exists(preinstallDir / "app.bolt"));

    std::filesystem::remove_all(preinstallDir, ec);
}

// --- resetStartupPreinstallStatusTracking tests ---

// Test resetStartupPreinstallStatusTracking clears the failure flag
TEST_F(SceneSetTest, ResetStartupPreinstallStatusTrackingClearsFailureFlag) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, true);
    ASSERT_TRUE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));

    SceneSetAppTestPeer::CallResetStartupPreinstallStatusTracking(app);

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test resetStartupPreinstallStatusTracking is idempotent when flag is already false
TEST_F(SceneSetTest, ResetStartupPreinstallStatusTrackingIsIdempotentWhenAlreadyClear) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallResetStartupPreinstallStatusTracking(app);
    });

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// --- recordStartupPreinstallStatus tests ---

// Test recordStartupPreinstallStatus ignores empty string
TEST_F(SceneSetTest, RecordStartupPreinstallStatusIgnoresEmptyString) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, "");
    });

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus ignores invalid JSON
TEST_F(SceneSetTest, RecordStartupPreinstallStatusIgnoresInvalidJson) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, "not valid json {{[");
    });

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus does not set failure for INSTALLED state
TEST_F(SceneSetTest, RecordStartupPreinstallStatusDoesNotSetFailureForInstalledState) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    const std::string json = R"([{"packageId":"com.example.app","version":"1.0","state":"INSTALLED"}])";
    SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, json);

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus does not set failure for INSTALLING state
TEST_F(SceneSetTest, RecordStartupPreinstallStatusDoesNotSetFailureForInstallingState) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    const std::string json = R"([{"packageId":"com.example.app","version":"1.0","state":"INSTALLING"}])";
    SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, json);

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus sets failure flag for unexpected state
TEST_F(SceneSetTest, RecordStartupPreinstallStatusSetsFailureForUnexpectedState) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    const std::string json = R"([{"packageId":"com.example.app","version":"1.0","state":"FAILED"}])";
    SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, json);

    EXPECT_TRUE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus sets failure when any package has an unexpected state
TEST_F(SceneSetTest, RecordStartupPreinstallStatusSetsFailureWhenAnyPackageFails) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    const std::string json = R"([
        {"packageId":"com.example.app1","version":"1.0","state":"INSTALLED"},
        {"packageId":"com.example.app2","version":"2.0","state":"ERROR"}
    ])";
    SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, json);

    EXPECT_TRUE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus skips entries with missing packageId
TEST_F(SceneSetTest, RecordStartupPreinstallStatusSkipsEntryWithMissingPackageId) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    const std::string json = R"([{"version":"1.0","state":"FAILED"}])";
    SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, json);

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// Test recordStartupPreinstallStatus skips entries with missing state
TEST_F(SceneSetTest, RecordStartupPreinstallStatusSkipsEntryWithMissingState) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    const std::string json = R"([{"packageId":"com.example.app","version":"1.0"}])";
    SceneSetAppTestPeer::CallRecordStartupPreinstallStatus(app, json);

    EXPECT_FALSE(SceneSetAppTestPeer::GetStartupPreinstallHasFailure(app));
}

// --- isStartupPreinstallSucceed tests ---

// Test isStartupPreinstallSucceed returns true when no failure recorded
TEST_F(SceneSetTest, IsStartupPreinstallSucceedReturnsTrueWhenNoFailure) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, false);

    EXPECT_TRUE(SceneSetAppTestPeer::CallIsStartupPreinstallSucceed(app));
}

// Test isStartupPreinstallSucceed returns false when failure has been recorded
TEST_F(SceneSetTest, IsStartupPreinstallSucceedReturnsFalseWhenFailureRecorded) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, true);

    EXPECT_FALSE(SceneSetAppTestPeer::CallIsStartupPreinstallSucceed(app));
}

// Test resetStartupPreinstallStatusTracking followed by isStartupPreinstallSucceed
TEST_F(SceneSetTest, IsStartupPreinstallSucceedReturnsTrueAfterReset) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetStartupPreinstallHasFailure(app, true);
    ASSERT_FALSE(SceneSetAppTestPeer::CallIsStartupPreinstallSucceed(app));

    SceneSetAppTestPeer::CallResetStartupPreinstallStatusTracking(app);

    EXPECT_TRUE(SceneSetAppTestPeer::CallIsStartupPreinstallSucceed(app));
}

// --- checkAndLaunchIfAlreadyInstalled tests ---

// Test checkAndLaunchIfAlreadyInstalled does not set appLaunched when app manager is null
TEST_F(SceneSetTest, CheckAndLaunchIfAlreadyInstalledDoesNotLaunchWhenManagerIsNull) {
    SceneSetApp app;
    ASSERT_FALSE(SceneSetAppTestPeer::GetAppLaunched(app));

    // AppManager is null — isReferenceAppInstalled() returns false
    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCheckAndLaunchIfAlreadyInstalled(app);
    });

    EXPECT_FALSE(SceneSetAppTestPeer::GetAppLaunched(app));
}

// Test checkAndLaunchIfAlreadyInstalled is a no-op when app is already launched
TEST_F(SceneSetTest, CheckAndLaunchIfAlreadyInstalledDoesNothingWhenAlreadyLaunched) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetAppLaunched(app, true);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallCheckAndLaunchIfAlreadyInstalled(app);
    });

    // Flag must remain true and no crash or side-effect
    EXPECT_TRUE(SceneSetAppTestPeer::GetAppLaunched(app));
}

// --- startLaunchThread tests ---

// Test startLaunchThread does not throw
TEST_F(SceneSetTest, StartLaunchThreadDoesNotThrow) {
    SceneSetApp app;
    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);

    if (!refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is configured; startLaunchThread may invoke launchDefaultApp without initialized AppManager.";
    }

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartLaunchThread(app);
    });
    // Destructor joins the thread; no race or crash expected
}

// Test startLaunchThread can be called repeatedly without crashing
TEST_F(SceneSetTest, StartLaunchThreadCanBeCalledMultipleTimes) {
    SceneSetApp app;
    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);

    if (!refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is configured; startLaunchThread may invoke launchDefaultApp without initialized AppManager.";
    }

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartLaunchThread(app);
        SceneSetAppTestPeer::CallStartLaunchThread(app);
        SceneSetAppTestPeer::CallStartLaunchThread(app);
    });
    // Each call stops the previous thread before starting a new one
}

// --- startDownloadMonitorThread tests ---

// Test startDownloadMonitorThread does nothing when download directory is empty
TEST_F(SceneSetTest, StartDownloadMonitorThreadDoesNothingWhenDownloadDirectoryEmpty) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetDownloadDirectory(app, "");

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartDownloadMonitorThread(app);
    });
    // Destructor will safely clean up (no thread was spawned)
}

// Test startDownloadMonitorThread does not throw when given a valid directory
TEST_F(SceneSetTest, StartDownloadMonitorThreadDoesNotThrowWithValidDirectory) {
    SceneSetApp app;
    const auto downloadDir = MakeUniqueTempPath("sceneset_download_monitor_dir");

    std::error_code ec;
    std::filesystem::create_directories(downloadDir, ec);
    ASSERT_FALSE(ec);

    SceneSetAppTestPeer::SetDownloadDirectory(app, downloadDir.string());

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartDownloadMonitorThread(app);
    });
    // Destructor stops and joins the monitor thread

    std::filesystem::remove_all(downloadDir, ec);
}

// Test startDownloadMonitorThread can be called repeatedly without crashing
TEST_F(SceneSetTest, StartDownloadMonitorThreadCanBeCalledMultipleTimes) {
    SceneSetApp app;
    const auto downloadDir = MakeUniqueTempPath("sceneset_download_monitor_repeat");

    std::error_code ec;
    std::filesystem::create_directories(downloadDir, ec);
    ASSERT_FALSE(ec);

    SceneSetAppTestPeer::SetDownloadDirectory(app, downloadDir.string());

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartDownloadMonitorThread(app);
        SceneSetAppTestPeer::CallStartDownloadMonitorThread(app);
    });
    // Each call stops the previous thread before spawning a new one

    std::filesystem::remove_all(downloadDir, ec);
}

// --- startPreinstallCompletionThread tests ---

// Test startPreinstallCompletionThread does not start when inactive (shutdown in progress)
TEST_F(SceneSetTest, StartPreinstallCompletionThreadSkipsWhenInactive) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetIsActive(app, false);
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartPreinstallCompletionThread(app);
    });
    // No thread was spawned; destructor must not deadlock
}

// Test startPreinstallCompletionThread starts when active
TEST_F(SceneSetTest, StartPreinstallCompletionThreadStartsWhenActive) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetIsActive(app, true);
    // m_waitingForStartupPreinstallCompletion is false by default so
    // completeStartupAfterPreinstall() returns immediately — thread is safe to join
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartPreinstallCompletionThread(app);
    });
    // Destructor joins the spawned thread
}

// Test startPreinstallCompletionThread does not spawn a second thread when one is already running
TEST_F(SceneSetTest, StartPreinstallCompletionThreadSkipsDuplicateStart) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetIsActive(app, true);
    SceneSetAppTestPeer::SetWaitingForStartupPreinstallCompletion(app, false);

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallStartPreinstallCompletionThread(app);
        // Second call while the first thread may still be running — must be a no-op
        SceneSetAppTestPeer::CallStartPreinstallCompletionThread(app);
    });
}

// --- monitorDownloadDirectory tests ---

// Test monitorDownloadDirectory exits cleanly when download directory is empty string
TEST_F(SceneSetTest, MonitorDownloadDirectoryExitsCleanlyWithEmptyDownloadDirectory) {
    SceneSetApp app;
    SceneSetAppTestPeer::SetDownloadDirectory(app, "");

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallMonitorDownloadDirectory(app);
    });
}

// Test monitorDownloadDirectory exits cleanly when download directory does not exist
TEST_F(SceneSetTest, MonitorDownloadDirectoryExitsCleanlyWhenDirectoryDoesNotExist) {
    SceneSetApp app;
    const auto nonExistentDir = MakeUniqueTempPath("sceneset_monitor_nonexistent");

    std::error_code ec;
    std::filesystem::remove_all(nonExistentDir, ec);

    SceneSetAppTestPeer::SetDownloadDirectory(app, nonExistentDir.string());

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallMonitorDownloadDirectory(app);
    });
}

// Test monitorDownloadDirectory exits cleanly when download directory path points to a file
TEST_F(SceneSetTest, MonitorDownloadDirectoryExitsCleanlyWhenPathIsAFile) {
    SceneSetApp app;
    const auto filePath = MakeUniqueTempPath("sceneset_monitor_not_a_dir");

    {
        std::ofstream f(filePath);
        f << "not a directory";
    }

    SceneSetAppTestPeer::SetDownloadDirectory(app, filePath.string());

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallMonitorDownloadDirectory(app);
    });

    std::error_code ec;
    std::filesystem::remove(filePath, ec);
}

// --- getInstalledReferenceAppVersion additional tests ---

// Test getInstalledReferenceAppVersion returns empty when both referenceAppId and appManager are null/empty
TEST_F(SceneSetTest, GetInstalledReferenceAppVersionReturnsEmptyWhenBothConditionsUnsatisfied) {
    unsetenv("SCENESET_DEFAULT_APPNAME");
    SceneSetApp app;

    const std::string& refAppId = SceneSetAppTestPeer::GetReferenceAppId(app);
    if (!refAppId.empty()) {
        GTEST_SKIP() << "Reference app ID is not empty in this build; skipping.";
    }

    // Both m_referenceAppId is empty AND m_appManager is null
    const std::string version = SceneSetAppTestPeer::GetInstalledReferenceAppVersion(app);
    EXPECT_TRUE(version.empty());
}

// Test getInstalledReferenceAppVersion returns empty when called on a newly constructed app
TEST_F(SceneSetTest, GetInstalledReferenceAppVersionReturnsEmptyForNewlyConstructedApp) {
    SceneSetApp app;
    // initialize() was never called, so m_appManager is null regardless of env
    const std::string version = SceneSetAppTestPeer::GetInstalledReferenceAppVersion(app);
    EXPECT_TRUE(version.empty());
}

// --- fetchPluginConfigValue tests ---

// Test fetchPluginConfigValue returns false when Thunder is unreachable (invalid path)
TEST_F(SceneSetTest, FetchPluginConfigValueReturnsFalseWhenThunderUnreachable) {
    setenv("THUNDER_ACCESS", "/invalid/thunder/path", 1);
    SceneSetApp app;

    std::string value = "should_be_cleared";
    const bool result = SceneSetAppTestPeer::CallFetchPluginConfigValue(
        app, "org.rdk.SomePlugin", "someKey", value);

    EXPECT_FALSE(result);
    EXPECT_TRUE(value.empty());
}

// Test fetchPluginConfigValue clears the value parameter on failure
TEST_F(SceneSetTest, FetchPluginConfigValueClearsValueOnFailure) {
    setenv("THUNDER_ACCESS", "/nonexistent/communicator", 1);
    SceneSetApp app;

    std::string value = "pre_existing_value";
    SceneSetAppTestPeer::CallFetchPluginConfigValue(
        app, "org.rdk.AnyPlugin", "anyKey", value);

    EXPECT_TRUE(value.empty());
}

// Test fetchPluginConfigValue returns false for an empty callsign
TEST_F(SceneSetTest, FetchPluginConfigValueReturnsFalseForEmptyCallsign) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    std::string value;
    const bool result = SceneSetAppTestPeer::CallFetchPluginConfigValue(
        app, "", "someKey", value);

    EXPECT_FALSE(result);
}

// Test fetchPluginConfigValue returns false for an empty config key
TEST_F(SceneSetTest, FetchPluginConfigValueReturnsFalseForEmptyConfigKey) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    std::string value;
    const bool result = SceneSetAppTestPeer::CallFetchPluginConfigValue(
        app, "org.rdk.SomePlugin", "", value);

    EXPECT_FALSE(result);
}

// Test fetchPluginConfigValue does not throw
TEST_F(SceneSetTest, FetchPluginConfigValueDoesNotThrow) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    std::string value;
    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallFetchPluginConfigValue(
            app, "org.rdk.SomePlugin", "someKey", value);
    });
}

// --- resolveDynamicDirectories tests ---

// Test resolveDynamicDirectories does not throw when Thunder is unavailable
TEST_F(SceneSetTest, ResolveDynamicDirectoriesDoesNotThrowWhenThunderUnavailable) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallResolveDynamicDirectories(app);
    });
}

// Test resolveDynamicDirectories preserves download directory when Thunder is unavailable
TEST_F(SceneSetTest, ResolveDynamicDirectoriesPreservesDownloadDirectoryWhenThunderUnavailable) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;
    SceneSetAppTestPeer::SetDownloadDirectory(app, "/preexisting/download/dir");

    SceneSetAppTestPeer::CallResolveDynamicDirectories(app);

    EXPECT_EQ(SceneSetAppTestPeer::GetDownloadDirectory(app), "/preexisting/download/dir");
}

// Test resolveDynamicDirectories preserves preinstall directory when Thunder is unavailable
TEST_F(SceneSetTest, ResolveDynamicDirectoriesPreservesPreinstallDirectoryWhenThunderUnavailable) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;
    SceneSetAppTestPeer::SetPreinstallDirectory(app, "/preexisting/preinstall/dir");

    SceneSetAppTestPeer::CallResolveDynamicDirectories(app);

    EXPECT_EQ(SceneSetAppTestPeer::GetPreinstallDirectory(app), "/preexisting/preinstall/dir");
}

// Test resolveDynamicDirectories can be called multiple times without crashing
TEST_F(SceneSetTest, ResolveDynamicDirectoriesCanBeCalledMultipleTimes) {
    setenv("THUNDER_ACCESS", "/invalid/path", 1);
    SceneSetApp app;

    EXPECT_NO_THROW({
        SceneSetAppTestPeer::CallResolveDynamicDirectories(app);
        SceneSetAppTestPeer::CallResolveDynamicDirectories(app);
    });
}
