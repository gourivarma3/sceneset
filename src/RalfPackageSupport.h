/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#ifndef RALF_PACKAGE_SUPPORT_H
#define RALF_PACKAGE_SUPPORT_H

#include <filesystem>
#include <string>

namespace ralf_support {

bool ExtractPackageMetadata(const std::filesystem::path& packagePath,
                           const std::filesystem::path& certDir,
                           std::string& packageAppId,
                           std::string& packageVersion);

// Convenience overload: uses the cert directory compiled in via DAC_APP_CERT_PATH.
bool ExtractPackageMetadata(const std::filesystem::path& packagePath,
                           std::string& packageAppId,
                           std::string& packageVersion);

} // namespace ralf_support

#endif // RALF_PACKAGE_SUPPORT_H
