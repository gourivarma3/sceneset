/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
 */

#include "SceneSet.h"
#include <fstream>

#ifndef SCENESET_DEFAULT_APPNAME
#define SCENESET_DEFAULT_APPNAME ""
#endif

#define SCENESET_CONFIG_FILE "/opt/sceneset_app.conf"

static std::string getDefaultAppName() {
    std::ifstream configFile(SCENESET_CONFIG_FILE);
    if (configFile.is_open()) {
        std::string appName;
        std::getline(configFile, appName);

        if (!appName.empty()) {
            std::cout << "Using sceneset default app from config file: " << appName << std::endl;
            return appName;
        }
    }

    std::string appDefault = SCENESET_DEFAULT_APPNAME;
    if (!appDefault.empty()) {
        std::cout << "Using sceneset default app: " << appDefault << std::endl;
    }
    return appDefault;
}

SceneSetApp::SceneSetApp()
    :  m_act_cv(), m_isActive(false), m_lock(), m_appManager(nullptr), m_preinstallManager(nullptr), m_appManagerEventHandler(nullptr), m_preinstallManagerEventHandler(nullptr), m_appmgrCallsign("org.rdk.AppManager"), m_preinstallCallsign("org.rdk.PreinstallManager"), m_referenceAppId(getDefaultAppName()), m_comrpcPath("/tmp/communicator"), m_launchThread(nullptr), m_stopLaunchThread(false), m_appLaunched(false), m_launchThreadMutex() {
}

SceneSetApp::~SceneSetApp() {
    stopCurrentLaunchThread();
    unRegisterForAppEvents();
    unRegisterForPreinstallEvents();
}

bool SceneSetApp::initialize() {
    const char *thunderAccess = std::getenv("THUNDER_ACCESS");
    std::string envThunderAccess = (thunderAccess != nullptr) ? thunderAccess : m_comrpcPath;

    std::cout << "Thunder Access Path: " << envThunderAccess << std::endl;

    Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), envThunderAccess.c_str());

    auto appManagerClient = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));

    if (!appManagerClient.IsValid()) {
        std::cerr << "Failed to create COMRPC client for AppManager." << std::endl;
        return false;
    }

    std::cout << "AppManager COMRPC client created successfully" << std::endl;

    // Open AppManager interface
    m_appManager = appManagerClient->Open<Exchange::IAppManager>(m_appmgrCallsign.c_str());
    if (m_appManager == nullptr) {
        std::cerr << "Failed to open IAppManager interface." << std::endl;
        return false;
    }

    std::cout << "Successfully opened " << m_appmgrCallsign << " interface" << std::endl;

    auto preinstallClient = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId(envThunderAccess.c_str()));

    if (!preinstallClient.IsValid()) {
        std::cerr << "Failed to create COMRPC client for PreinstallManager." << std::endl;
        // Clean up appManager before returning
        if (m_appManager != nullptr) {
            m_appManager->Release();
            m_appManager = nullptr;
        }
        return false;
    }

    std::cout << "PreinstallManager COMRPC client created successfully" << std::endl;
    // Open PreinstallManager interface
    m_preinstallManager = preinstallClient->Open<Exchange::IPreinstallManager>(m_preinstallCallsign.c_str());
    if (m_preinstallManager == nullptr) {
        std::cerr << "Failed to open IPreinstallManager interface." << std::endl;
        // Clean up appManager before returning
        if (m_appManager != nullptr) {
            m_appManager->Release();
            m_appManager = nullptr;
        }
        return false;
    }

    std::cout << "Successfully opened " << m_preinstallCallsign << " interface" << std::endl;

    {
        lock_guard<mutex> lkgd(m_lock);
        m_isActive = true;
    }
    cout << "Registered to AppManager. Setting term signal" << endl;
    // Register term signal handler
    signal(SIGTERM, [](int x) {
        SceneSetApp::handleTerminationSignal(x);
    });
    return m_isActive;
}

bool SceneSetApp::registerForAppEvents() {
    if (nullptr == m_appManagerEventHandler) {
        m_appManagerEventHandler = std::make_shared<AppManagerEventHandler>();
    }
    if (m_appManager != nullptr) {
        m_appManager->Register(m_appManagerEventHandler.get());
        return true;
    }
    return false;
}

