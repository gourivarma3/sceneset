# SceneSet Architecture

SceneSet coordinates reference app launch, package preinstall, runtime lifecycle handling, and reference app update staging.

## Boot and Preinstall Flow

```mermaid
flowchart TD
    A[SceneSetApp run] --> B[initialize]
    B --> C[Create AppManager and PreinstallManager COMRPC clients]
    C --> D{Both interfaces opened?}
    D -- No --> X1[Initialization fails and run exits]
    D -- Yes --> E[resolveDynamicDirectories]
    E --> F{appPreinstallDirectory available?}
    F -- No --> X1
    F -- Yes --> G[Set active flag and SIGTERM handler]
    G --> H{referenceAppId empty?}
    H -- Yes --> X2[Skip preinstall and launch path]
    H -- No --> I[Register Preinstall and App notifications]
    I --> J{Factory apps already copied marker exists?}
    J -- No --> K[copyFactoryAppsToPreinstall]
    J -- Yes --> L[Skip factory copy]
    K --> M[startPreinstall forceInstall true]
    L --> N[startPreinstall forceInstall false]
    M --> O{Preinstall success?}
    N --> O
    O -- Yes --> P[cleanupPreinstallFolder]
    O -- No --> Q[Continue without cleanup]
    P --> R[checkAndLaunchIfAlreadyInstalled]
    Q --> R
    R --> S{Reference app installed?}
    S -- Yes --> T[startLaunchThread then launchDefaultApp]
    S -- No --> U[Wait for install or lifecycle events]
    T --> V[waitForTermSignal]
    U --> V
```

## Runtime Event and Update Flow

```mermaid
flowchart TD
    A[Runtime active] --> B[Start download monitor if enabled and downloadDir is valid]
    B --> C[monitorDownloadDirectory inotify close_write moved_to]
    C --> D[processDownloadedPackage]
    D --> E[Extract metadata via libralf]
    E --> F{package appId equals reference appId?}
    F -- No --> G[Ignore package]
    F -- Yes --> H{m_appLaunched true and installed version equals package version?}
    H -- Yes --> G
    H -- No --> I[movePackageToPreinstallDirectory]
    I --> J{Move succeeded?}
    J -- Yes --> K[Package staged for next install cycle]
    J -- No --> L[Log error and continue monitoring]

    A --> M[OnAppLifecycleStateChanged for reference app]
    M --> N{State RUNNING or ACTIVE?}
    N -- Yes --> O[Set m_appLaunched true]
    N -- No --> P{State TERMINATING or UNLOADED?}
    P -- Yes --> Q[Set m_appLaunched false]
    P -- No --> R[No running-status update]
    Q --> S{pendingRestart true?}
    S -- Yes --> T[Clear pendingRestart and startLaunchThread]
    S -- No --> U{Old state TERMINATING and reason ABORT?}
    U -- Yes --> V[startLaunchThread for crash restart]
    U -- No --> W[No relaunch]

    A --> X[OnAppInstalled for reference app]
    X --> Y{m_appLaunched true?}
    Y -- Yes --> Z[Set pendingRestart true and killReferenceApp]
    Y -- No --> AA[No restart needed now]
    Z --> AB[On UNLOADED event restart launches new version]

    A --> AC{Termination signal or shutdown}
    AC --> AD[onTerminate set active false and notify]
    AD --> AE[stopDownloadMonitorThread]
    AE --> AF[unregister App and Preinstall events]
    AF --> AG[Destructor final cleanup]
```

## Keep Diagram Editable and Generate PNG

Use Mermaid source as the single source of truth, then regenerate PNG whenever needed.

1. Save each diagram into Mermaid source files such as `docs/sceneset-boot-preinstall.mmd` and `docs/sceneset-runtime-update.mmd` if you want standalone render inputs.
2. Generate PNG from Mermaid sources:

```bash
npx @mermaid-js/mermaid-cli -i docs/sceneset-boot-preinstall.mmd -o docs/sceneset-boot-preinstall.png -t default -b white -s 2
npx @mermaid-js/mermaid-cli -i docs/sceneset-runtime-update.mmd -o docs/sceneset-runtime-update.png -t default -b white -s 2
```

3. Embed the generated PNG where needed.
4. For future updates, edit only Mermaid source and regenerate PNG.
