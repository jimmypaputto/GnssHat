// Elevation Mask sky-dome preview.
//
// Renders a half-dome ("cross-section" at azimuth = current view) with:
//   • radial elevation ticks at 0/15/30/45/60/75/90 deg
//   • a hatched earth strip below the horizon
//   • a translucent red sector covering 0..minElev (the filtered band)
//   • a dashed mask line and angle label
//   • live satellites projected onto the dome using both elevation and
//     azimuth: x = cos(el) · sin(az), y = sin(el) (north-south slice).
//     Filtered satellites (el < minElev) are drawn greyed out.
//
// Reads the current theme palette via getChartPalette('navfilter') on every
// draw so the "per-chart" light/dark switch applies instantly.

(function () {
    'use strict';

    const DEFAULT_SYNTHETIC_SATS = [
        { elevation: 4,  azimuth:  50, gnss_id: 'GPS'     },
        { elevation: 12, azimuth: 120, gnss_id: 'Galileo' },
        { elevation: 35, azimuth: 200, gnss_id: 'GLONASS' },
        { elevation: 70, azimuth: 300, gnss_id: 'BeiDou'  },
    ];

    class ElevationMaskPreview {
        constructor(canvasId) {
            this.canvas = document.getElementById(canvasId);
            if (!this.canvas) return;
            this.ctx = this.canvas.getContext('2d');
            this.mask = 5;                    // degrees
            this.satellites = null;           // null → use synthetic demo set
            this.width = 0;
            this.height = 0;
            this.setupCanvas();
            window.addEventListener('resize', () => {
                this.setupCanvas();
                this.draw();
            });
        }

        setupCanvas() {
            if (!this.canvas) return;
            const rect = this.canvas.getBoundingClientRect();
            if (rect.width < 1 || rect.height < 1) return;
            const dpr = window.devicePixelRatio || 1;
            this.canvas.style.width  = '100%';
            this.canvas.style.height = '100%';
            this.canvas.width  = rect.width * dpr;
            this.canvas.height = rect.height * dpr;
            this.ctx.setTransform(1, 0, 0, 1, 0, 0);
            this.ctx.scale(dpr, dpr);
            this.width = rect.width;
            this.height = rect.height;
        }

        setMask(deg) {
            const v = Number(deg);
            if (!Number.isFinite(v)) return;
            this.mask = Math.max(-90, Math.min(90, Math.round(v)));
            this.draw();
        }

        // Accepts an array of { elevation, azimuth, gnss_id, used? } or null.
        setSatellites(sats) {
            this.satellites = Array.isArray(sats) ? sats : null;
            this.draw();
        }

        draw() {
            if (!this.ctx) return;
            if (this.width < 1 || this.height < 1) {
                this.setupCanvas();
                if (this.width < 1 || this.height < 1) return;
            }

            const pal = (typeof getChartPalette === 'function')
                ? getChartPalette('navfilter') : null;
            const p = pal || {
                bg: '#0a0a1a', sky: '#10152b', earth: '#1b1f2d',
                earthHatch: '#2a2f42', horizon: '#b8c0ff', dome: '#3c4264',
                ring: '#252844', ringText: '#6a7199', maskLine: '#f87171',
                maskFill: 'rgba(248, 113, 113, 0.2)', maskLabel: '#fecaca',
                satOutline: '#0f1220', satFiltered: '#4b5269', label: '#e6e8ff',
            };

            const ctx = this.ctx;
            const W = this.width, H = this.height;

            ctx.fillStyle = p.bg;
            ctx.fillRect(0, 0, W, H);

            // Dome geometry: horizon is a baseline near the bottom, dome
            // fills the upper area with a bit of margin on the sides.
            const margin = 30;
            const baseY = H - 32;                       // horizon line y-coord
            const maxR  = Math.min(
                (W - 2 * margin) / 2,
                baseY - 20
            );
            if (maxR < 10) return;
            const cx = W / 2;

            // Sky gradient
            const grd = ctx.createLinearGradient(0, baseY - maxR, 0, baseY);
            grd.addColorStop(0, p.sky);
            grd.addColorStop(1, p.bg);
            ctx.fillStyle = grd;
            ctx.beginPath();
            ctx.arc(cx, baseY, maxR, Math.PI, 2 * Math.PI);
            ctx.closePath();
            ctx.fill();

            // Elevation rings + radial ticks (at 0, 15, ..., 75 deg)
            ctx.strokeStyle = p.ring;
            ctx.lineWidth = 1;
            ctx.fillStyle = p.ringText;
            ctx.font = '10px system-ui, sans-serif';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'middle';
            for (let deg = 15; deg < 90; deg += 15) {
                const r = Math.cos(deg * Math.PI / 180) * maxR;
                ctx.beginPath();
                ctx.arc(cx, baseY, r, Math.PI, 2 * Math.PI);
                ctx.stroke();
                ctx.fillText(deg + '°', cx + r + 2, baseY - 6);
            }
            // 90° label (centre / top of dome)
            ctx.textAlign = 'center';
            ctx.fillText('90°', cx, baseY - maxR - 8);

            // Filtered band (translucent red sector 0..mask deg)
            const mask = this.mask;
            if (mask > 0) {
                // The band is the region with elevation in [0, mask].
                // In our projection (x = sin(az) · cos(el), y = sin(el) · r),
                // the outer edge is the horizon arc (el=0) and the inner
                // edge is the ring at el = mask.
                const rInner = Math.cos(mask * Math.PI / 180) * maxR;
                ctx.fillStyle = p.maskFill;
                ctx.beginPath();
                ctx.arc(cx, baseY, maxR,  Math.PI, 2 * Math.PI, false);
                ctx.arc(cx, baseY, rInner, 2 * Math.PI, Math.PI, true);
                ctx.closePath();
                ctx.fill();

                // Mask ring (dashed)
                ctx.save();
                ctx.setLineDash([6, 4]);
                ctx.strokeStyle = p.maskLine;
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                ctx.arc(cx, baseY, rInner, Math.PI, 2 * Math.PI);
                ctx.stroke();
                ctx.restore();
            }

            // Satellites — projected to the half-dome cross-section.
            const sats = (this.satellites && this.satellites.length > 0)
                ? this.satellites : DEFAULT_SYNTHETIC_SATS;
            const isSynthetic = !this.satellites || this.satellites.length === 0;
            for (const s of sats) {
                if (s == null) continue;
                const el = Number(s.elevation);
                const az = Number(s.azimuth);
                if (!Number.isFinite(el) || !Number.isFinite(az)) continue;
                // Keep only the N/S visible half for readability (sin(az) sign
                // determines left/right of the cross-section).
                const r = Math.cos(el * Math.PI / 180) * maxR;
                const x = cx + Math.sin(az * Math.PI / 180) * r;
                const y = baseY - Math.sin(el * Math.PI / 180) * maxR;

                const filtered = el < mask;
                const colour = filtered
                    ? p.satFiltered
                    : ((typeof getGnssColor === 'function')
                        ? getGnssColor(s.gnss_id) : '#8ab4ff');

                ctx.beginPath();
                ctx.arc(x, y, 5, 0, 2 * Math.PI);
                ctx.fillStyle = colour;
                ctx.fill();
                ctx.lineWidth = 1.5;
                ctx.strokeStyle = p.satOutline;
                ctx.stroke();

                if (filtered) {
                    // Slash across the sat
                    ctx.save();
                    ctx.strokeStyle = p.maskLine;
                    ctx.lineWidth = 1.5;
                    ctx.beginPath();
                    ctx.moveTo(x - 4, y + 4);
                    ctx.lineTo(x + 4, y - 4);
                    ctx.stroke();
                    ctx.restore();
                }
            }

            // Horizon (solid line)
            ctx.strokeStyle = p.horizon;
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.moveTo(cx - maxR - 10, baseY);
            ctx.lineTo(cx + maxR + 10, baseY);
            ctx.stroke();

            // Earth strip below horizon (hatched)
            ctx.fillStyle = p.earth;
            ctx.fillRect(0, baseY, W, H - baseY);
            ctx.save();
            ctx.strokeStyle = p.earthHatch;
            ctx.lineWidth = 1;
            for (let x = -H; x < W; x += 8) {
                ctx.beginPath();
                ctx.moveTo(x, baseY);
                ctx.lineTo(x + (H - baseY), H);
                ctx.stroke();
            }
            ctx.restore();

            // Mask angle label near the right side
            if (mask !== 0) {
                const labelY = baseY - Math.sin(mask * Math.PI / 180) * maxR;
                const labelX = cx + Math.cos(mask * Math.PI / 180) * maxR + 4;
                ctx.fillStyle = p.maskLabel;
                ctx.font = 'bold 12px system-ui, sans-serif';
                ctx.textAlign = 'left';
                ctx.textBaseline = 'middle';
                ctx.fillText(`Mask: ${mask}°`, labelX, labelY);
            }

            // Legend / hint in top-left
            ctx.fillStyle = p.label;
            ctx.font = '11px system-ui, sans-serif';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';
            ctx.fillText(
                isSynthetic ? 'Example satellites (no live data)'
                            : `${sats.length} satellite${sats.length === 1 ? '' : 's'}`,
                8, 6);
        }
    }

    window.ElevationMaskPreview = ElevationMaskPreview;
})();
