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

#include "RalfPackageSupport.h"

#include <filesystem>
#include <iostream>

#if __has_include(<ralf/Package.h>)
#include <ralf/Package.h>
#include <ralf/Certificate.h>
#elif __has_include(<Package.h>)
#include <Package.h>
#include <Certificate.h>
#endif

namespace ralf = LIBRALF_NS;

namespace ralf_support {

bool ExtractPackageMetadata(const std::filesystem::path& packagePath,
                            const std::filesystem::path& certDir,
                            std::string& packageAppId,
                            std::string& packageVersion)
{
    packageAppId.clear();
    packageVersion.clear();

    ralf::VerificationBundle verificationBundle;
    size_t certCount = 0;

    try {
        std::error_code certEc;
        if (std::filesystem::exists(certDir, certEc) && !certEc &&
            std::filesystem::is_directory(certDir, certEc) && !certEc) {
            for (const auto& dirEntry : std::filesystem::directory_iterator(certDir, std::filesystem::directory_options::skip_permission_denied, certEc)) {
                if (certEc) {
                    std::cerr << "Error while scanning cert directory " << certDir << ": " << certEc.message() << std::endl;
                    break;
                }

                std::error_code entryEc;
                if (!dirEntry.is_regular_file(entryEc)) {
                    if (entryEc) {
                        std::cerr << "Skipping cert entry due to stat error " << dirEntry.path() << ": " << entryEc.message() << std::endl;
                    }
                    continue;
                }

                auto certResult = ralf::Certificate::loadFromFile(dirEntry.path().string());
                if (certResult.is_error()) {
                    std::cerr << "Failed to load certificate from file: " << dirEntry.path()
                              << " Error: " << certResult.error().what() << std::endl;
                    continue;
                }

                verificationBundle.addCertificate(certResult.value());
                ++certCount;
            }
        }
    } catch (const std::filesystem::filesystem_error& fsError) {
        std::cerr << "Filesystem error while loading certificates from " << certDir << ": " << fsError.what() << std::endl;
        return false;
    }

    if (certCount == 0) {
        std::cerr << "No certificates loaded from " << certDir << ". Cannot verify package: " << packagePath << std::endl;
        return false;
    }

    auto packageResult = ralf::Package::open(packagePath, verificationBundle, ralf::Package::OpenFlags::CheckCertificateExpiry);
    if (packageResult.is_error()) {
        std::cerr << "Failed to open/verify package with libralf: " << packagePath
                  << " Error: " << packageResult.error().what() << std::endl;
        return false;
    }

    auto metadataResult = packageResult.value().metaData();
    if (metadataResult.is_error()) {
        std::cerr << "Failed to parse package metadata with libralf: " << packagePath
                  << " Error: " << metadataResult.error().what() << std::endl;
        return false;
    }

    const auto& metadata = metadataResult.value();
    if (!metadata.isValid()) {
        return false;
    }

    packageAppId = metadata.id();
    packageVersion = metadata.version().toString();
    if (packageVersion.empty()) {
        packageVersion = metadata.versionName();
    }

    return !packageAppId.empty();
}

} // namespace ralf_support
