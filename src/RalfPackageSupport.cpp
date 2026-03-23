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
#include <memory>
#include <mutex>
#include <sys/stat.h>

#if __has_include(<ralf/Package.h>)
#include <ralf/Package.h>
#include <ralf/Certificate.h>
#elif __has_include(<Package.h>)
#include <Package.h>
#include <Certificate.h>
#else
#error "libralf Package.h header not found. Ensure libralf is installed and its include path is configured."
#endif

namespace ralf = LIBRALF_NS;

namespace {
struct CertCacheEntry {
    std::filesystem::path dir;
    time_t mtime{0};
    bool valid{false};
    std::shared_ptr<ralf::VerificationBundle> bundle;
    size_t certCount{0};
};
std::mutex g_certCacheMutex;
CertCacheEntry g_certCache;
} // namespace

namespace ralf_support {

bool ExtractPackageMetadata(const std::filesystem::path& packagePath,
                            const std::filesystem::path& certDir,
                            std::string& packageAppId,
                            std::string& packageVersion)
{
    packageAppId.clear();
    packageVersion.clear();

    std::shared_ptr<ralf::VerificationBundle> verificationBundlePtr;
    size_t certCount = 0;

    struct ::stat dirStat{};
    const bool dirStatOk = (::stat(certDir.string().c_str(), &dirStat) == 0);
    const time_t certDirMtime = dirStatOk ? dirStat.st_mtime : 0;
    bool needRebuild = false;

    {
        std::lock_guard<std::mutex> lock(g_certCacheMutex);
        if (dirStatOk && g_certCache.valid &&
            g_certCache.dir == certDir && g_certCache.mtime == certDirMtime) {
            // Cache hit: reuse the already-loaded bundle.
            verificationBundlePtr = g_certCache.bundle;
            certCount = g_certCache.certCount;
        } else {
            needRebuild = true;
        }
    }

    std::shared_ptr<ralf::VerificationBundle> newBundle;
    size_t newCertCount = 0;
    if (!verificationBundlePtr && needRebuild) {
        newBundle = std::make_shared<ralf::VerificationBundle>();

        try {
            std::error_code certEc;
            if (std::filesystem::exists(certDir, certEc) && !certEc &&
                std::filesystem::is_directory(certDir, certEc) && !certEc) {
                std::filesystem::directory_iterator it(
                    certDir,
                    std::filesystem::directory_options::skip_permission_denied,
                    certEc);
                if (certEc) {
                    std::cerr << "Error while opening cert directory " << certDir << ": " << certEc.message() << std::endl;
                }

                std::filesystem::directory_iterator end;
                for (; it != end; it.increment(certEc)) {
                    if (certEc) {
                        std::cerr << "Error while scanning cert directory " << certDir << ": " << certEc.message() << std::endl;
                        break;
                    }

                    const auto& dirEntry = *it;

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

                    newBundle->addCertificate(certResult.value());
                    ++newCertCount;
                }
            }
        } catch (const std::filesystem::filesystem_error& fsError) {
            std::cerr << "Filesystem error while loading certificates from " << certDir << ": " << fsError.what() << std::endl;
            return false;
        }
    }

    if (!verificationBundlePtr) {
        std::lock_guard<std::mutex> lock(g_certCacheMutex);
        if (dirStatOk && g_certCache.valid &&
            g_certCache.dir == certDir && g_certCache.mtime == certDirMtime) {
            // Another thread refreshed the cache while we were rebuilding.
            verificationBundlePtr = g_certCache.bundle;
            certCount = g_certCache.certCount;
        } else if (newBundle) {
            if (dirStatOk) {
                g_certCache.dir = certDir;
                g_certCache.mtime = certDirMtime;
                g_certCache.valid = true;
                g_certCache.bundle = newBundle;
                g_certCache.certCount = newCertCount;
            }
            verificationBundlePtr = std::move(newBundle);
            certCount = newCertCount;
        }
    }

    if (certCount == 0) {
        std::cerr << "No certificates loaded from " << certDir << ". Cannot verify package: " << packagePath << std::endl;
        return false;
    }

    auto packageResult = ralf::Package::open(packagePath, *verificationBundlePtr, ralf::Package::OpenFlags::CheckCertificateExpiry);
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

bool ExtractPackageMetadata(const std::filesystem::path& packagePath,
                            std::string& packageAppId,
                            std::string& packageVersion)
{
#ifndef DAC_APP_CERT_PATH
#error "DAC_APP_CERT_PATH must be defined, (CMake default: /etc/rdk/certs)."
#endif
    static const std::filesystem::path kCertDir(DAC_APP_CERT_PATH);
    return ExtractPackageMetadata(packagePath, kCertDir, packageAppId, packageVersion);
}

} // namespace ralf_support