bool SceneSetApp::registerForPreinstallEvents() {
    if (nullptr == m_preinstallManagerEventHandler) {
        m_preinstallManagerEventHandler = std::make_shared<PreinstallManagerEventHandler>();
    }
    if (m_preinstallManager != nullptr) {
        m_preinstallManager->Register(m_preinstallManagerEventHandler.get());
        return true;
    }
    return false;
}

bool SceneSetApp::unRegisterForAppEvents() {
    std::cout << " Unregistering for AppManager Events " << endl;
    if (nullptr != m_appManagerEventHandler && nullptr != m_appManager) {
        try {
            m_appManager->Unregister(m_appManagerEventHandler.get());
            std::cout << " Unregistered AppManager Events " << endl;
        } catch (...) {
            std::cerr << "Exception during AppManager unregister" << endl;
        }
    } else {
        std::cout << "AppManager or EventHandler is null, cannot unregister" << endl;
    }
    m_appManagerEventHandler = nullptr;
    return true;
}

bool SceneSetApp::unRegisterForPreinstallEvents() {
    std::cout << " Unregistering for Preinstall Events " << endl;
    if (nullptr != m_preinstallManagerEventHandler && nullptr != m_preinstallManager) {
        try {
            m_preinstallManager->Unregister(m_preinstallManagerEventHandler.get());
            std::cout << " Unregistered Preinstall Events " << endl;
        } catch (...) {
            std::cerr << "Exception during PreinstallManager unregister" << endl;
        }
    } else {
        std::cout << "PreinstallManager or EventHandler is null, cannot unregister" << endl;
    }
    m_preinstallManagerEventHandler = nullptr;
    return true;
}

bool SceneSetApp::launchDefaultApp() {
    if (m_referenceAppId.empty()) {
        std::cout << "No app name specified in SCENESET_REFERENCE_APPID env variable." << std::endl;
        return false;
    }
    std::cout << "Launching default app: " << m_referenceAppId << std::endl;
    Core::hresult result = m_appManager->LaunchApp(m_referenceAppId, "", "");
    if (result != Core::ERROR_NONE) {
        std::cerr << "LaunchApp failed with error code: " << result << std::endl;
        return false;
    }
    return true;
}

bool SceneSetApp::startPreinstall() {
    if (m_preinstallManager == nullptr) {
        std::cerr << "PreinstallManager is not initialized, cannot start preinstall." << std::endl;
        return false;
    }
    Core::hresult result = m_preinstallManager->StartPreinstall(false);  // StartPreinstall is called with argument false . Not doing force install
    if (result != Core::ERROR_NONE) {
        std::cerr << "StartPreinstall failed with error code: " << result << std::endl;
        return false;
    }
    return true;
}

bool SceneSetApp::isReferenceAppInstalled() {
    if (m_referenceAppId.empty()) {
        std::cout << "No reference app ID specified" << std::endl;
        return false;
    }

    if (m_appManager == nullptr) {
        std::cerr << "AppManager is not initialized" << std::endl;
        return false;
    }

    std::string installedApps;
    Core::hresult result = m_appManager->GetInstalledApps(installedApps);

    if (result != Core::ERROR_NONE) {
        std::cerr << "GetInstalledApps failed with error code: " << result << std::endl;
        return false;
    }

    std::cout << "Installed apps: " << installedApps << std::endl;

    // Parse JSON array
    JsonArray apps;
    if (!apps.FromString(installedApps)) {
        std::cerr << "Failed to parse installed apps JSON response" << std::endl;
        return false;
    }

    // Iterate through the array to find reference app
    JsonArray::Iterator index = apps.Elements();
    while (index.Next()) {
        const JsonValue& element = index.Current();
        
        if (element.Content() == JsonValue::type::OBJECT) {
            JsonObject appObj = element.Object();
            
            if (appObj.HasLabel("appId")) {
                const JsonValue& appIdValue = appObj["appId"];
                if (appIdValue.Content() == JsonValue::type::STRING) {
                    std::string appId = appIdValue.String();
                    if (appId == m_referenceAppId) {
                        std::cout << "Reference app '" << m_referenceAppId << "' is already installed" << std::endl;
                        return true;
                    }
                }
            }
        }
    }

    std::cout << "Reference app '" << m_referenceAppId << "' is not installed yet" << std::endl;
    return false;
}

