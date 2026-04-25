/*
 * Jimmy Paputto 2026
 *
 * Single Point Positioning (SPP) — weighted least-squares solver.
 *
 * --------------------------------------------------------------------
 * INPUT
 * --------------------------------------------------------------------
 * - RawMeasurements (UBX-RXM-RAWX): rcvTow [s of GPS week], week,
 *   per-SV pseudorange `prMes` and signal/SV identification.
 * - EphemerisStore: per-SV broadcast ephemerides (Kepler for GPS/GAL/
 *   BDS; state-vector for GLONASS).
 *
 * --------------------------------------------------------------------
 * ALGORITHM
 * --------------------------------------------------------------------
 *  1. For each observation pick the best signal per (gnss, sv) pair.
 *  2. Convert receiver TOW → SV transmission time per GNSS:
 *        GPS / GAL: txTime = rcvTow - prMes / c            (GPS time)
 *        BDS:       txTime = rcvTow - 14 - prMes / c       (BDT = GPS-14s)
 *        GLO:       txTime = rcvTow - leapS - prMes / c    (UTC seconds of day)
 *  3. propagateKepler() / propagateGlonass() → SV ECEF + clock bias.
 *  4. Apply Sagnac (Earth rotation) correction over signal travel time
 *     so SV position is in the rx-frame at signal reception time.
 *  5. Tropo (Saastamoinen + simple mapping).  Iono = 0 (no Klobuchar
 *     coefficients available from CFG-SIGNAL set; left as TODO).
 *  6. Iterate WLS:
 *        residual = prMes - geomRange + c*satClkBias - tropo - iono - c*rxClkBias_g
 *        weight   = sin²(elev) (after first iteration; uniform first pass)
 *     System size = Nobs × (3 + Ngnss).  Each GNSS gets its own clock-
 *     bias unknown to absorb inter-system biases.
 *
 * --------------------------------------------------------------------
 * OUTPUT
 * --------------------------------------------------------------------
 * Solution { ECEF, LLA, clock biases per GNSS, used SVs, residual RMS }.
 */

#ifndef SPP_SOLVER_HPP_
#define SPP_SOLVER_HPP_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>

#include "EphemerisStore.hpp"
#include "GnssMath.hpp"

