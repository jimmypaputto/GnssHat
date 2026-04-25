/*
 * Jimmy Paputto 2026
 *
 * EphemerisStore — common storage for decoded broadcast ephemerides
 * across GPS / Galileo / BeiDou / GLONASS, plus satellite-state
 * computation for the SPP solver.
 *
 * A single, flat Keplerian record covers GPS CNAV, Galileo I/NAV and
 * BeiDou D1. GLONASS uses a position/velocity state vector and is
 * stored separately.
 */

#ifndef EPHEMERIS_STORE_HPP_
#define EPHEMERIS_STORE_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <jimmypaputto/GnssHat.hpp>

#include "GnssMath.hpp"

namespace EphemerisStore
{

// ── Kepler-style ephemeris (GPS CNAV / Galileo I/NAV / BeiDou D1) ──
struct Kepler
{
    JimmyPaputto::EGnssId gnss;
    uint8_t svId      = 0;
    bool    valid     = false;
    uint32_t rxEpoch  = 0;   // receiver epoch when fully assembled (arbitrary units)

    // Clock polynomial
    double toc        = 0;   // time of clock [s of GNSS week]
    double af0        = 0;   // [s]
    double af1        = 0;   // [s/s]
    double af2        = 0;   // [s/s^2]
    double tgd        = 0;   // group delay [s] (GPS: TGD L1-L5 or ISC; GAL: BGD E1-E5b; BDS: TGD1)

    // Orbit (ICD-common)
    double toe        = 0;   // time of ephemeris [s of GNSS week]
    double sqrtA      = 0;   // [sqrt(m)]
    double ecc        = 0;   // eccentricity
    double M0         = 0;   // mean anomaly at toe [rad]
    double omega      = 0;   // argument of perigee [rad]
    double Omega0     = 0;   // longitude of ascending node [rad]
    double OmegaDot   = 0;   // rate of RAAN [rad/s]
    double i0         = 0;   // inclination at toe [rad]
    double iDot       = 0;   // rate of inclination [rad/s]
    double deltaN     = 0;   // mean motion correction [rad/s]
    double cus        = 0;   // argument-of-latitude sin correction [rad]
    double cuc        = 0;   //                     cos correction [rad]
    double crs        = 0;   // radius sin correction [m]
    double crc        = 0;   // radius cos correction [m]
    double cis        = 0;   // inclination sin correction [rad]
    double cic        = 0;   // inclination cos correction [rad]

    double muRef      = GnssMath::MU_GPS;   // per-GNSS GM
    double omegaERef  = GnssMath::OMEGA_E;  // per-GNSS earth rotation
};

// ── GLONASS state vector (PZ-90.02 ECEF, tb referenced) ────────────
struct Glonass
{
    uint8_t svId     = 0;
    int8_t  freqSlot = 0;    // k, -7..+6
    bool    valid    = false;
    uint32_t rxEpoch = 0;

    double tb        = 0;    // time of day [s, MSK UTC]
    double tauN      = 0;    // clock bias [s]
    double gammaN    = 0;    // rel. freq deviation

    // Position [m], velocity [m/s], acceleration (luni-solar) [m/s^2] at tb
    double x=0,y=0,z=0;
    double vx=0,vy=0,vz=0;
    double ax=0,ay=0,az=0;
};

// ── Iono / UTC parameters (GPS broadcast; optional) ─────────────────
struct IonoKlobuchar
{
    bool valid = false;
    double alpha[4] = {0,0,0,0};
    double beta [4] = {0,0,0,0};
};

// ── Computed satellite state at signal transmission time ────────────
struct SatState
{
    JimmyPaputto::EGnssId gnss;
    uint8_t svId;
    GnssMath::Ecef pos;   // ECEF at tx time, earth-rotation-corrected to rx frame
    double clockBias;     // [s], relativistic+polynomial, signed for subtract-from-range
    double tgd;           // [s]
};

class Store
{
public:
    // Insert / replace ephemeris. Thread-safe.
    void put(const Kepler& k)
    {
        std::lock_guard<std::mutex> lk(m_);
        kep_[key(k.gnss, k.svId)] = k;
        if (std::getenv("SPP_DEBUG_EPH"))
        {
            std::fprintf(stderr,
                "[EPH] put gnss=%d sv=%-3u toe=%-7.0f toc=%-7.0f "
                "sqrtA=%.4f e=%.6f i0=%.6f Om0=%.6f w=%.6f M0=%.6f "
                "af0=%.3e af1=%.3e tgd=%.3e\n",
                static_cast<int>(k.gnss), k.svId,
                k.toe, k.toc,
                k.sqrtA, k.ecc, k.i0, k.Omega0, k.omega, k.M0,
                k.af0, k.af1, k.tgd);
        }
    }
    void put(const Glonass& g)
    {
        std::lock_guard<std::mutex> lk(m_);
        glo_[g.svId] = g;
    }
    void putIono(const IonoKlobuchar& i)
    {
        std::lock_guard<std::mutex> lk(m_);
        iono_ = i;
    }

