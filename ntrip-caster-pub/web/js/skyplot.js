/*
 * Polar sky-plot for the ntrip-caster mountpoint page.
 * Ported from examples/visualization/static/js/map.js drawSkyPlot().
 *
 * Input: an array of satellite objects:
 *   { gnss: "GPS"|"Galileo"|..., sv_id: 12, az_deg: 0..360 | null,
 *     el_deg: -90..90 | null, eph_age_s: number | null }
 * SVs without az/el (null) are drawn as faint outer-ring chips.
 */
(function (global) {
    'use strict';

    // GNSS palette — must match the visualization app and the CSS.
    const GNSS_COLORS = {
        GPS:      '#4fc3f7',
        Galileo:  '#81c784',
        GLONASS:  '#e57373',
        BeiDou:   '#ffb74d',
        SBAS:     '#ce93d8',
        QZSS:     '#fff176',
        NavIC:    '#90caf9',
        Unknown:  '#909090',
    };

    const PALETTE = {
        bg:           '#0a0a1a',
        rings:        '#2a2a3e',
        ringText:     '#555555',
        compass:      '#2a2a3e',
        compassLabel: '#888888',
        center:       '#ffffff',
        pendingRing:  '#3a3a55',
        pendingText:  '#7a7a95',
    };

    function colorOf(gnss) { return GNSS_COLORS[gnss] || GNSS_COLORS.Unknown; }

    function drawSkyPlot(canvasId, satellites) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;

        canvas.style.width  = '';
        canvas.style.height = '';

        const container = canvas.parentElement;
        const rect = container.getBoundingClientRect();
        const size = Math.max(180, Math.min(rect.width, rect.height));
        if (size < 10) return;

        const dpr = window.devicePixelRatio || 1;
        canvas.width  = size * dpr;
        canvas.height = size * dpr;
        canvas.style.width  = size + 'px';
        canvas.style.height = size + 'px';

        const ctx = canvas.getContext('2d');
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.scale(dpr, dpr);

        const cx   = size / 2;
        const cy   = size / 2;
        const maxR = size / 2 * 0.86;

        // Background
        ctx.fillStyle = PALETTE.bg;
        ctx.fillRect(0, 0, size, size);

        // Elevation rings 0/30/60/90
        ctx.strokeStyle = PALETTE.rings;
        ctx.lineWidth = 1;
        for (let el = 0; el <= 90; el += 30) {
            const r = maxR * (1 - el / 90);
            ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
            if (el > 0 && el < 90) {
                ctx.fillStyle = PALETTE.ringText;
                ctx.font = '11px monospace';
                ctx.textAlign = 'center';
                ctx.fillText(el + '\u00B0', cx, cy - r + 13);
            }
        }

        // Compass lines + labels
        const dirs = [
            { l: 'N', a: -90 }, { l: 'E', a: 0 },
            { l: 'S',  a: 90 }, { l: 'W', a: 180 },
        ];
        ctx.strokeStyle = PALETTE.compass;
        ctx.lineWidth = 1;
        for (const d of dirs) {
            const rad = d.a * Math.PI / 180;
            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(cx + Math.cos(rad) * maxR, cy + Math.sin(rad) * maxR);
            ctx.stroke();
            ctx.fillStyle = PALETTE.compassLabel;
            ctx.font = 'bold 14px sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            const lr = maxR + 14;
            ctx.fillText(d.l, cx + Math.cos(rad) * lr, cy + Math.sin(rad) * lr);
        }

        // Pending ring (ephemeris-not-yet-known SVs)
        const pending = [];
        const placed  = [];
        for (const s of satellites || []) {
            if (s.az_deg == null || s.el_deg == null || s.el_deg < 0) {
                pending.push(s);
            } else {
                placed.push(s);
            }
        }

        // Draw placed sats
        for (const s of placed) {
            const r = maxR * (1 - s.el_deg / 90);
            const aRad = (s.az_deg - 90) * Math.PI / 180;
            const sx = cx + Math.cos(aRad) * r;
            const sy = cy + Math.sin(aRad) * r;
            const c  = colorOf(s.gnss);

            ctx.beginPath();
            ctx.arc(sx, sy, 6, 0, Math.PI * 2);
            ctx.fillStyle = c;
            ctx.globalAlpha = 0.85;
            ctx.fill();
            ctx.globalAlpha = 1.0;
            ctx.lineWidth = 1.5;
            ctx.strokeStyle = '#0f0f1e';
            ctx.stroke();

            ctx.fillStyle = c;
            ctx.font = '10px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'bottom';
            ctx.fillText(String(s.sv_id), sx, sy - 8);
        }

        // Draw pending sats around an outer ring labelled with sv-id only
        if (pending.length) {
            const ringR = maxR + 28;
            ctx.strokeStyle = PALETTE.pendingRing;
            ctx.setLineDash([3, 3]);
            ctx.beginPath(); ctx.arc(cx, cy, ringR, 0, Math.PI * 2); ctx.stroke();
            ctx.setLineDash([]);

            const step = (Math.PI * 2) / pending.length;
            for (let i = 0; i < pending.length; ++i) {
                const ang = -Math.PI / 2 + i * step;
                const px = cx + Math.cos(ang) * ringR;
                const py = cy + Math.sin(ang) * ringR;
                const c  = colorOf(pending[i].gnss);
                ctx.beginPath();
                ctx.arc(px, py, 3.5, 0, Math.PI * 2);
                ctx.fillStyle = c;
                ctx.globalAlpha = 0.5;
                ctx.fill();
                ctx.globalAlpha = 1.0;

                ctx.fillStyle = PALETTE.pendingText;
                ctx.font = '9px monospace';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText(String(pending[i].sv_id),
                             cx + Math.cos(ang) * (ringR + 10),
                             cy + Math.sin(ang) * (ringR + 10));
            }
        }

        // Center dot
        ctx.beginPath();
        ctx.arc(cx, cy, 3, 0, Math.PI * 2);
        ctx.fillStyle = PALETTE.center;
        ctx.fill();
    }

    global.SkyPlot = { draw: drawSkyPlot, GNSS_COLORS: GNSS_COLORS };
})(window);