namespace Spp
{

struct Solution
{
    bool valid = false;
    GnssMath::Ecef ecef{0,0,0};
    GnssMath::Lla  lla {0,0,0};
    double clkG = 0;     // [m] receiver clock bias (light-seconds × c)
    double clkE = 0;
    double clkC = 0;
    double clkR = 0;
    int    nUsed = 0;
    int    nCandidates = 0;
    int    nDropped = 0;
    double rmsResidual = 0;   // [m]
    int    iterations = 0;
};

// Accept any signal carrying a valid pseudorange. Highest-CNO signal
// per (gnss,sv) wins. Inter-signal biases (TGD differences) are sub-
// metre and absorbed into the receiver clock unknowns.
inline bool isUsableSignal(JimmyPaputto::EGnssId /*g*/, uint8_t /*sigId*/)
{
    return true;
}

// Saastamoinen-lite zenith tropospheric delay [m] mapped by elevation.
// Standard atmosphere (P=1013.25 hPa, T=288.15 K, e=11.69 hPa).
inline double tropoSaastamoinen(double elevRad, double altMSL)
{
    if (elevRad < 5.0 * GnssMath::DEG2RAD) elevRad = 5.0 * GnssMath::DEG2RAD;
    const double P  = 1013.25 * std::pow(1 - 2.2557e-5 * altMSL, 5.2568);
    const double T  = 288.15  - 6.5e-3 * altMSL;
    const double e  = 11.691  * std::pow(1 - 2.2557e-5 * altMSL, 5.2568);
    const double zhd = 0.0022768 * P;
    const double zwd = 0.0022768 * (1255.0 / T + 0.05) * e;
    const double m   = 1.0 / std::sin(elevRad);
    return (zhd + zwd) * m;
}

// Klobuchar single-frequency iono delay [m] at the broadcast reference
// frequency. Caller scales by (f_ref/f_signal)^2 for a different band.
// Reference: GPS-IS-200 §20.3.3.5.2.5 (same form for BDS B1I).
//   `gpsTow` is GPS seconds-of-week at signal reception.
inline double klobucharDelay(const EphemerisStore::IonoKlobuchar& iono,
                             const GnssMath::Lla& rxLla,
                             double azimRad, double elevRad,
                             double gpsTow)
{
    if (!iono.valid) return 0.0;

    const double phiU = rxLla.lat_rad / GnssMath::PI;          // semicircles
    const double lamU = rxLla.lon_rad / GnssMath::PI;
    const double E    = elevRad / GnssMath::PI;            // semicircles
    const double A    = azimRad;

    double psi = 0.0137 / (E + 0.11) - 0.022;              // semicircles
    double phiI = phiU + psi * std::cos(A);
    if (phiI >  0.416) phiI =  0.416;
    if (phiI < -0.416) phiI = -0.416;
    const double cosPhiI = std::cos(phiI * GnssMath::PI);
    double lamI = lamU + psi * std::sin(A) / cosPhiI;
    double phiM = phiI + 0.064 * std::cos((lamI - 1.617) * GnssMath::PI);

    double t = 4.32e4 * lamI + gpsTow;
    t = std::fmod(t, 86400.0);
    if (t < 0) t += 86400.0;

    double AMP = iono.alpha[0] + phiM*(iono.alpha[1]
                + phiM*(iono.alpha[2] + phiM*iono.alpha[3]));
    if (AMP < 0.0) AMP = 0.0;
    double PER = iono.beta[0] + phiM*(iono.beta[1]
                + phiM*(iono.beta[2] + phiM*iono.beta[3]));
    if (PER < 72000.0) PER = 72000.0;

    const double x = 2.0 * GnssMath::PI * (t - 50400.0) / PER;
    const double F = 1.0 + 16.0 * std::pow(0.53 - E, 3.0);

    double Tsec;
    if (std::fabs(x) < 1.57)
        Tsec = F * (5e-9 + AMP * (1 - x*x/2.0 + x*x*x*x/24.0));
    else
        Tsec = F * 5e-9;
    return Tsec * GnssMath::C;     // metres
}

// Per-GNSS index used to assign clock-bias columns.
inline int gnssIdx(JimmyPaputto::EGnssId g)
{
    using G = JimmyPaputto::EGnssId;
    switch (g)
    {
        case G::GPS:     return 0;
        case G::Galileo: return 1;
        case G::BeiDou:  return 2;
        case G::GLONASS: return 3;
        default:         return -1;
    }
}

// ── Tiny dense linear algebra (square solver, Gaussian elimination) ──
// Solve A x = b where A is N x N row-major. Returns false on singular.
inline bool solveLinear(std::vector<double>& A, std::vector<double>& b, int N)
{
    auto a = [&](int r, int c) -> double& { return A[r*N + c]; };
    for (int k = 0; k < N; ++k)
    {
        // partial pivot
        int piv = k;
        double best = std::fabs(a(k,k));
        for (int r = k+1; r < N; ++r)
            if (std::fabs(a(r,k)) > best) { best = std::fabs(a(r,k)); piv = r; }
        if (best < 1e-12) return false;
        if (piv != k)
        {
            for (int c = k; c < N; ++c) std::swap(a(k,c), a(piv,c));
            std::swap(b[k], b[piv]);
        }
        for (int r = k+1; r < N; ++r)
        {
            const double f = a(r,k) / a(k,k);
            for (int c = k; c < N; ++c) a(r,c) -= f * a(k,c);
            b[r] -= f * b[k];
        }
    }
    for (int r = N-1; r >= 0; --r)
    {
        double s = b[r];
        for (int c = r+1; c < N; ++c) s -= a(r,c) * b[c];
        b[r] = s / a(r,r);
    }
    return true;
}

// ── Solver ────────────────────────────────────────────────────────
inline Solution solve(const JimmyPaputto::RawMeasurements& raw,
                      const EphemerisStore::Store& store,
                      const GnssMath::Ecef& initialEcef = {0,0,0})
{
    using G = JimmyPaputto::EGnssId;
    Solution out{};
    if (raw.observations.empty()) return out;

    constexpr double C_M = GnssMath::C;
    const double rcvTow  = raw.rcvTow;
    const double leapS   = raw.leapS;
    const auto   iono    = store.iono();

    // Reference Klobuchar frequency. BDS B1I = 1561.098 MHz; we scale to
    // the receiver's signal at evaluation time. (BDS-SIS-ICD §5.2.4.7.)
    constexpr double F_BDS_B1I = 1561.098e6;
    constexpr double F_GPS_L1  = 1575.42e6;
    constexpr double F_GAL_E1  = 1575.42e6;
    constexpr double F_GLO_L1  = 1602.0e6;     // FDMA centre, channel-agnostic

    // 1. Pick best obs per (gnss, sv). Prefer primary signal; fall back
    //    to any signal if none match (rare).
    struct Candidate
    {
        JimmyPaputto::EGnssId gnss;
        uint8_t svId;
        double  prMes;
        double  cno;
    };
    std::map<uint32_t, Candidate> chosen;
    for (const auto& o : raw.observations)
    {
        if (!o.prValid)        continue;
        if (o.cno < 25)        continue;
        if (!isUsableSignal(o.gnssId, o.sigId)) continue;
        const uint32_t k = (static_cast<uint32_t>(o.gnssId) << 8) | o.svId;
        auto it = chosen.find(k);
        if (it == chosen.end() || o.cno > it->second.cno)
            chosen[k] = {o.gnssId, o.svId, o.prMes, static_cast<double>(o.cno)};
    }

    // 2. Resolve SV state for each candidate.
    struct Row
    {
        JimmyPaputto::EGnssId gnss;
        uint8_t svId;
        double prMes;       // [m]
        GnssMath::Ecef satEcef;
        double satClkM;     // [m] (= c * satClockBias)
        double cno;
    };
    std::vector<Row> rows;
    rows.reserve(chosen.size());

    for (const auto& [_, cand] : chosen)
    {
        // SV transmission time in own GNSS time frame.
        double txTime;
        switch (cand.gnss)
        {
            case G::GPS:
            case G::Galileo: txTime = rcvTow - cand.prMes / C_M;          break;
            case G::BeiDou:  txTime = rcvTow - 14.0 - cand.prMes / C_M;   break;
            case G::GLONASS: txTime = rcvTow - leapS - cand.prMes / C_M;  break;
            default: continue;
        }

        EphemerisStore::SatState st;
        if (cand.gnss == G::GLONASS)
        {
            auto eph = store.getGlonass(cand.svId);
            if (!eph) { ++out.nDropped; continue; }
            const double txDay = std::fmod(txTime, 86400.0);
            st = EphemerisStore::propagateGlonass(*eph, txDay);
        }
        else
        {
            auto eph = store.getKepler(cand.gnss, cand.svId);
            if (!eph) { ++out.nDropped; continue; }

            // Reject stale ephemerides. RTKLIB-style age window:
            //   GPS / GAL : 4 h
            //   BDS       : 1 h (GEO/IGSO orbit param drift is faster)
            // Fit interval beyond this gives unbounded error.
            double tk = txTime - eph->toe;
            if      (tk >  302400.0) tk -= 604800.0;
            else if (tk < -302400.0) tk += 604800.0;
            const double maxAge = (cand.gnss == G::BeiDou) ? 3600.0 : 14400.0;
            if (std::fabs(tk) > maxAge)
            {
                if (std::getenv("SPP_DEBUG"))
                {
                    std::fprintf(stderr,
                        "[SPP] stale eph gnss=%d sv=%u tk=%.0fs (max=%.0fs)\n",
                        static_cast<int>(cand.gnss), cand.svId, tk, maxAge);
                }
                ++out.nDropped;
                continue;
            }
            st = EphemerisStore::propagateKepler(*eph, txTime);
        }

        // Guard against NaN/garbage from bad ephemerides.
        if (!std::isfinite(st.pos.x) || !std::isfinite(st.pos.y) || !std::isfinite(st.pos.z)
            || !std::isfinite(st.clockBias))
        {
            ++out.nDropped;
            continue;
        }

        Row r;
        r.gnss     = cand.gnss;
        r.svId     = cand.svId;
        r.prMes    = cand.prMes;
        r.satEcef  = st.pos;
        r.satClkM  = C_M * (st.clockBias - st.tgd);
        r.cno      = cand.cno;
        rows.push_back(r);
    }

    if (rows.size() < 4)
    {
        out.nCandidates = static_cast<int>(chosen.size());
        out.nUsed       = static_cast<int>(rows.size());
        return out;
    }

    // Always populate diagnostic counters from this point onward, so
    // a downstream failure (WLS divergence, NaN, etc.) still reports
    // how many candidates we had to begin with.
    out.nCandidates = static_cast<int>(chosen.size());
    out.nUsed       = static_cast<int>(rows.size());

    // 3. Identify which GNSS-clock unknowns we need (one per active GNSS).
    std::array<int, 4> gnssCol = {-1,-1,-1,-1};
    int nGnss = 0;
    for (const auto& r : rows)
    {
        const int g = gnssIdx(r.gnss);
        if (g >= 0 && gnssCol[g] < 0) gnssCol[g] = nGnss++;
    }
    const int N = 3 + nGnss;
    if (static_cast<int>(rows.size()) < N) return out;

    // 4. WLS iteration. Two-pass with MAD-based outlier rejection.
    GnssMath::Ecef rx = (initialEcef.x*initialEcef.x + initialEcef.y*initialEcef.y
                        + initialEcef.z*initialEcef.z > 1e6)
                        ? initialEcef
                        : GnssMath::Ecef{0,0,0};
    std::vector<double> clk(nGnss, 0.0);
    std::vector<bool>   active(rows.size(), true);
    std::vector<double> lastResid(rows.size(), 0.0);

    constexpr int    MAX_ITER     = 15;
    constexpr double CONV_TOL     = 1e-3;
    constexpr double OUTLIER_HARD = 1000.0;     // [m] anything beyond this is junk
    constexpr double OUTLIER_K    = 6.0;        // [#MAD] robust outlier threshold
    double rms = 0.0;
    int    iter = 0;
    int    nMasked = 0;

    static const bool kDbg = std::getenv("SPP_DEBUG") != nullptr;
    const char* failReason = nullptr;
    int        failIter   = -1;
    int        failNvalid = -1;

    auto runWls = [&]() -> bool
    {
        rms  = 0.0;
        iter = 0;
        for (; iter < MAX_ITER; ++iter)
        {
            GnssMath::Lla rxLla{0,0,0};
            const double r2 = rx.x*rx.x + rx.y*rx.y + rx.z*rx.z;
            const bool   havePos = r2 > 1e10;
            if (havePos) rxLla = GnssMath::ecef2lla(rx);

            std::vector<double> ATA(N*N, 0.0);
            std::vector<double> ATb(N,   0.0);
            double sumSq = 0;
            int    nValid = 0;

            for (size_t ri = 0; ri < rows.size(); ++ri)
            {
                if (!active[ri]) continue;
                const auto& r = rows[ri];

                // Sagnac: rotate sat ECEF by -omega * tau (signal travel time).
                const double dx0 = r.satEcef.x - rx.x;
                const double dy0 = r.satEcef.y - rx.y;
                const double dz0 = r.satEcef.z - rx.z;
                const double rho0 = std::sqrt(dx0*dx0 + dy0*dy0 + dz0*dz0);
                const double tau = rho0 / C_M;
                const double th  = GnssMath::OMEGA_E * tau;
                const double cth = std::cos(th), sth = std::sin(th);
                const double sx =  cth*r.satEcef.x + sth*r.satEcef.y;
                const double sy = -sth*r.satEcef.x + cth*r.satEcef.y;
                const double sz =  r.satEcef.z;

                const double dx = sx - rx.x;
                const double dy = sy - rx.y;
                const double dz = sz - rx.z;
                const double rho = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (rho < 1e6) continue;

                double elev  = 90.0 * GnssMath::DEG2RAD;
                double w     = 1.0;
                double tropo = 0.0;
                double ionoM = 0.0;
                if (havePos)
                {
                    const auto enu = GnssMath::ecef2enu({sx,sy,sz}, rx);
                    const double he = std::sqrt(enu.e*enu.e + enu.n*enu.n);
                    elev = std::atan2(enu.u, he);
                    if (elev < 5.0 * GnssMath::DEG2RAD) continue;  // mask low-elev
                    tropo = tropoSaastamoinen(elev, std::max(rxLla.alt, 0.0));
                    if (iono.valid)
                    {
                        const double azim = std::atan2(enu.e, enu.n);
                        const double tow  = (r.gnss == G::BeiDou)
                                              ? rcvTow - 14.0 : rcvTow;
                        double fSig = F_GPS_L1;
                        switch (r.gnss)
                        {
                            case G::GPS:     fSig = F_GPS_L1; break;
                            case G::Galileo: fSig = F_GAL_E1; break;
                            case G::BeiDou:  fSig = F_BDS_B1I; break;
                            case G::GLONASS: fSig = F_GLO_L1; break;
                            default: break;
                        }
                        const double dB1I = klobucharDelay(iono, rxLla, azim, elev, tow);
                        const double scale = (F_BDS_B1I / fSig);
                        ionoM = dB1I * scale * scale;
                    }
                    const double sinE = std::sin(elev);
                    w = sinE * sinE * std::pow(10.0, (r.cno - 35.0) / 20.0);
                    if (w < 1e-3) w = 1e-3;
                }

                const int gIdx = gnssIdx(r.gnss);
                const int gCol = gnssCol[gIdx];

                const double rxClkG = clk[gCol];
                const double resid  = r.prMes - rho + r.satClkM - tropo - ionoM - rxClkG;
                lastResid[ri] = resid;

                const double jx = -dx / rho;
                const double jy = -dy / rho;
                const double jz = -dz / rho;

                std::array<double, 32> Hrow{};
                Hrow[0] = jx; Hrow[1] = jy; Hrow[2] = jz;
                Hrow[3 + gCol] = 1.0;

                for (int i = 0; i < N; ++i)
                {
                    ATb[i] += w * Hrow[i] * resid;
                    for (int j = 0; j < N; ++j)
                        ATA[i*N + j] += w * Hrow[i] * Hrow[j];
                }
                sumSq += resid * resid;
                ++nValid;
            }

            if (nValid < N)
            {
                failReason = "nValid<N"; failIter = iter; failNvalid = nValid;
                return false;
            }

            std::vector<double> A = ATA;
            std::vector<double> b = ATb;
            if (!solveLinear(A, b, N))
            {
                failReason = "singular ATA"; failIter = iter; failNvalid = nValid;
                return false;
            }

            for (int i = 0; i < N; ++i)
                if (!std::isfinite(b[i]))
                {
                    failReason = "NaN dx"; failIter = iter; failNvalid = nValid;
                    return false;
                }

            rx.x += b[0];
            rx.y += b[1];
            rx.z += b[2];
            for (int g = 0; g < nGnss; ++g) clk[g] += b[3 + g];

            const double dnorm = std::sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
            rms = std::sqrt(sumSq / nValid);
            if (!std::isfinite(rms) || !std::isfinite(rx.x)
                || !std::isfinite(rx.y) || !std::isfinite(rx.z))
            {
                failReason = "NaN state"; failIter = iter; failNvalid = nValid;
                return false;
            }
            if (dnorm < CONV_TOL) { ++iter; break; }
        }
        if (iter >= MAX_ITER)
        {
            failReason = "no convergence"; failIter = iter; failNvalid = -1;
            return false;
        }
        return true;
    };

    // Pass 1.
    if (!runWls())
    {
        if (kDbg)
        {
            std::fprintf(stderr,
                "[SPP] pass1 FAIL: %s @iter=%d nValid=%d rows=%zu N=%d seed=(%.1f,%.1f,%.1f)\n",
                failReason ? failReason : "?",
                failIter, failNvalid, rows.size(), N,
                initialEcef.x, initialEcef.y, initialEcef.z);
        }
        return out;
    }

    if (kDbg)
    {
        std::vector<double> ar;
        ar.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
            if (active[i]) ar.push_back(std::fabs(lastResid[i]));
        std::sort(ar.begin(), ar.end());
        auto pct = [&](double p) {
            if (ar.empty()) return 0.0;
            size_t k = static_cast<size_t>(p * (ar.size() - 1));
            return ar[k];
        };
        std::fprintf(stderr,
            "[SPP] pass1 OK iter=%d rms=%.2f n=%zu |r| min=%.2f p25=%.2f med=%.2f p75=%.2f max=%.2f\n",
            iter, rms, ar.size(), pct(0.0), pct(0.25), pct(0.5), pct(0.75), pct(1.0));
    }

    // Iteratively reject outliers: at each round, use min(K*MAD, HARD_MAX)
    // as the threshold and drop the single worst row exceeding it. Re-fit.
    // Stops when no row exceeds threshold or we run out of redundancy.
    constexpr double OUTLIER_HARD_MAX = 200.0;   // [m] absolute residual cap
    for (int round = 0; round < 8; ++round)
    {
        std::vector<double> abs_r;
        abs_r.reserve(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
            if (active[i]) abs_r.push_back(std::fabs(lastResid[i]));
        if (abs_r.empty()) break;
        std::sort(abs_r.begin(), abs_r.end());
        const double median = abs_r[abs_r.size() / 2];
        const double mad    = 1.4826 * median;
        const double thresh = std::min(OUTLIER_HARD_MAX,
                                       std::max(OUTLIER_HARD, OUTLIER_K * mad));

        size_t worstIdx = rows.size();
        double worstAbs = thresh;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (!active[i]) continue;
            const double a = std::fabs(lastResid[i]);
            if (a > worstAbs) { worstAbs = a; worstIdx = i; }
        }
        if (worstIdx == rows.size()) break;   // no more outliers

        int nActive = 0;
        for (bool a : active) if (a) ++nActive;
        if (nActive - 1 < N) break;           // would lose redundancy

        active[worstIdx] = false;
        ++nMasked;
        if (kDbg)
        {
            std::fprintf(stderr,
                "[SPP] reject round=%d gnss=%d sv=%u |r|=%.2f thresh=%.2f mad=%.2f nActive=%d->%d\n",
                round, static_cast<int>(rows[worstIdx].gnss), rows[worstIdx].svId,
                worstAbs, thresh, mad, nActive, nActive - 1);
        }

        // Re-fit from the same starting point.
        rx = (initialEcef.x*initialEcef.x + initialEcef.y*initialEcef.y
              + initialEcef.z*initialEcef.z > 1e6) ? initialEcef
                                                   : GnssMath::Ecef{0,0,0};
        std::fill(clk.begin(), clk.end(), 0.0);
        if (!runWls())
        {
            // Re-fit failed: undo the last mask and stop.
            active[worstIdx] = true;
            --nMasked;
            if (kDbg)
            {
                std::fprintf(stderr,
                    "[SPP] reject undo: refit failed (%s)\n",
                    failReason ? failReason : "?");
            }
            break;
        }
    }

    int nUsedFinal = 0;
    for (bool a : active) if (a) ++nUsedFinal;

    out.valid       = true;
    out.ecef        = rx;
    out.lla         = GnssMath::ecef2lla(rx);
    out.nUsed       = nUsedFinal;
    out.nCandidates = static_cast<int>(chosen.size());
    out.nDropped   += nMasked;
    out.iterations  = iter;
    out.rmsResidual = rms;
    if (gnssCol[0] >= 0) out.clkG = clk[gnssCol[0]];
    if (gnssCol[1] >= 0) out.clkE = clk[gnssCol[1]];
    if (gnssCol[2] >= 0) out.clkC = clk[gnssCol[2]];
    if (gnssCol[3] >= 0) out.clkR = clk[gnssCol[3]];
    return out;
}

}  // namespace Spp

#endif  // SPP_SOLVER_HPP_