    size_t countValid() const
    {
        std::lock_guard<std::mutex> lk(m_);
        size_t n = 0;
        for (const auto& [_, e] : kep_) if (e.valid) ++n;
        for (const auto& [_, e] : glo_) if (e.valid) ++n;
        return n;
    }

    size_t countValid(JimmyPaputto::EGnssId g) const
    {
        std::lock_guard<std::mutex> lk(m_);
        size_t n = 0;
        if (g == JimmyPaputto::EGnssId::GLONASS)
        {
            for (const auto& [_, e] : glo_) if (e.valid) ++n;
            return n;
        }
        for (const auto& [k, e] : kep_)
            if (e.valid && e.gnss == g) ++n;
        return n;
    }

    std::optional<Kepler>  getKepler (JimmyPaputto::EGnssId g, uint8_t sv) const
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = kep_.find(key(g, sv));
        if (it == kep_.end() || !it->second.valid) return std::nullopt;
        return it->second;
    }
    std::optional<Glonass> getGlonass(uint8_t sv) const
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = glo_.find(sv);
        if (it == glo_.end() || !it->second.valid) return std::nullopt;
        return it->second;
    }
    IonoKlobuchar iono() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return iono_;
    }

    // For diagnostics.
    std::vector<std::pair<JimmyPaputto::EGnssId, uint8_t>> validKeys() const
    {
        std::lock_guard<std::mutex> lk(m_);
        std::vector<std::pair<JimmyPaputto::EGnssId, uint8_t>> v;
        for (const auto& [_, e] : kep_) if (e.valid) v.push_back({e.gnss, e.svId});
        for (const auto& [_, e] : glo_) if (e.valid) v.push_back({JimmyPaputto::EGnssId::GLONASS, e.svId});
        return v;
    }

private:
    static uint32_t key(JimmyPaputto::EGnssId g, uint8_t sv)
    { return (static_cast<uint32_t>(g) << 8) | sv; }

    mutable std::mutex m_;
    std::unordered_map<uint32_t, Kepler>  kep_;
    std::unordered_map<uint8_t,  Glonass> glo_;
    IonoKlobuchar iono_;
};

// ── Kepler → SatState (GPS/GAL/BDS common) ──────────────────────────
//
// Standard ICD orbit algorithm. txTime is SV transmission time in the
// GNSS native time frame (seconds of week for GPS/GAL; BDS sec of week
// from BDT epoch with 14s offset handled upstream).
//
inline SatState propagateKepler(const Kepler& e, double txTime)
{
    const double A  = e.sqrtA * e.sqrtA;
    const double n0 = std::sqrt(e.muRef / (A*A*A));
    const double n  = n0 + e.deltaN;

    double tk = txTime - e.toe;
    if      (tk >  302400.0) tk -= 604800.0;
    else if (tk < -302400.0) tk += 604800.0;

    const double Mk = e.M0 + n * tk;

    // Solve Kepler E - e sin E = M
    double Ek = Mk;
    for (int i = 0; i < 10; ++i)
        Ek = Mk + e.ecc * std::sin(Ek);

    const double sinE = std::sin(Ek), cosE = std::cos(Ek);
    const double nu = std::atan2(std::sqrt(1 - e.ecc*e.ecc) * sinE,
                                 cosE - e.ecc);
    const double phi = nu + e.omega;
    const double s2 = std::sin(2*phi), c2 = std::cos(2*phi);

    const double u  = phi + e.cus*s2 + e.cuc*c2;
    const double r  = A*(1 - e.ecc*cosE) + e.crs*s2 + e.crc*c2;
    const double ik = e.i0 + e.iDot*tk + e.cis*s2 + e.cic*c2;

    const double xp = r * std::cos(u);
    const double yp = r * std::sin(u);

    const double Omk = e.Omega0 + (e.OmegaDot - e.omegaERef) * tk
                     - e.omegaERef * e.toe;

    const double cosOm = std::cos(Omk), sinOm = std::sin(Omk);
    const double cosi = std::cos(ik),   sini  = std::sin(ik);

    SatState s;
    s.gnss  = e.gnss;
    s.svId  = e.svId;

    // BDS GEO satellites (PRN 1-5, 59, 60, 61) use a different frame:
    // ICD-B1I §5.2.4.12. Compute position in inertial-like frame and
    // rotate by R_X(-5°) * R_Z(omega_e * tk).
    const bool bdsGeo = (e.gnss == JimmyPaputto::EGnssId::BeiDou)
        && (e.svId <= 5 || (e.svId >= 59 && e.svId <= 61));
    if (bdsGeo)
    {
        const double OmkG = e.Omega0 + e.OmegaDot * tk - e.omegaERef * e.toe;
        const double cosG = std::cos(OmkG), sinG = std::sin(OmkG);
        const double xg = xp*cosG - yp*cosi*sinG;
        const double yg = xp*sinG + yp*cosi*cosG;
        const double zg = yp*sini;
        const double phi = e.omegaERef * tk;
        const double cp = std::cos(phi), sp = std::sin(phi);
        const double c5 = std::cos(-5.0 * GnssMath::DEG2RAD);
        const double s5 = std::sin(-5.0 * GnssMath::DEG2RAD);
        // R_Z(phi) * R_X(-5°)
        s.pos.x =  cp*xg + sp*c5*yg + sp*s5*zg;
        s.pos.y = -sp*xg + cp*c5*yg + cp*s5*zg;
        s.pos.z =          -s5*yg + c5*zg;
    }
    else
    {
        s.pos.x = xp*cosOm - yp*cosi*sinOm;
        s.pos.y = xp*sinOm + yp*cosi*cosOm;
        s.pos.z = yp*sini;
    }

    // Clock bias: polynomial + relativistic
    const double dtsv = e.af0 + e.af1*(txTime - e.toc)
                      + e.af2*(txTime - e.toc)*(txTime - e.toc);
    const double F = -4.442807633e-10;  // -2*sqrt(mu)/c^2 (GPS value; ok approx)
    const double dtRel = F * e.ecc * e.sqrtA * sinE;
    s.clockBias = dtsv + dtRel;
    s.tgd       = e.tgd;
    return s;
}

