// Altitude Tape Visualization — Jimmy Paputto 2026
// One-axis altimeter showing current altitude relative to a reference.
// Mirrors the Relative Map (GPSMap) pattern but on a single vertical axis.

class AltitudeTape {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');

        // State
        // Discrete, exponentially-spaced range presets (in metres). The slider,
        // wheel and ± buttons all step through these positions — predictable
        // snap points from RTK-grade centimetres up to a wide overview.
        this.scaleLadder = [0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 50, 100];
        this.scaleIndex = 8;                  // default = 20 m
        this.scale = this.scaleLadder[this.scaleIndex];
        this._wheelAccum = 0;
        this.source = 'msl';           // 'msl' | 'wgs84'
        this.referenceAltitude = null; // locked on first valid sample
        this.originSet = false;
        this.currentAlt = 0;           // absolute (raw) altitude in meters
        this.currentAccuracy = 0;      // vertical accuracy in meters (0 if unknown)
        this.hasAccuracy = false;
        // Trail is a fixed time window of recent samples: array of
        // {t: epoch-ms, d: delta-from-reference-m}. Newest sample sits
        // at the right edge of the trace strip; older samples scroll
        // left at a constant pixels-per-second rate and fall off when
        // they pass the left edge. Memory bounded by update-rate ×
        // window, not by uptime.
        this.trail = [];
        this.trailWindowMs = 60_000;   // 60 s of scrollback

        this.animationFrame = null;

        this.setupCanvas();
        this.setupScaleSlider();
        this.startAnimation();