void SceneSetApp::checkAndLaunchIfAlreadyInstalled() {
    if (!m_appLaunched) {
        std::cout << "Checking if reference app is already installed" << std::endl;
        if (isReferenceAppInstalled()) {
            bool expected = false;
            if (m_appLaunched.compare_exchange_strong(expected, true)) {
                std::cout << "Reference app '" << m_referenceAppId << "' is already installed. Launching default app." << std::endl;
                startLaunchThread();
            }
        } else {
            std::cout << "Reference app '" << m_referenceAppId << "' is not installed." << std::endl;
        }
    }
}

void SceneSetApp::waitForTermSignal() {
    std::thread termThread([&]() {
        while (m_isActive) {
            std::unique_lock<std::mutex> ulock(m_lock);
            m_act_cv.wait(ulock);
        }
        std::cout << "Exiting application..." << std::endl;
    });
    termThread.join();
}

void SceneSetApp::handleTerminationSignal(int signal) {
    std::cout << "Received termination signal: " << signal << std::endl;
    SceneSetApp::getInstance().onTerminate();
}

void SceneSetApp::onTerminate() {
    std::unique_lock<std::mutex> ulock(m_lock);
    m_isActive = false;
    m_act_cv.notify_one();
    unRegisterForAppEvents();
    unRegisterForPreinstallEvents();
}

SceneSetApp& SceneSetApp::getInstance() {
    static SceneSetApp instance;
    return instance;
}

void SceneSetApp::run() {
    if (!initialize()) return;
    sd_notifyf(0, "READY=1\n"
                  "STATUS=ComRPC client is Successfully Initialized\n"
                  "MAINPID=%lu",
               (unsigned long)getpid());
    if (m_referenceAppId.empty()) {
        std::cout << "No reference app ID specified, skipping preinstall and app launch" << std::endl;
        return;
    }
    registerForPreinstallEvents();
    registerForAppEvents();

    // Start preinstall first
    std::cout << "Starting preinstall process" << std::endl;
    if (!startPreinstall()) {
        std::cerr << "Preinstall process failed to trigger" << std::endl;
    }

    // Check if app is already installed and launch if not launched already from preinstall events
    checkAndLaunchIfAlreadyInstalled();

    waitForTermSignal();
}

// AppManagerEventHandler implementations
SceneSetApp::AppManagerEventHandler::~AppManagerEventHandler() {}

void SceneSetApp::AppManagerEventHandler::OnAppInstalled(const string &appId, const string &version) {
    std::cout << "App Installed: " << appId << " Version: " << version << std::endl;
}

void SceneSetApp::AppManagerEventHandler::OnAppUninstalled(const string &appId) {
    std::cout << "App Uninstalled: " << appId << std::endl;
}

void SceneSetApp::AppManagerEventHandler::OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason) {

    SceneSetApp& instance = SceneSetApp::getInstance();
    std::cout << "App Lifecycle State Changed for " << appId
              << " from " << getAppStateString(oldState) << " (" << static_cast<int>(oldState) << ")"
              << " to " << getAppStateString(newState) << " (" << static_cast<int>(newState) << ")" << std::endl;
    if (!instance.m_referenceAppId.empty() && appId == instance.m_referenceAppId) {
        if (oldState == Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING &&
            newState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED &&
            errorReason == Exchange::IAppManager::AppErrorReason::APP_ERROR_ABORT) {
            std::cout << "App " << appId << " terminated with ABORT error. Restarting reference app." << std::endl;
            instance.startLaunchThread();
        }
    }
}

void SceneSetApp::AppManagerEventHandler::OnAppLaunchRequest(const string &appId, const string &intent, const string &source) {
    std::cout << "App Launch Request: " << appId << " Intent: " << intent << " Source: " << source << std::endl;
}

const char* SceneSetApp::AppManagerEventHandler::getAppStateString(const Exchange::IAppManager::AppLifecycleState state) {
    switch(state) {
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED: return "UNLOADED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_LOADING: return "LOADING";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_INITIALIZING: return "INITIALIZING";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED: return "PAUSED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_RUNNING: return "RUNNING";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE: return "ACTIVE";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED: return "SUSPENDED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED: return "HIBERNATED";
        case Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING: return "TERMINATING";
        default: return "UNKNOWN";
    }
}