// ── GLONASS RK4 propagation (PE-90 → ECEF handled by caller) ────────
inline SatState propagateGlonass(const Glonass& g, double txTime)
{
    // dt relative to tb (same day). Handle wrap roughly.
    double dt = txTime - g.tb;
    if      (dt >  43200.0) dt -= 86400.0;
    else if (dt < -43200.0) dt += 86400.0;

    const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(dt) / 30.0)));
    const double h  = dt / steps;

    double x[6] = { g.x, g.y, g.z, g.vx, g.vy, g.vz };
    const double a[3] = { g.ax, g.ay, g.az };

    auto deriv = [&](const double s[6], double ds[6]) {
        constexpr double mu  = GnssMath::MU_GLO;
        constexpr double ae  = 6378136.0;
        constexpr double J20 = 1.08262575e-3;
        const double r2 = s[0]*s[0] + s[1]*s[1] + s[2]*s[2];
        const double r  = std::sqrt(r2);
        const double rho = ae / r;
        const double mur3 = mu / (r2 * r);
        const double k  = 1.5 * J20 * mur3 * rho * rho;
        const double zr2 = 5.0 * s[2]*s[2] / r2;
        ds[0] = s[3]; ds[1] = s[4]; ds[2] = s[5];
        ds[3] = -mur3*s[0] + k*s[0]*(1 - zr2) + a[0]
              + 2*GnssMath::OMEGA_GLO*s[4] + GnssMath::OMEGA_GLO*GnssMath::OMEGA_GLO*s[0];
        ds[4] = -mur3*s[1] + k*s[1]*(1 - zr2) + a[1]
              - 2*GnssMath::OMEGA_GLO*s[3] + GnssMath::OMEGA_GLO*GnssMath::OMEGA_GLO*s[1];
        ds[5] = -mur3*s[2] + k*s[2]*(3 - zr2) + a[2];
    };

    for (int step = 0; step < steps; ++step)
    {
        double k1[6], k2[6], k3[6], k4[6], tmp[6];
        deriv(x, k1);
        for (int i=0;i<6;++i) tmp[i] = x[i] + 0.5*h*k1[i]; deriv(tmp, k2);
        for (int i=0;i<6;++i) tmp[i] = x[i] + 0.5*h*k2[i]; deriv(tmp, k3);
        for (int i=0;i<6;++i) tmp[i] = x[i] +     h*k3[i]; deriv(tmp, k4);
        for (int i=0;i<6;++i) x[i] += (h/6.0)*(k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);
    }

    SatState s;
    s.gnss = JimmyPaputto::EGnssId::GLONASS;
    s.svId = g.svId;
    s.pos  = { x[0], x[1], x[2] };
    s.clockBias = -g.tauN + g.gammaN * dt;   // GLONASS convention
    s.tgd  = 0.0;
    return s;
}

}  // namespace EphemerisStore

#endif  // EPHEMERIS_STORE_HPP_