        window.addEventListener('resize', () => this.setupCanvas());
    }

    setupCanvas() {
        const container = this.canvas.parentElement;
        const rect = container.getBoundingClientRect();
        if (rect.width < 1 || rect.height < 1) return;

        this.canvas.style.width = '100%';
        this.canvas.style.height = '100%';

        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        this.ctx.setTransform(1, 0, 0, 1, 0, 0);
        this.ctx.scale(dpr, dpr);

        this.width = rect.width;
        this.height = rect.height;
    }

    setupScaleSlider() {
        // Slider value is an integer index into `scaleLadder` so clicks on
        // the track snap cleanly to a preset.
        const slider = document.getElementById('altitude-scale-slider');
        if (slider) {
            slider.min = '0';
            slider.max = String(this.scaleLadder.length - 1);
            slider.step = '1';
            slider.value = String(this.scaleIndex);
            slider.addEventListener('input', (e) => {
                this.setScaleIndex(parseInt(e.target.value, 10));
            });
        }
        // Mouse wheel zoom: accumulate deltas so high-resolution touchpads
        // don't fly through the ladder.
        const wrapper = this.canvas.parentElement;
        if (wrapper) {
            wrapper.addEventListener('wheel', (e) => {
                e.preventDefault();
                this._wheelAccum += e.deltaY;
                const threshold = 40;
                while (this._wheelAccum >= threshold) {
                    this.setScaleIndex(this.scaleIndex + 1);
                    this._wheelAccum -= threshold;
                }
                while (this._wheelAccum <= -threshold) {
                    this.setScaleIndex(this.scaleIndex - 1);
                    this._wheelAccum += threshold;
                }
            }, { passive: false });
            // Pinch-to-zoom on touch devices (helper defined in map.js).
            if (typeof attachPinchZoom === 'function') {
                attachPinchZoom(wrapper, this);
            }
        }
        this.setScaleIndex(this.scaleIndex);
    }

    setScaleIndex(idx) {
        const max = this.scaleLadder.length - 1;
        const clamped = Math.max(0, Math.min(max, idx | 0));
        this.scaleIndex = clamped;
        this.scale = this.scaleLadder[clamped];
        const slider = document.getElementById('altitude-scale-slider');
        if (slider && parseInt(slider.value, 10) !== clamped) slider.value = String(clamped);
        const valueEl = document.getElementById('altitude-scale-value');
        if (valueEl) valueEl.textContent = `±${formatMeters(this.scale)}`;
        this.updateScaleDisplay();
    }

    setScale(value) {
        // Snap an arbitrary metre value to the nearest ladder rung in log-space.
        let bestIdx = 0;
        let bestDist = Infinity;
        const target = Math.log(Math.max(value, 1e-6));
        for (let i = 0; i < this.scaleLadder.length; i++) {
            const d = Math.abs(Math.log(this.scaleLadder[i]) - target);
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }
        this.setScaleIndex(bestIdx);
    }

    getGridSpacing() {
        // Extended ladder for RTK-grade centimetre ranges.
        if (this.scale <= 0.05)      return 0.01;  // 1 cm
        else if (this.scale <= 0.1)  return 0.02;  // 2 cm
        else if (this.scale <= 0.25) return 0.05;  // 5 cm
        else if (this.scale <= 0.5)  return 0.1;   // 10 cm
        else if (this.scale <= 1)    return 0.25;  // 25 cm
        else if (this.scale <= 2)    return 0.5;
        else if (this.scale <= 5)    return 1;
        else if (this.scale <= 20)   return 2;
        else if (this.scale <= 50)   return 5;
        else                         return 10;
    }

    updateScaleDisplay() {
        const scaleInfo = document.getElementById('altitude-scale-info');
        if (scaleInfo) {
            const grid = this.getGridSpacing();
            const gridLabel = grid < 1 ? `${(grid * 100).toFixed(grid < 0.1 ? 1 : 0)}cm` : `${grid}m`;
            // Grid only — the slider readout shows the range.
            scaleInfo.innerHTML =
                `<span class="si-key">Grid:</span>` +
                `<span class="si-val si-val-grid">${gridLabel}</span>`;
        }
    }

    metersToPixels(meters) {
        // Map ±scale meters onto 90% of canvas height
        const halfSize = this.height / 2;
        return (meters / this.scale) * (halfSize * 0.9);
    }

    setSource(source) {
        if (source !== 'msl' && source !== 'wgs84') return;
        if (this.source === source) return;

        // Preserve the reference across the source change using the current
        // geoid separation (WGS84 − MSL). The Δ altitude (and the trail) stays
        // put; only the "Absolute" readout and zero-line label change.
        const pvt = window.lastGPSData && window.lastGPSData.pvt;
        if (this.originSet && this.referenceAltitude !== null
            && pvt && pvt.altitude !== undefined && pvt.altitude_msl !== undefined) {
            const separation = pvt.altitude - pvt.altitude_msl; // WGS84 − MSL
            if (source === 'wgs84' && this.source === 'msl') {
                this.referenceAltitude = this.referenceAltitude + separation;
            } else if (source === 'msl' && this.source === 'wgs84') {
                this.referenceAltitude = this.referenceAltitude - separation;
            }
            this.source = source;
            // currentAlt will be refreshed on the next updateFromPvt(); leave
            // trail untouched so the history line stays visible.
        } else {
            // No reference yet (or WGS84 unavailable in this payload) — just
            // switch; the reference will lock on the next sample.
            this.source = source;
            this.referenceAltitude = null;
            this.originSet = false;
            this.trail = [];
        }
    }

    updateFromPvt(pvt) {
        if (!pvt) return null;

        let alt;
        if (this.source === 'wgs84') {
            if (pvt.altitude === undefined) return null;
            alt = pvt.altitude;
        } else {
            if (pvt.altitude_msl === undefined) return null;
            alt = pvt.altitude_msl;
        }

        // Guard against garbage fixes (pvt.latitude==0 & lon==0 typically means "no fix")
        const hasFix = (pvt.latitude !== 0.0 || pvt.longitude !== 0.0);
        if (!hasFix && !this.originSet) return null;

        this.currentAlt = alt;
        if (pvt.vertical_accuracy !== undefined && pvt.vertical_accuracy > 0) {
            this.currentAccuracy = pvt.vertical_accuracy;
            this.hasAccuracy = true;
        } else {
            this.currentAccuracy = 0;
            this.hasAccuracy = false;
        }

        if (this.referenceAltitude === null) {
            this.referenceAltitude = alt;
            this.originSet = true;
        }

        const delta = alt - this.referenceAltitude;
        const now = performance.now();
        this.trail.push({ t: now, d: delta });
        // Drop everything older than the visible window.
        const cutoff = now - this.trailWindowMs;
        let i = 0;
        while (i < this.trail.length && this.trail[i].t < cutoff) i++;
        if (i > 0) this.trail.splice(0, i);

        return { delta, absolute: alt, accuracy: this.currentAccuracy };
    }

    resetOrigin() {
        const data = window.lastGPSData;
        if (data && data.pvt) {
            const alt = (this.source === 'wgs84') ? data.pvt.altitude : data.pvt.altitude_msl;
            if (alt !== undefined) {
                this.referenceAltitude = alt;
                this.currentAlt = alt;
            } else {
                this.referenceAltitude = null;
            }
        } else {
            this.referenceAltitude = null;
        }
        this.originSet = this.referenceAltitude !== null;
        this.trail = this.originSet ? [{ t: performance.now(), d: 0 }] : [];
    }

    draw() {
        if (!this.width || !this.height) return;

        const ctx = this.ctx;
        const p = (typeof getChartPalette === 'function') ? getChartPalette('altitude') : null;
        const pal = p || {
            bg: '#ffffff', grid: '#e0e0e0', gridText: '#555555',
            axisLabel: '#000000', zero: '#000000',
            tape: '#f5f5f5', tapeBorder: '#000000', tapeTicks: '#888888',
            marker: '#ef4444', markerOutline: '#ffffff',
            offScale: '#f59e0b', trail: '#22c55e',
            accBand: 'rgba(239, 68, 68, 0.12)', accStroke: 'rgba(239, 68, 68, 0.35)',
            delta: '#000000', waiting: '#888888',
        };
        ctx.fillStyle = pal.bg;
        ctx.fillRect(0, 0, this.width, this.height);

        // Centre line (X pos of the tape), plenty of room for labels on the left.
        const centerY = this.height / 2;
        const tapeX = Math.max(80, this.width * 0.35);
        const tapeWidth = 40;

        this.drawGrid(centerY, tapeX, tapeWidth, pal);
        this.drawTape(centerY, tapeX, tapeWidth, pal);
        this.drawZeroLine(centerY, tapeX, tapeWidth, pal);
        this.drawTrail(centerY, tapeX, tapeWidth, pal);

        if (this.originSet) {
            this.drawAccuracyBand(centerY, tapeX, tapeWidth, pal);
            this.drawMarker(centerY, tapeX, tapeWidth, pal);
        } else {
            this.drawWaitingMessage(pal);
        }
    }

    drawGrid(centerY, tapeX, tapeWidth, pal) {
        const ctx = this.ctx;
        const gridSpacing = this.getGridSpacing();

        ctx.strokeStyle = pal.grid;
        ctx.lineWidth = 1;
        ctx.fillStyle = pal.gridText;
        ctx.font = '12px monospace';
        ctx.textAlign = 'right';
        ctx.textBaseline = 'middle';

        for (let i = 0; i <= this.scale + 1e-6; i += gridSpacing) {
            const offset = this.metersToPixels(i);

            // Positive (above origin)
            const yUp = centerY - offset;
            if (yUp >= 0 && yUp <= this.height) {
                ctx.beginPath();
                ctx.moveTo(tapeX - 6, yUp);
                ctx.lineTo(this.width, yUp);
                ctx.stroke();
                const label = (i >= 1) ? `+${i.toFixed(0)} m` : `+${(i * 100).toFixed(0)} cm`;
                ctx.fillText(label, tapeX - 10, yUp);
            }

            if (i > 0) {
                const yDown = centerY + offset;
                if (yDown >= 0 && yDown <= this.height) {
                    ctx.beginPath();
                    ctx.moveTo(tapeX - 6, yDown);
                    ctx.lineTo(this.width, yDown);
                    ctx.stroke();
                    const label = (i >= 1) ? `−${i.toFixed(0)} m` : `−${(i * 100).toFixed(0)} cm`;
                    ctx.fillText(label, tapeX - 10, yDown);
                }
            }
        }

        // Axis label at the top
        ctx.fillStyle = pal.axisLabel;
        ctx.font = 'bold 13px sans-serif';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';
        const srcLabel = this.source === 'wgs84' ? 'Altitude (WGS84)' : 'Altitude (MSL)';
        ctx.fillText(srcLabel, 10, 10);
    }

    drawTape(centerY, tapeX, tapeWidth, pal) {
        const ctx = this.ctx;
        // Tape bar
        ctx.fillStyle = pal.tape;
        ctx.strokeStyle = pal.tapeBorder;
        ctx.lineWidth = 1.5;
        ctx.fillRect(tapeX, 0, tapeWidth, this.height);
        ctx.strokeRect(tapeX, 0, tapeWidth, this.height);

        // Minor ticks on tape
        ctx.strokeStyle = pal.tapeTicks;
        ctx.lineWidth = 1;
        const gridSpacing = this.getGridSpacing();
        const minorSpacing = gridSpacing / 5;
        for (let i = 0; i <= this.scale + 1e-6; i += minorSpacing) {
            const offset = this.metersToPixels(i);
            for (const sign of [-1, 1]) {
                if (i === 0 && sign < 0) continue;
                const y = centerY - sign * offset;
                if (y < 0 || y > this.height) continue;
                ctx.beginPath();
                ctx.moveTo(tapeX, y);
                ctx.lineTo(tapeX + 5, y);
                ctx.stroke();
            }
        }
    }

    drawZeroLine(centerY, tapeX, tapeWidth, pal) {
        const ctx = this.ctx;
        ctx.strokeStyle = pal.zero;
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(tapeX - 12, centerY);
        ctx.lineTo(this.width, centerY);
        ctx.stroke();

        ctx.fillStyle = pal.zero;
        ctx.font = 'bold 12px sans-serif';
        ctx.textAlign = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText('0', tapeX - 14, centerY);
    }

    drawTrail(centerY, tapeX, tapeWidth, pal) {
        if (this.trail.length < 2) return;
        const ctx = this.ctx;

        // Horizontal time strip to the right of the tape showing recent trace.
        const traceX0 = tapeX + tapeWidth + 12;
        const traceX1 = this.width - 8;
        if (traceX1 <= traceX0 + 10) return;

        ctx.save();
        ctx.beginPath();
        ctx.rect(traceX0, 0, traceX1 - traceX0, this.height);
        ctx.clip();

        ctx.strokeStyle = pal.trail;
        ctx.lineWidth = 2;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        ctx.beginPath();

        const now = performance.now();
        const win = this.trailWindowMs;
        const n = this.trail.length;
        let started = false;
        for (let i = 0; i < n; i++) {
            const sample = this.trail[i];
            const age = now - sample.t;
            if (age > win) continue;             // safety: skip stale
            const t = age / win;                 // 0 = now, 1 = window edge
            const px = traceX1 - t * (traceX1 - traceX0);
            const py = centerY - this.metersToPixels(sample.d);
            if (!started) { ctx.moveTo(px, py); started = true; }
            else ctx.lineTo(px, py);
        }
        ctx.stroke();
        ctx.restore();
    }

    drawAccuracyBand(centerY, tapeX, tapeWidth, pal) {
        if (!this.hasAccuracy || this.currentAccuracy <= 0) return;
        const ctx = this.ctx;
        const delta = this.currentAlt - this.referenceAltitude;
        const centerPx = centerY - this.metersToPixels(delta);
        const bandPx = this.metersToPixels(this.currentAccuracy);
        const x0 = tapeX - 2;
        const x1 = this.width - 4;

        ctx.save();
        ctx.fillStyle = pal.accBand;
        ctx.strokeStyle = pal.accStroke;
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        const top = Math.max(0, centerPx - bandPx);
        const bottom = Math.min(this.height, centerPx + bandPx);
        ctx.fillRect(x0, top, x1 - x0, bottom - top);
        ctx.strokeRect(x0, top, x1 - x0, bottom - top);
        ctx.restore();
    }

    drawMarker(centerY, tapeX, tapeWidth, pal) {
        const ctx = this.ctx;
        const delta = this.currentAlt - this.referenceAltitude;
        const deltaClamped = Math.max(-this.scale, Math.min(this.scale, delta));
        const y = centerY - this.metersToPixels(deltaClamped);
        const offScale = (delta !== deltaClamped);

        // Pointer triangle pointing left, positioned over the tape
        const px = tapeX + tapeWidth / 2;
        ctx.save();
        ctx.shadowColor = 'rgba(0, 0, 0, 0.3)';
        ctx.shadowBlur = 4;
        ctx.shadowOffsetY = 2;

        ctx.fillStyle = offScale ? pal.offScale : pal.marker;
        ctx.beginPath();
        ctx.arc(px, y, 9, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = pal.markerOutline;
        ctx.lineWidth = 2;
        ctx.stroke();
        ctx.shadowColor = 'transparent';
        ctx.fillStyle = pal.markerOutline;
        ctx.beginPath();
        ctx.arc(px, y, 3, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();

        // Off-scale arrow if clamped
        if (offScale) {
            ctx.fillStyle = pal.offScale;
            ctx.font = 'bold 18px sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            const arrow = (delta > 0) ? '▲' : '▼';
            ctx.fillText(arrow, px, y + (delta > 0 ? -18 : 18));
        }

        // Δ label next to marker
        ctx.fillStyle = pal.delta;
        ctx.font = 'bold 13px monospace';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'middle';
        const label = `${delta >= 0 ? '+' : ''}${delta.toFixed(2)} m`;
        ctx.fillText(label, tapeX + tapeWidth + 6, y);
    }

    drawWaitingMessage(pal) {
        const ctx = this.ctx;
        ctx.fillStyle = pal.waiting;
        ctx.font = 'italic 14px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('Waiting for valid altitude fix…', this.width / 2, this.height / 2);
    }

    startAnimation() {
        if (this.animationFrame) return;     // already running
        // The trail scrolls smoothly with time, so we have to redraw
        // continuously — but 60 fps is overkill for a slow-scrolling
        // line. Throttle to ~15 fps; that's still imperceptibly smooth
        // for a strip moving at a few px/s and cuts GPU load ~4x.
        const targetFps = 15;
        const minFrameMs = 1000 / targetFps;
        let lastDraw = 0;
        const animate = (ts) => {
            if (ts - lastDraw >= minFrameMs) {
                this.draw();
                lastDraw = ts;
            }
            this.animationFrame = requestAnimationFrame(animate);
        };
        this.animationFrame = requestAnimationFrame(animate);
    }

    stopAnimation() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
            this.animationFrame = null;
        }
    }
}