void SceneSetApp::AppManagerEventHandler::OnAppUnloaded(const string &appId, const string &appInstanceId) {
    std::cout << "App Unloaded: " << appId << " Instance ID: " << appInstanceId << std::endl;
}

uint32_t SceneSetApp::AppManagerEventHandler::AddRef() const {
    cout << " AddRef called  " << endl;
    return Core::ERROR_NONE;
}

uint32_t SceneSetApp::AppManagerEventHandler::Release() const {
    cout << " Release called " << endl;
    return Core::ERROR_NONE;
}

/**
 * Stops the currently running launch thread, if any.
 */
void SceneSetApp::stopCurrentLaunchThread() {
    std::lock_guard<std::mutex> lock(m_launchThreadMutex);
    if (m_launchThread && m_launchThread->joinable()) {
        std::cout << "Stopping current launch thread" << std::endl;
        m_stopLaunchThread = true;
        m_launchThread->join();
        m_launchThread.reset();
        std::cout << "Launch thread stopped successfully" << std::endl;
    }
    m_stopLaunchThread = false;
}

// Launching the default app from a thread as this is also called from appmanager OnAppLifecycleStateChanged event handler
void SceneSetApp::startLaunchThread() {
    stopCurrentLaunchThread();

    m_launchThread = std::make_unique<std::thread>([this]() {
        try {
            if (!m_stopLaunchThread) {
                std::cout << "Executing launchDefaultApp from thread" << std::endl;
                if (!launchDefaultApp()) {
                    std::cerr << "Failed to launch default app from thread" << std::endl;
                }
            } else {
                std::cout << "Launch thread cancelled before execution" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in launch thread: " << e.what() << std::endl;
        }
    });
}

SceneSetApp::PreinstallManagerEventHandler::~PreinstallManagerEventHandler() {}

void SceneSetApp::PreinstallManagerEventHandler::OnAppInstallationStatus(const string &jsonresponse) {
    std::cout << "OnAppInstallationStatus: " << jsonresponse << std::endl;

    if (jsonresponse.empty()) {
        return;
    }

    SceneSetApp& instance = SceneSetApp::getInstance();

    // Format: [{"packageId":"appId","version":"x.y.z","state":"INSTALLED"}]
    // Parse JSON array
    JsonArray packages;
    if (!packages.FromString(jsonresponse)) {
        std::cerr << "Failed to parse JSON response" << std::endl;
        return;
    }

    // Iterate through the array as we get response in jsonarray format
    JsonArray::Iterator index = packages.Elements();
    while (index.Next()) {
        const JsonValue& element = index.Current();
        // Get the JSON object
        if (element.Content() == JsonValue::type::OBJECT) {
            JsonObject packageObj = element.Object();
            std::string packageId;
            std::string state;
            
            if (packageObj.HasLabel("packageId")) {
                const JsonValue& pkgIdValue = packageObj["packageId"];
                if (pkgIdValue.Content() == JsonValue::type::STRING) {
                    packageId = pkgIdValue.String();
                }
            }
            
            if (packageObj.HasLabel("state")) {
                const JsonValue& stateValue = packageObj["state"];
                if (stateValue.Content() == JsonValue::type::STRING) {
                    state = stateValue.String();
                }
            }
            
            std::cout << "Package: " << packageId << ", State: " << state << std::endl;
            
            // Check if this is the reference app and it is installed
            if (!instance.m_referenceAppId.empty() &&
                packageId == instance.m_referenceAppId &&
                state == "INSTALLED") {
                bool expected = false;
                if (instance.m_appLaunched.compare_exchange_strong(expected, true)) {
                    std::cout << "Reference app '" << packageId << "' installed via preinstall. Launching default app." << std::endl;
                    instance.startLaunchThread();
                }
                break;
            }
        }
    }
}

uint32_t SceneSetApp::PreinstallManagerEventHandler::AddRef() const {
    return Core::ERROR_NONE;
}

uint32_t SceneSetApp::PreinstallManagerEventHandler::Release() const {
    return Core::ERROR_NONE;
}
