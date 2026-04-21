// Altitude Tape Visualization — Jimmy Paputto 2026
// One-axis altimeter showing current altitude relative to a reference.
// Mirrors the Relative Map (GPSMap) pattern but on a single vertical axis.

class AltitudeTape {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');

        // State
        this.scale = 20;               // ± meters shown on the tape
        this.source = 'msl';           // 'msl' | 'wgs84'
        this.referenceAltitude = null; // locked on first valid sample
        this.originSet = false;
        this.currentAlt = 0;           // absolute (raw) altitude in meters
        this.currentAccuracy = 0;      // vertical accuracy in meters (0 if unknown)
        this.hasAccuracy = false;
        this.trail = [];               // array of delta-altitudes (relative to ref), capped
        this.maxTrail = 600;

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
        const slider = document.getElementById('altitude-scale-slider');
        if (slider) {
            slider.addEventListener('input', (e) => {
                this.scale = parseFloat(e.target.value);
                const valueEl = document.getElementById('altitude-scale-value');
                if (valueEl) valueEl.textContent = `${this.scale}m`;
                this.updateScaleDisplay();
            });
        }
        this.updateScaleDisplay();
    }

    getGridSpacing() {
        if (this.scale <= 1)       return 0.25;
        else if (this.scale <= 2)  return 0.5;
        else if (this.scale <= 5)  return 1;
        else if (this.scale <= 20) return 2;
        else if (this.scale <= 50) return 5;
        else                       return 10;
    }

    updateScaleDisplay() {
        const scaleInfo = document.getElementById('altitude-scale-info');
        if (scaleInfo) {
            const grid = this.getGridSpacing();
            const gridLabel = grid < 1 ? `${(grid * 100).toFixed(0)}cm` : `${grid}m`;
            scaleInfo.textContent = `Scale: ±${this.scale.toFixed(1)}m  |  Grid: ${gridLabel}`;
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
        this.trail.push(delta);
        if (this.trail.length > this.maxTrail) {
            this.trail.splice(0, this.trail.length - this.maxTrail);
        }

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
        this.trail = this.originSet ? [0] : [];
    }

    draw() {
        if (!this.width || !this.height) return;

        const ctx = this.ctx;
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(0, 0, this.width, this.height);

        // Centre line (X pos of the tape), plenty of room for labels on the left.
        const centerY = this.height / 2;
        const tapeX = Math.max(80, this.width * 0.35);
        const tapeWidth = 40;

        this.drawGrid(centerY, tapeX, tapeWidth);
        this.drawTape(centerY, tapeX, tapeWidth);
        this.drawZeroLine(centerY, tapeX, tapeWidth);
        this.drawTrail(centerY, tapeX, tapeWidth);

        if (this.originSet) {
            this.drawAccuracyBand(centerY, tapeX, tapeWidth);
            this.drawMarker(centerY, tapeX, tapeWidth);
        } else {
            this.drawWaitingMessage();
        }
    }

    drawGrid(centerY, tapeX, tapeWidth) {
        const ctx = this.ctx;
        const gridSpacing = this.getGridSpacing();

        ctx.strokeStyle = '#e0e0e0';
        ctx.lineWidth = 1;
        ctx.fillStyle = '#555555';
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
        ctx.fillStyle = '#000000';
        ctx.font = 'bold 13px sans-serif';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';
        const srcLabel = this.source === 'wgs84' ? 'Altitude (WGS84)' : 'Altitude (MSL)';
        ctx.fillText(srcLabel, 10, 10);
    }

    drawTape(centerY, tapeX, tapeWidth) {
        const ctx = this.ctx;
        // Tape bar
        ctx.fillStyle = '#f5f5f5';
        ctx.strokeStyle = '#000000';
        ctx.lineWidth = 1.5;
        ctx.fillRect(tapeX, 0, tapeWidth, this.height);
        ctx.strokeRect(tapeX, 0, tapeWidth, this.height);

        // Minor ticks on tape
        ctx.strokeStyle = '#888888';
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

    drawZeroLine(centerY, tapeX, tapeWidth) {
        const ctx = this.ctx;
        ctx.strokeStyle = '#000000';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(tapeX - 12, centerY);
        ctx.lineTo(this.width, centerY);
        ctx.stroke();

        ctx.fillStyle = '#000000';
        ctx.font = 'bold 12px sans-serif';
        ctx.textAlign = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText('0', tapeX - 14, centerY);
    }

    drawTrail(centerY, tapeX, tapeWidth) {
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

        ctx.strokeStyle = '#22c55e';
        ctx.lineWidth = 2;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        ctx.beginPath();

        const n = this.trail.length;
        for (let i = 0; i < n; i++) {
            const t = (n - 1 - i) / (n - 1 || 1); // 0 = newest, 1 = oldest
            const px = traceX1 - t * (traceX1 - traceX0);
            const delta = this.trail[i];
            const py = centerY - this.metersToPixels(delta);
            if (i === 0) ctx.moveTo(px, py);
            else ctx.lineTo(px, py);
        }
        ctx.stroke();
        ctx.restore();
    }

    drawAccuracyBand(centerY, tapeX, tapeWidth) {
        if (!this.hasAccuracy || this.currentAccuracy <= 0) return;
        const ctx = this.ctx;
        const delta = this.currentAlt - this.referenceAltitude;
        const centerPx = centerY - this.metersToPixels(delta);
        const bandPx = this.metersToPixels(this.currentAccuracy);
        const x0 = tapeX - 2;
        const x1 = this.width - 4;

        ctx.save();
        ctx.fillStyle = 'rgba(239, 68, 68, 0.12)';
        ctx.strokeStyle = 'rgba(239, 68, 68, 0.35)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        const top = Math.max(0, centerPx - bandPx);
        const bottom = Math.min(this.height, centerPx + bandPx);
        ctx.fillRect(x0, top, x1 - x0, bottom - top);
        ctx.strokeRect(x0, top, x1 - x0, bottom - top);
        ctx.restore();
    }

    drawMarker(centerY, tapeX, tapeWidth) {
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

        ctx.fillStyle = offScale ? '#f59e0b' : '#ef4444';
        ctx.beginPath();
        ctx.arc(px, y, 9, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.stroke();
        ctx.shadowColor = 'transparent';
        ctx.fillStyle = '#ffffff';
        ctx.beginPath();
        ctx.arc(px, y, 3, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();

        // Off-scale arrow if clamped
        if (offScale) {
            ctx.fillStyle = '#f59e0b';
            ctx.font = 'bold 18px sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            const arrow = (delta > 0) ? '▲' : '▼';
            ctx.fillText(arrow, px, y + (delta > 0 ? -18 : 18));
        }

        // Δ label next to marker
        ctx.fillStyle = '#000000';
        ctx.font = 'bold 13px monospace';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'middle';
        const label = `${delta >= 0 ? '+' : ''}${delta.toFixed(2)} m`;
        ctx.fillText(label, tapeX + tapeWidth + 6, y);
    }

    drawWaitingMessage() {
        const ctx = this.ctx;
        ctx.fillStyle = '#888888';
        ctx.font = 'italic 14px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('Waiting for valid altitude fix…', this.width / 2, this.height / 2);
    }

    startAnimation() {
        const animate = () => {
            this.draw();
            this.animationFrame = requestAnimationFrame(animate);
        };
        animate();
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