// Global instance + wiring (consumed by map.js)
window.AltitudeTape = AltitudeTape;
window.altitudeTape = null;

document.addEventListener('DOMContentLoaded', function () {
    const canvas = document.getElementById('altitude-tape');
    if (!canvas) return;
    window.altitudeTape = new AltitudeTape('altitude-tape');

    // Source toggle (MSL / WGS84)
    const toggle = document.querySelectorAll('.alt-source-btn');
    toggle.forEach(btn => {
        btn.addEventListener('click', () => {
            const src = btn.dataset.altSource;
            toggle.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            window.altitudeTape.setSource(src);
            // Refresh footer immediately from the latest sample so the
            // "Absolute" readout reflects the new source without waiting
            // for the next gps_update.
            if (typeof window.updateAltitudeTape === 'function') {
                window.updateAltitudeTape(window.lastGPSData);
            }
        });
    });

    // Reset button
    const resetBtn = document.getElementById('alt-reset-btn');
    if (resetBtn) {
        resetBtn.addEventListener('click', () => {
            window.altitudeTape.resetOrigin();
            resetAltitudeFooter();
        });
    }
});

function resetAltitudeFooter() {
    const d = document.getElementById('alt-delta');
    const a = document.getElementById('alt-absolute');
    const v = document.getElementById('alt-vacc');
    if (d) d.textContent = '0.00 m';
    if (a) a.textContent = '0.00 m';
    if (v) v.textContent = '—';
}

// Called from map.js updateGPSData hook
window.updateAltitudeTape = function (data) {
    if (!window.altitudeTape || !data || !data.pvt) return;
    const result = window.altitudeTape.updateFromPvt(data.pvt);
    if (!result) return;
    const d = document.getElementById('alt-delta');
    const a = document.getElementById('alt-absolute');
    const v = document.getElementById('alt-vacc');
    if (d) d.textContent = `${result.delta >= 0 ? '+' : ''}${result.delta.toFixed(2)} m`;
    if (a) a.textContent = `${result.absolute.toFixed(2)} m`;
    if (v) {
        if (window.altitudeTape.hasAccuracy) {
            v.textContent = `± ${result.accuracy.toFixed(2)} m`;
        } else {
            v.textContent = '—';
        }
    }
};
