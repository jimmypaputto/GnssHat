/*
 * Jimmy Paputto 2026
 *
 * Configuration for ntrip-caster — TOML file + CLI override.
 */

#ifndef NTRIP_CASTER_CONFIG_HPP_
#define NTRIP_CASTER_CONFIG_HPP_

#include <cstdint>
#include <stdexcept>
#include <string>

#define TOML_HEADER_ONLY 1
#include "toml.hpp"

#include "NtripLog.hpp"

namespace JimmyPaputto
{

    struct CasterConfig
    {
        // [ntrip]
        std::string host           = "0.0.0.0";
        uint16_t    port           = 2101;
        size_t      maxClients     = 64;
        std::string user;          // empty = open access
        std::string pass;
        std::string tlsCert;
        std::string tlsKey;
        std::string logLevel       = "info";
        int         statsInterval  = 30;

        // [http]
        bool        httpEnabled    = false;
        std::string httpHost       = "0.0.0.0";
        uint16_t    httpPort       = 8080;
        std::string httpUser       = "rtk";
        std::string httpPass       = "rtkpassword";
        std::string httpRealm      = "ntrip-caster";
        std::string httpWebRoot;   // empty → resolved at runtime

        /// Load values from a TOML file.  Missing keys keep their defaults.
        /// Throws std::runtime_error on parse error.
        void loadFromToml(const std::string& path)
        {
            toml::table tbl;
            try
            {
                tbl = toml::parse_file(path);
            }
            catch (const toml::parse_error& e)
            {
                throw std::runtime_error(
                    std::string("config: parse error in ") + path +
                    ": " + std::string(e.description()));
            }

            if (auto n = tbl["ntrip"].as_table())
            {
                if (auto v = (*n)["host"].value<std::string>())          host           = *v;
                if (auto v = (*n)["port"].value<int64_t>())              port           = static_cast<uint16_t>(*v);
                if (auto v = (*n)["max_clients"].value<int64_t>())       maxClients     = static_cast<size_t>(*v);
                if (auto v = (*n)["user"].value<std::string>())          user           = *v;
                if (auto v = (*n)["pass"].value<std::string>())          pass           = *v;
                if (auto v = (*n)["tls_cert"].value<std::string>())      tlsCert        = *v;
                if (auto v = (*n)["tls_key"].value<std::string>())       tlsKey         = *v;
                if (auto v = (*n)["log_level"].value<std::string>())     logLevel       = *v;
                if (auto v = (*n)["stats_interval"].value<int64_t>())    statsInterval  = static_cast<int>(*v);
            }

            if (auto h = tbl["http"].as_table())
            {
                if (auto v = (*h)["enabled"].value<bool>())              httpEnabled    = *v;
                if (auto v = (*h)["host"].value<std::string>())          httpHost       = *v;
                if (auto v = (*h)["port"].value<int64_t>())              httpPort       = static_cast<uint16_t>(*v);
                if (auto v = (*h)["user"].value<std::string>())          httpUser       = *v;
                if (auto v = (*h)["pass"].value<std::string>())          httpPass       = *v;
                if (auto v = (*h)["realm"].value<std::string>())         httpRealm      = *v;
                if (auto v = (*h)["web_root"].value<std::string>())      httpWebRoot    = *v;
            }
        }
    };

    inline ENtripLogLevel parseLogLevelString(const std::string& s)
    {
        if (s == "error")   return ENtripLogLevel::Error;
        if (s == "warning") return ENtripLogLevel::Warning;
        if (s == "info")    return ENtripLogLevel::Info;
        if (s == "debug")   return ENtripLogLevel::Debug;
        return ENtripLogLevel::Info;
    }

}

#endif // NTRIP_CASTER_CONFIG_HPP_
