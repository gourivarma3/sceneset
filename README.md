# SceneSet

SceneSet is an application launcher service for RDK based Set-Top Boxes that automatically launches the rdk refernce application during system boot up. It provides a reliable mechanism to start reference application using the WPEFramework AppManager interface.

## Overview

SceneSet is designed to run as a systemd service that:
- Initializes communication with the Thunder/WPEFramework
- Registers for reference application lifecycle events via AppManager
- Automatically launches a specified reference application
- Monitors reference application state changes and restart the app if it crashes
- Handles graceful shutdown via signal handling

## Features

- **Automatic App Launch**: Launches the rdk reference application specified via environment variable
- **Event Monitoring**: Registers for and logs AppManager lifecycle events
- **Signal Handling**: Graceful shutdown on SIGTERM signals
- **Thunder Integration**: Uses WPEFramework COMRPC for AppManager communication

## Architecture

SceneSet architecture, detailed flows, and PNG regeneration steps are documented in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Configuration

### Environment Variables

- **`SCENESET_DEFAULT_APPNAME`**: Specifies the application to launch at startup

- **`THUNDER_ACCESS`**: Optional path to Thunder communicator socket
  - Default: `/tmp/communicator`

## Build Instructions

The project uses CMake for building

### Dependencies

- **WPEFramework**: Core framework and interfaces
- **gtest/gmock**: For unit testing

## Service Dependencies

SceneSet depends on the AppManager service:
- **Requires**: `wpeframework-appmanager.service`
- **After**: `wpeframework-appmanager.service`

Ensure AppManager is running before starting SceneSet.

## License

Licensed under the Apache License, Version 2.0. See the source files for full license text.

## Copyright

Copyright 2024 RDK Management
