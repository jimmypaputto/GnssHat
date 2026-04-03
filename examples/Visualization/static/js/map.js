// GPS Visualization Map - Jimmy Paputto 2025

class GPSMap {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        
        // Map state
        this.scale = 20; // Default ±20 meters, can be changed by slider
        this.position = { x: 0, y: 0 }; // Current position in meters
        this.trail = []; // Complete position history (never clears except on reset)
        this.originSet = false; // Whether reference position has been set
        this.geofences = []; // Array of {offsetX, offsetY, radius} in meters
        
        // Animation
        this.animationFrame = null;
        
        // Setup
        this.setupCanvas();
        this.setupScaleSlider();
        this.startAnimation();
        
        // Handle window resize
        window.addEventListener('resize', () => this.setupCanvas());
    }
    
    setupScaleSlider() {
        // Setup slider event listener
        const slider = document.getElementById('scale-slider');
        if (slider) {
            slider.addEventListener('input', (e) => {
                this.scale = parseFloat(e.target.value);
                document.getElementById('scale-value').textContent = `${this.scale}m`;
                this.updateScaleDisplay();
            });
        }
    }
    
    setupCanvas() {
        // Set canvas size to match container (responsive)
        const container = this.canvas.parentElement;
        const rect = container.getBoundingClientRect();
        
        // Skip if container is hidden (tab not active) — dimensions would be 0
        if (rect.width < 1 || rect.height < 1) return;
        
        // Set display size (CSS pixels)
        this.canvas.style.width = '100%';
        this.canvas.style.height = '100%';
        
        // Set actual size in memory (scaled to device pixel ratio)
        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        
        // Scale context to match device pixel ratio
        this.ctx.scale(dpr, dpr);
        
        // Store logical dimensions
        this.width = rect.width;
        this.height = rect.height;
    }
    
    updatePosition(x, y) {
        // Initialize trail on first valid position
        if (!this.originSet) {
            this.originSet = true;
            this.trail = [{ x, y }];
            this.position = { x, y };
            this.updateScaleDisplay();
            // Resolve pending geofences now that we have a reference
            if (this._pendingGeofences) {
                const data = window.lastGPSData;
                if (data && data.pvt && data.has_reference) {
                    this._computeGeofenceOffsets(this._pendingGeofences, data);
                    this._pendingGeofences = null;
                }
            }
            return;
        }
        
        this.position = { x, y };
        
        // Add to trail - NEVER clear it (persistent trail)
        this.trail.push({ x, y });

        // Resolve pending geofences if not yet done
        if (this._pendingGeofences) {
            const data = window.lastGPSData;
            if (data && data.pvt && data.has_reference) {
                this._computeGeofenceOffsets(this._pendingGeofences, data);
                this._pendingGeofences = null;
            }
        }
    }
    
    getGridSpacing() {
        if (this.scale <= 1)       return 0.25;
        else if (this.scale <= 2)  return 0.5;
        else if (this.scale <= 5)  return 1;
        else                       return 5;
    }
    
    updateScaleDisplay() {
        const scaleInfo = document.getElementById('scale-info');
        if (scaleInfo) {
            const grid = this.getGridSpacing();
            const gridLabel = grid < 1 ? `${(grid * 100).toFixed(0)}cm` : `${grid}m`;
            scaleInfo.textContent = `Scale: ±${this.scale.toFixed(1)}m  |  Grid: ${gridLabel}`;
        }
    }
    
    metersToPixels(meters) {
        // Convert meters to pixels based on fixed scale (±20m)
        const halfSize = Math.min(this.width, this.height) / 2;
        return (meters / this.scale) * (halfSize * 0.9); // Use 90% of available space
    }
    
    isPointInCanvas(px, py) {
        // Check if pixel coordinate is within canvas bounds
        return px >= 0 && px <= this.width && py >= 0 && py <= this.height;
    }
    
    draw() {
        // Clear canvas with WHITE background
        this.ctx.fillStyle = '#ffffff';
        this.ctx.fillRect(0, 0, this.width, this.height);
        
        // Calculate center
        const centerX = this.width / 2;
        const centerY = this.height / 2;
        
        // Draw grid
        this.drawGrid(centerX, centerY);
        
        // Draw axes
        this.drawAxes(centerX, centerY);
        
        // Draw geofences (behind trail and position)
        this.drawGeofences(centerX, centerY);
        
        // Draw trail (GREEN, persistent)
        this.drawTrail(centerX, centerY);
        
        // Draw current position (RED dot)
        this.drawPosition(centerX, centerY);
    }
    
    drawGrid(centerX, centerY) {
        // Light gray grid lines
        this.ctx.strokeStyle = '#e0e0e0';
        this.ctx.lineWidth = 1;
        
        // Dynamic grid spacing based on current scale
        const gridSpacing = this.getGridSpacing();
        
        // Draw grid lines symmetrically from center (0,0 always has a crossing)
        for (let i = 0; i <= this.scale; i += gridSpacing) {
            const offset = this.metersToPixels(i);
            
            // Positive side: vertical line at +i, horizontal line at +i
            this.ctx.beginPath();
            this.ctx.moveTo(centerX + offset, 0);
            this.ctx.lineTo(centerX + offset, this.height);
            this.ctx.stroke();
            
            this.ctx.beginPath();
            this.ctx.moveTo(0, centerY - offset);
            this.ctx.lineTo(this.width, centerY - offset);
            this.ctx.stroke();
            
            // Negative side (skip 0 to avoid double-drawing)
            if (i > 0) {
                this.ctx.beginPath();
                this.ctx.moveTo(centerX - offset, 0);
                this.ctx.lineTo(centerX - offset, this.height);
                this.ctx.stroke();
                
                this.ctx.beginPath();
                this.ctx.moveTo(0, centerY + offset);
                this.ctx.lineTo(this.width, centerY + offset);
                this.ctx.stroke();
            }
        }
    }
    
    drawAxes(centerX, centerY) {
        // X-axis (East-West) - BLACK, solid
        this.ctx.strokeStyle = '#000000';
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();
        this.ctx.moveTo(0, centerY);
        this.ctx.lineTo(this.width, centerY);
        this.ctx.stroke();
        
        // Y-axis (North-South) - BLACK, solid
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();
        this.ctx.moveTo(centerX, 0);
        this.ctx.lineTo(centerX, this.height);
        this.ctx.stroke();
        
        // Axis labels - BLACK text
        this.ctx.fillStyle = '#000000';
        this.ctx.font = 'bold 14px sans-serif';
        this.ctx.textAlign = 'center';
        
        // North
        this.ctx.fillText('N', centerX - 10, 20);
        // South
        this.ctx.fillText('S', centerX - 10, this.height - 10);
        // East
        this.ctx.textAlign = 'left';
        this.ctx.fillText('E', this.width - 20, centerY - 8);
        // West
        this.ctx.textAlign = 'right';
        this.ctx.fillText('W', 20, centerY - 8);
        
        // Origin (0,0) marker - small circle at center
        this.ctx.fillStyle = '#000000';
        this.ctx.beginPath();
        this.ctx.arc(centerX, centerY, 3, 0, Math.PI * 2);
        this.ctx.fill();
    }
    
    drawTrail(centerX, centerY) {
        if (this.trail.length < 1) return;
        
        // GREEN line for trail
        this.ctx.strokeStyle = '#22c55e';
        this.ctx.lineWidth = 2;
        this.ctx.lineCap = 'round';
        this.ctx.lineJoin = 'round';
        
        // Enable clipping to avoid drawing outside canvas
        this.ctx.save();
        this.ctx.beginPath();
        this.ctx.rect(0, 0, this.width, this.height);
        this.ctx.clip();
        
        // Draw continuous trail
        this.ctx.beginPath();
        for (let i = 0; i < this.trail.length; i++) {
            const point = this.trail[i];
            const px = centerX + this.metersToPixels(point.x);
            const py = centerY - this.metersToPixels(point.y); // Invert Y for screen coords
            
            // Skip points outside canvas (but continue line)
            if (i === 0) {
                this.ctx.moveTo(px, py);
            } else {
                this.ctx.lineTo(px, py);
            }
        }
        this.ctx.stroke();
        
        this.ctx.restore();
        
        // Draw semi-transparent green dots for trail history (every 5th point)
        this.ctx.fillStyle = 'rgba(34, 197, 94, 0.4)';
        for (let i = 0; i < this.trail.length; i += Math.max(1, Math.floor(this.trail.length / 20))) {
            const point = this.trail[i];
            const px = centerX + this.metersToPixels(point.x);
            const py = centerY - this.metersToPixels(point.y);
            
            if (this.isPointInCanvas(px, py)) {
                this.ctx.beginPath();
                this.ctx.arc(px, py, 2, 0, Math.PI * 2);
                this.ctx.fill();
            }
        }
    }
    
    drawPosition(centerX, centerY) {
        const px = centerX + this.metersToPixels(this.position.x);
        const py = centerY - this.metersToPixels(this.position.y); // Invert Y
        
        // Only draw if within canvas bounds
        if (!this.isPointInCanvas(px, py)) {
            return;
        }
        
        // RED dot with shadow
        this.ctx.shadowColor = 'rgba(0, 0, 0, 0.3)';
        this.ctx.shadowBlur = 4;
        this.ctx.shadowOffsetX = 0;
        this.ctx.shadowOffsetY = 2;
        
        // Main red circle - current position
        this.ctx.fillStyle = '#ef4444';
        this.ctx.beginPath();
        this.ctx.arc(px, py, 8, 0, Math.PI * 2);
        this.ctx.fill();
        
        // White outline
        this.ctx.strokeStyle = '#ffffff';
        this.ctx.lineWidth = 2;
        this.ctx.stroke();
        
        // White center dot
        this.ctx.fillStyle = '#ffffff';
        this.ctx.beginPath();
        this.ctx.arc(px, py, 3, 0, Math.PI * 2);
        this.ctx.fill();
        
        // Reset shadow
        this.ctx.shadowColor = 'transparent';
    }
    
    drawGeofences(centerX, centerY) {
        if (this.geofences.length === 0) return;

        for (let i = 0; i < this.geofences.length; i++) {
            const gf = this.geofences[i];
            const px = centerX + this.metersToPixels(gf.offsetX);
            const py = centerY - this.metersToPixels(gf.offsetY);
            const rPx = this.metersToPixels(gf.radius);

            // Fill
            this.ctx.fillStyle = 'rgba(0, 180, 255, 0.08)';
            this.ctx.beginPath();
            this.ctx.arc(px, py, rPx, 0, Math.PI * 2);
            this.ctx.fill();

            // Stroke
            this.ctx.strokeStyle = 'rgba(0, 180, 255, 0.6)';
            this.ctx.lineWidth = 2;
            this.ctx.setLineDash([6, 4]);
            this.ctx.beginPath();
            this.ctx.arc(px, py, rPx, 0, Math.PI * 2);
            this.ctx.stroke();
            this.ctx.setLineDash([]);

            // Label
            this.ctx.fillStyle = 'rgba(0, 140, 220, 0.85)';
            this.ctx.font = 'bold 12px sans-serif';
            this.ctx.textAlign = 'center';
            this.ctx.textBaseline = 'bottom';
            this.ctx.fillText(`GF${i + 1} (${gf.radius}m)`, px, py - rPx - 4);
        }
    }

    setGeofences(geofences) {
        // geofences: array of {lat, lon, radius}
        this._rawGeofences = geofences;
        // Convert to relative offsets from reference position
        const data = window.lastGPSData;
        if (!data || !data.pvt || !data.has_reference) {
            // No reference yet — store raw and convert on next draw
            this._pendingGeofences = geofences;
            this.geofences = [];
            return;
        }
        this._pendingGeofences = null;
        this._computeGeofenceOffsets(geofences, data);
    }

    _computeGeofenceOffsets(geofences, data) {
        const curLat = data.pvt.latitude;
        const curLon = data.pvt.longitude;
        const curOffX = data.offset_x;
        const curOffY = data.offset_y;
        const DEG_TO_M = 111320;
        const cosLat = Math.cos(curLat * Math.PI / 180);

        this.geofences = geofences.map(gf => ({
            offsetX: curOffX + (gf.lon - curLon) * DEG_TO_M * cosLat,
            offsetY: curOffY + (gf.lat - curLat) * DEG_TO_M,
            radius: gf.radius,
        }));
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
    
    resetOrigin() {
        // Immediately clear trail and jump to 0,0
        this.position = { x: 0, y: 0 };
        this.trail = [{ x: 0, y: 0 }];
        this.originSet = true;
        // Re-queue geofences for recalculation with new reference
        if (this._rawGeofences && this._rawGeofences.length > 0) {
            this._pendingGeofences = this._rawGeofences;
            this.geofences = [];
        }
    }
}

// Application state
let map = null;
let socket = null;

// Initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    console.log('GPS Visualization starting...');
    
    // Initialize map
    map = new GPSMap('gps-map');
    
    // Initialize Socket.IO connection
    initializeSocket();
    
    // Setup UI event handlers
    setupUIHandlers();
});

function initializeSocket() {
    socket = io();
    
    socket.on('connect', function() {
        console.log('Connected to server');
        updateConnectionStatus(true);
    });
    
    socket.on('disconnect', function() {
        console.log('Disconnected from server');
        updateConnectionStatus(false);
    });
    
    socket.on('connection_response', function(data) {
        console.log('Connection response:', data);
    });
    
    socket.on('gps_update', function(data) {
        updateGPSData(data);
    });
    
    socket.on('reference_reset', function(data) {
        console.log('Reference position reset:', data);
    });

    // Config progress events (native mode)
    socket.on('config_progress', function(data) {
        handleConfigProgress(data);
    });

    // HAT type changed (ros2 mode — frame_id detection)
    socket.on('hat_changed', function(data) {
        location.reload();
    });

    // NTRIP status updates (native mode, RTK HAT only)
    socket.on('ntrip_status', function(data) {
        updateNtripUI(data.state, data.message || null);
    });
}

function setupUIHandlers() {
    const resetBtn = document.getElementById('reset-btn');
    if (resetBtn) {
        resetBtn.addEventListener('click', function() {
            console.log('Reset Origin button clicked');
            map.resetOrigin();
            document.getElementById('pos-x').textContent = '0.00 m';
            document.getElementById('pos-y').textContent = '0.00 m';
            document.getElementById('distance').textContent = '0.00 m';
            socket.emit('reset_reference');
        });
    }
}

function updateConnectionStatus(connected) {
    const statusEl = document.getElementById('connection-status');
    if (statusEl) {
        statusEl.textContent = connected ? 'Connected' : 'Disconnected';
        statusEl.className = connected ? 'status-value connected' : 'status-value disconnected';
    }
}

function updateGPSData(data) {
    if (!data || !data.pvt) return;
    
    const pvt = data.pvt;
    
    // Store last GPS data for tab switching
    window.lastGPSData = data;
    
    // Update map position
    if (data.has_reference) {
        map.updatePosition(data.offset_x, data.offset_y);
        
        // Update position displays
        document.getElementById('pos-x').textContent = `${data.offset_x.toFixed(2)} m`;
        document.getElementById('pos-y').textContent = `${data.offset_y.toFixed(2)} m`;
        
        const distance = Math.sqrt(data.offset_x ** 2 + data.offset_y ** 2);
        document.getElementById('distance').textContent = `${distance.toFixed(2)} m`;
    }
    
    // Update OSM map if initialized
    if (osmMap && pvt.latitude && pvt.longitude) {
        updateOSMMap(pvt.latitude, pvt.longitude);
    }
    
    // Update status bar
    document.getElementById('fix-status').textContent = pvt.fix_status;
    document.getElementById('satellites').textContent = pvt.visible_satellites;
    
    // Update data table
    updateDataField('data-utc-time', pvt.utc_time);
    updateDataField('data-date', pvt.date);
    updateDataField('data-time-accuracy', pvt.time_accuracy);
    updateDataField('data-fix-quality', pvt.fix_quality);
    updateDataField('data-fix-status', pvt.fix_status);
    updateDataField('data-fix-type', pvt.fix_type);
    updateDataField('data-satellites', pvt.visible_satellites);
    updateDataField('data-latitude', `${pvt.latitude.toFixed(7)}°`);
    updateDataField('data-longitude', `${pvt.longitude.toFixed(7)}°`);
    updateDataField('data-altitude-msl', `${pvt.altitude_msl.toFixed(1)} m`);
    updateDataField('data-speed', `${pvt.speed_over_ground.toFixed(2)} m/s`);
    updateDataField('data-heading', `${pvt.heading.toFixed(1)}°`);

    // Native mode: extra fields
    if (window.APP_MODE === 'native' || window.APP_MODE === 'ros2') {
        // HDOP from DOP object (not pvt)
        if (data.dop) {
            updateDataField('data-hdop', data.dop.horizontal.toFixed(2));
            updateDataField('data-gdop', data.dop.geometric.toFixed(2));
            updateDataField('data-pdop', data.dop.position.toFixed(2));
            updateDataField('data-vdop', data.dop.vertical.toFixed(2));
            updateDataField('data-tdop', data.dop.time.toFixed(2));
            updateDataField('data-ndop', data.dop.northing.toFixed(2));
            updateDataField('data-edop', data.dop.easting.toFixed(2));
        }

        // Altitude WGS84
        if (pvt.altitude !== undefined) {
            updateDataField('data-altitude', `${pvt.altitude.toFixed(1)} m`);
        }

        // Accuracy
        if (pvt.horizontal_accuracy !== undefined) {
            updateDataField('data-hacc', `${pvt.horizontal_accuracy.toFixed(2)} m`);
            updateDataField('data-vacc', `${pvt.vertical_accuracy.toFixed(2)} m`);
            updateDataField('data-sacc', `${pvt.speed_accuracy.toFixed(2)} m/s`);
            updateDataField('data-headacc', `${pvt.heading_accuracy.toFixed(1)}°`);
        }

        // Geofencing
        if (data.geofencing) {
            updateDataField('data-geo-status', data.geofencing.status);
            updateDataField('data-geo-count', data.geofencing.number_of_geofences);
            updateDataField('data-geo-combined', data.geofencing.combined_state || '-');
            if (data.geofencing.geofences && data.geofencing.geofences.length > 0) {
                updateDataField('data-geo-fences', data.geofencing.geofences.map((s, i) => `#${i+1}: ${s}`).join(', '));
            } else {
                updateDataField('data-geo-fences', '-');
            }
        }

        // RF Blocks
        if (data.rf_blocks) {
            updateRfBlocks(data.rf_blocks);
        }

        // Time Mark
        if (data.time_mark) {
            updateTimeMark(data.time_mark);
        }

        // Satellites → sky view
        if (data.satellites) {
            updateSkyView(data.satellites);
        }

        // RF Analyzer → spectrum chart + RF status
        if (data.spectrum || data.rf_blocks) {
            updateRfAnalyzer(data);
        }
    } else {
        // TTY mode: HDOP from pvt
        updateDataField('data-hdop', `${pvt.hdop.toFixed(2)}`);
    }
}

function updateRfBlocks(rfBlocks) {
    const container = document.getElementById('rf-blocks-container');
    if (!container) return;

    if (!rfBlocks || rfBlocks.length === 0) {
        container.innerHTML = '<p class="rf-no-data">No RF data</p>';
        return;
    }

    let html = '';
    for (const rf of rfBlocks) {
        const jammingClass = rf.jamming_state === 'OK' ? 'jamming-ok'
            : rf.jamming_state === 'Warning' ? 'jamming-warning'
            : rf.jamming_state === 'Critical' ? 'jamming-critical'
            : 'jamming-unknown';

        html += `
        <div class="rf-block-card">
            <div class="rf-block-header">Band: ${rf.band}</div>
            <div class="rf-block-row">
                <span class="rf-block-label">Jamming</span>
                <span class="rf-block-value ${jammingClass}">${rf.jamming_state}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">Antenna</span>
                <span class="rf-block-value">${rf.antenna_status}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">Power</span>
                <span class="rf-block-value">${rf.antenna_power}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">Noise/ms</span>
                <span class="rf-block-value">${rf.noise_per_ms}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">AGC</span>
                <span class="rf-block-value">${rf.agc_monitor.toFixed(1)}%</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">CW Supp.</span>
                <span class="rf-block-value">${rf.cw_suppression.toFixed(1)} dB</span>
            </div>
        </div>`;
    }
    container.innerHTML = html;
}

function updateTimeMark(tm) {
    updateDataField('data-tm-status', 'Active');
    updateDataField('data-tm-mode', tm.mode);
    updateDataField('data-tm-run', tm.run);
    updateDataField('data-tm-timebase', tm.time_base);
    updateDataField('data-tm-timevalid', tm.time_valid ? 'Yes' : 'No');
    updateDataField('data-tm-utc', tm.utc_available ? 'Yes' : 'No');
    updateDataField('data-tm-count', tm.count);
    updateDataField('data-tm-accuracy', tm.accuracy_estimate);

    // Format TOW rising: week + tow_ms + sub_ns
    const risingMs = tm.tow_rising_ms;
    const risingSub = tm.tow_sub_rising_ns;
    updateDataField('data-tm-rising',
        `W${tm.week_number_rising} ${(risingMs / 1000).toFixed(3)}s +${risingSub}ns`);

    // Format TOW falling
    const fallingMs = tm.tow_falling_ms;
    const fallingSub = tm.tow_sub_falling_ns;
    updateDataField('data-tm-falling',
        `W${tm.week_number_falling} ${(fallingMs / 1000).toFixed(3)}s +${fallingSub}ns`);
}

function updateDataField(id, value) {
    const element = document.getElementById(id);
    if (element) {
        element.textContent = value;
    }
}

// =======================
// RF Analyzer - Spectrum Chart (MON-SPAN) + RF Status (MON-RF)
// =======================

const RF_BLOCK_COLORS = ['#4fc3f7', '#ffb74d', '#81c784', '#e57373'];
const RF_BLOCK_LABELS = ['L1', 'L2/L5', 'RF2', 'RF3'];

function drawSingleSpectrum(canvas, block, blockIndex) {
    const container = canvas.parentElement;
    const rect = container.getBoundingClientRect();
    const width = rect.width;
    const height = rect.height;
    if (width < 40 || height < 40) return;

    const dpr = window.devicePixelRatio || 1;
    canvas.width = width * dpr;
    canvas.height = height * dpr;
    canvas.style.width = width + 'px';
    canvas.style.height = height + 'px';

    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    const color = RF_BLOCK_COLORS[blockIndex % RF_BLOCK_COLORS.length];

    // Background
    ctx.fillStyle = '#0a0a1a';
    ctx.fillRect(0, 0, width, height);

    const data = block.data;
    if (!data || data.length === 0) {
        ctx.fillStyle = '#666';
        ctx.font = '13px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('No spectrum data', width / 2, height / 2);
        return;
    }

    const centerFreqMHz = block.center_freq / 1e6;
    const spanMHz = block.span / 1e6;
    const startFreqMHz = centerFreqMHz - spanMHz / 2;
    const endFreqMHz = centerFreqMHz + spanMHz / 2;

    // Header area for label, extra headroom above data
    const headerH = 28;
    const margin = { top: headerH, right: 15, bottom: 32, left: 50 };
    const plotW = width - margin.left - margin.right;
    const plotH = height - margin.top - margin.bottom;
    if (plotW < 10 || plotH < 10) return;

    // Find amplitude range with 15% headroom
    let maxVal = 0;
    for (let i = 0; i < data.length; i++) {
        if (data[i] > maxVal) maxVal = data[i];
    }
    if (maxVal === 0) maxVal = 255;
    const yScale = maxVal * 1.15;

    // --- Header label ---
    const label = RF_BLOCK_LABELS[blockIndex] || ('RF' + block.id);
    ctx.fillStyle = color;
    ctx.font = 'bold 12px monospace';
    ctx.textAlign = 'left';
    ctx.fillText(
        `${label}  ·  ${centerFreqMHz.toFixed(2)} MHz  ·  span ${spanMHz.toFixed(1)} MHz  ·  gain ${block.gain}`,
        margin.left, headerH - 9
    );
    // thin separator
    ctx.strokeStyle = '#222';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(margin.left, headerH - 3);
    ctx.lineTo(margin.left + plotW, headerH - 3);
    ctx.stroke();

    // --- Grid ---
    ctx.strokeStyle = '#1a1a2e';
    ctx.lineWidth = 0.5;

    // Horizontal grid (amplitude)
    const ySteps = 4;
    ctx.font = '10px monospace';
    ctx.fillStyle = '#555';
    ctx.textAlign = 'right';
    for (let i = 0; i <= ySteps; i++) {
        const y = margin.top + (plotH / ySteps) * i;
        ctx.beginPath();
        ctx.moveTo(margin.left, y);
        ctx.lineTo(margin.left + plotW, y);
        ctx.stroke();
        const val = Math.round(yScale * (1 - i / ySteps));
        ctx.fillText(val.toString(), margin.left - 5, y + 3);
    }

    // Vertical grid (frequency)
    const freqRange = endFreqMHz - startFreqMHz;
    let freqStep;
    if (freqRange > 5) freqStep = 1;
    else if (freqRange > 2) freqStep = 0.5;
    else if (freqRange > 0.5) freqStep = 0.1;
    else freqStep = 0.05;

    ctx.textAlign = 'center';
    ctx.fillStyle = '#555';
    const firstTick = Math.ceil(startFreqMHz / freqStep) * freqStep;
    for (let freq = firstTick; freq <= endFreqMHz; freq += freqStep) {
        const xFrac = (freq - startFreqMHz) / freqRange;
        const x = margin.left + xFrac * plotW;
        ctx.strokeStyle = '#1a1a2e';
        ctx.beginPath();
        ctx.moveTo(x, margin.top);
        ctx.lineTo(x, margin.top + plotH);
        ctx.stroke();
        ctx.fillText(freq.toFixed(freqStep < 0.1 ? 2 : 1), x, margin.top + plotH + 14);
    }

    // --- Spectrum line ---
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let i = 0; i < data.length; i++) {
        const x = margin.left + (i / (data.length - 1)) * plotW;
        const y = margin.top + plotH - (data[i] / yScale) * plotH;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Filled area
    ctx.globalAlpha = 0.12;
    ctx.fillStyle = color;
    ctx.lineTo(margin.left + plotW, margin.top + plotH);
    ctx.lineTo(margin.left, margin.top + plotH);
    ctx.closePath();
    ctx.fill();
    ctx.globalAlpha = 1.0;

    // --- Axis labels ---
    ctx.fillStyle = '#666';
    ctx.font = '10px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('MHz', margin.left + plotW + 2, margin.top + plotH + 14);

    ctx.save();
    ctx.translate(10, margin.top + plotH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('Amp', 0, 0);
    ctx.restore();

    // Plot border
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    ctx.strokeRect(margin.left, margin.top, plotW, plotH);
}

function drawSpectrumChart(spectrumBlocks) {
    const container = document.getElementById('spectrum-charts-container');
    if (!container) return;

    if (!spectrumBlocks || spectrumBlocks.length === 0) {
        container.innerHTML = '<p style="color:#666;text-align:center;font:14px monospace;padding:40px 0">Waiting for spectrum data…</p>';
        return;
    }

    // Ensure we have the right number of chart wrappers
    const existing = container.querySelectorAll('.spectrum-single-wrap');
    if (existing.length !== spectrumBlocks.length) {
        container.innerHTML = '';
        for (let i = 0; i < spectrumBlocks.length; i++) {
            const wrap = document.createElement('div');
            wrap.className = 'spectrum-single-wrap';
            const cvs = document.createElement('canvas');
            cvs.className = 'spectrum-single-canvas';
            wrap.appendChild(cvs);
            container.appendChild(wrap);
        }
    }

    const wraps = container.querySelectorAll('.spectrum-single-wrap');
    for (let i = 0; i < spectrumBlocks.length; i++) {
        const canvas = wraps[i].querySelector('canvas');
        drawSingleSpectrum(canvas, spectrumBlocks[i], i);
    }
}

function updateRfAnalyzerStatus(rfBlocks) {
    const container = document.getElementById('rfanalyzer-rf-status');
    if (!container) return;

    if (!rfBlocks || rfBlocks.length === 0) {
        container.innerHTML = '<p class="rf-no-data">No RF data</p>';
        return;
    }

    let html = '';
    for (const rf of rfBlocks) {
        const jammingClass = rf.jamming_state === 'OK' ? 'jamming-ok'
            : rf.jamming_state === 'Warning' ? 'jamming-warning'
            : rf.jamming_state === 'Critical' ? 'jamming-critical'
            : 'jamming-unknown';

        html += `
        <div class="rfanalyzer-status-card">
            <div class="rf-block-header">${rf.band}</div>
            <div class="rf-block-row">
                <span class="rf-block-label">Jamming</span>
                <span class="rf-block-value ${jammingClass}">${rf.jamming_state}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">Antenna</span>
                <span class="rf-block-value">${rf.antenna_status}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">Power</span>
                <span class="rf-block-value">${rf.antenna_power}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">Noise/ms</span>
                <span class="rf-block-value">${rf.noise_per_ms}</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">AGC</span>
                <span class="rf-block-value">${rf.agc_monitor.toFixed(1)}%</span>
            </div>
            <div class="rf-block-row">
                <span class="rf-block-label">CW Supp.</span>
                <span class="rf-block-value">${rf.cw_suppression.toFixed(1)} dB</span>
            </div>
        </div>`;
    }
    container.innerHTML = html;
}

function updateRfAnalyzer(data) {
    const rfEl = document.getElementById('rfanalyzer-map');
    if (!rfEl) return;

    drawSpectrumChart(data.spectrum);
    updateRfAnalyzerStatus(data.rf_blocks);
}

// =======================
// Sky View - Polar Satellite Plot
// =======================

const GNSS_COLORS = {
    'GPS':     '#4fc3f7',
    'Galileo': '#81c784',
    'GLONASS': '#e57373',
    'BeiDou':  '#ffb74d',
    'SBAS':    '#ce93d8',
    'QZSS':    '#fff176',
};

function getGnssColor(gnssId) {
    return GNSS_COLORS[gnssId] || '#888888';
}

function getGnssCssClass(gnssId) {
    const map = {
        'GPS': 'sat-gnss-gps', 'Galileo': 'sat-gnss-galileo',
        'GLONASS': 'sat-gnss-glonass', 'BeiDou': 'sat-gnss-beidou',
        'SBAS': 'sat-gnss-sbas', 'QZSS': 'sat-gnss-qzss',
    };
    return map[gnssId] || 'sat-gnss-other';
}

function drawSkyPlot(satellites) {
    const canvas = document.getElementById('sky-canvas');
    if (!canvas) return;

    // Reset inline size so container can grow on window maximize
    canvas.style.width = '';
    canvas.style.height = '';

    const container = canvas.parentElement;
    const rect = container.getBoundingClientRect();
    const size = Math.min(rect.width, rect.height);
    if (size < 10) return;

    const dpr = window.devicePixelRatio || 1;
    canvas.width = size * dpr;
    canvas.height = size * dpr;
    canvas.style.width = size + 'px';
    canvas.style.height = size + 'px';

    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    const cx = size / 2;
    const cy = size / 2;
    const maxR = size / 2 * 0.88;

    // Background
    ctx.fillStyle = '#0a0a1a';
    ctx.fillRect(0, 0, size, size);

    // Elevation rings (90° center, 0° edge)
    ctx.strokeStyle = '#2a2a3e';
    ctx.lineWidth = 1;
    for (let el = 0; el <= 90; el += 30) {
        const r = maxR * (1 - el / 90);
        ctx.beginPath();
        ctx.arc(cx, cy, r, 0, Math.PI * 2);
        ctx.stroke();
        // Label
        if (el > 0 && el < 90) {
            ctx.fillStyle = '#555';
            ctx.font = '11px monospace';
            ctx.textAlign = 'center';
            ctx.fillText(`${el}°`, cx, cy - r + 13);
        }
    }

    // Compass lines (N/E/S/W)
    const dirs = [
        { label: 'N', angle: -90 },
        { label: 'E', angle: 0 },
        { label: 'S', angle: 90 },
        { label: 'W', angle: 180 },
    ];
    ctx.strokeStyle = '#2a2a3e';
    ctx.lineWidth = 1;
    for (const d of dirs) {
        const rad = d.angle * Math.PI / 180;
        ctx.beginPath();
        ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(rad) * maxR, cy + Math.sin(rad) * maxR);
        ctx.stroke();
        // Label
        ctx.fillStyle = '#888';
        ctx.font = 'bold 14px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        const lr = maxR + 14;
        ctx.fillText(d.label, cx + Math.cos(rad) * lr, cy + Math.sin(rad) * lr);
    }

    // Draw satellites
    for (const sat of satellites) {
        if (sat.elevation < 0) continue;

        const r = maxR * (1 - sat.elevation / 90);
        // Azimuth: 0° = North (up), clockwise → canvas: -90° offset
        const aRad = (sat.azimuth - 90) * Math.PI / 180;
        const sx = cx + Math.cos(aRad) * r;
        const sy = cy + Math.sin(aRad) * r;

        const color = getGnssColor(sat.gnss_id);

        // Dot
        const dotR = sat.used_in_fix ? 7 : 5;
        ctx.beginPath();
        ctx.arc(sx, sy, dotR, 0, Math.PI * 2);
        if (sat.used_in_fix) {
            ctx.fillStyle = color;
            ctx.fill();
        } else {
            ctx.strokeStyle = color;
            ctx.lineWidth = 1.5;
            ctx.stroke();
        }

        // SV label
        ctx.fillStyle = color;
        ctx.font = '10px monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'bottom';
        ctx.fillText(`${sat.sv_id}`, sx, sy - dotR - 2);
    }

    // Center dot
    ctx.beginPath();
    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
    ctx.fillStyle = '#fff';
    ctx.fill();
}

function updateSatTable(satellites) {
    const tbody = document.getElementById('sat-table-body');
    if (!tbody) return;

    // Sort: used first, then by C/N0 descending
    const sorted = [...satellites].sort((a, b) => {
        if (a.used_in_fix !== b.used_in_fix) return b.used_in_fix ? 1 : -1;
        return b.cno - a.cno;
    });

    let html = '';
    for (const sat of sorted) {
        if (sat.cno === 0 && !sat.used_in_fix) continue; // skip silent sats
        const cls = getGnssCssClass(sat.gnss_id);
        const rowCls = sat.used_in_fix ? 'sat-used' : '';
        const cnoColor = sat.cno >= 35 ? '#00ff88' : sat.cno >= 20 ? '#ffaa00' : '#ff4444';
        const cnoWidth = Math.min(sat.cno, 55) / 55 * 40;
        html += `<tr class="${rowCls}">
            <td class="${cls}">${sat.gnss_id}</td>
            <td>${sat.sv_id}</td>
            <td><span class="sat-cno-bar" style="width:${cnoWidth}px;background:${cnoColor}"></span>${sat.cno}</td>
            <td>${sat.elevation}°</td>
            <td>${sat.azimuth}°</td>
            <td>${sat.used_in_fix ? '✓' : ''}</td>
        </tr>`;
    }
    tbody.innerHTML = html;

    // Summary
    const total = satellites.filter(s => s.cno > 0).length;
    const used = satellites.filter(s => s.used_in_fix).length;
    const elTotal = document.getElementById('sky-total');
    const elUsed = document.getElementById('sky-used');
    if (elTotal) elTotal.textContent = total;
    if (elUsed) elUsed.textContent = used;

    // Per-constellation usage summary
    const constellationEl = document.getElementById('constellation-summary');
    if (constellationEl) {
        constellationEl.textContent = '';
        const counts = {};
        for (const sat of satellites) {
            if (sat.used_in_fix) {
                counts[sat.gnss_id] = (counts[sat.gnss_id] || 0) + 1;
            }
        }
        const order = ['GPS', 'Galileo', 'GLONASS', 'BeiDou', 'SBAS', 'QZSS', 'IMES'];
        const entries = [];
        for (const name of order) {
            if (counts[name]) entries.push(name);
        }
        for (const name of Object.keys(counts)) {
            if (!order.includes(name)) entries.push(name);
        }
        const prefix = document.createTextNode(entries.length > 0 ? '\u{1F6F0}\uFE0F Used: ' : '\u{1F6F0}\uFE0F No satellites used in fix');
        constellationEl.appendChild(prefix);
        entries.forEach((name, i) => {
            if (i > 0) {
                const sep = document.createTextNode(' | ');
                constellationEl.appendChild(sep);
            }
            const span = document.createElement('span');
            span.style.color = getGnssColor(name);
            span.textContent = name + ': ' + counts[name];
            constellationEl.appendChild(span);
        });
    }
}

function updateSkyView(satellites) {
    drawSkyPlot(satellites);
    updateSatTable(satellites);

    // Constrain table height to 99% of sky canvas height
    const skyCanvas = document.getElementById('sky-canvas');
    const tableWrap = document.querySelector('.skyview-table-wrap');
    if (skyCanvas && tableWrap) {
        const canvasH = skyCanvas.getBoundingClientRect().height;
        if (canvasH > 50) {
            tableWrap.style.maxHeight = Math.floor(canvasH * 0.99) + 'px';
        }
    }
}

// Re-draw sky view on window resize so it scales back up
window.addEventListener('resize', () => {
    const skyviewEl = document.getElementById('skyview-map');
    if (skyviewEl && skyviewEl.classList.contains('active') && window.lastGPSData && window.lastGPSData.satellites) {
        updateSkyView(window.lastGPSData.satellites);
    }
    const rfEl = document.getElementById('rfanalyzer-map');
    if (rfEl && rfEl.classList.contains('active') && window.lastGPSData) {
        updateRfAnalyzer(window.lastGPSData);
    }
});

// =======================
// OpenStreetMap Integration
// =======================

let osmMap = null;
let osmMarker = null;
let osmTrail = null;
let osmTrailCoords = [];
let osmGeofenceCircles = [];

function initOSMMap() {
    if (osmMap) return;
    
    // Initialize Leaflet map
    osmMap = L.map('osm-map', {
        zoomControl: true,
        attributionControl: true
    }).setView([51.505, -0.09], 18); // Default view, will update on first GPS fix
    
    // Add OpenStreetMap tiles
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '© OpenStreetMap contributors'
    }).addTo(osmMap);
    
    // Create marker
    const markerIcon = L.divIcon({
        className: 'gps-marker',
        html: '<div class="gps-marker-dot"></div>',
        iconSize: [20, 20],
        iconAnchor: [10, 10]
    });
    
    osmMarker = L.marker([51.505, -0.09], { icon: markerIcon }).addTo(osmMap);
    
    // Create trail polyline
    osmTrail = L.polyline([], {
        color: '#00ff88',
        weight: 3,
        opacity: 0.7
    }).addTo(osmMap);
    
    // Add custom marker styles
    const style = document.createElement('style');
    style.textContent = `
        .gps-marker {
            background: none;
            border: none;
        }
        .gps-marker-dot {
            width: 20px;
            height: 20px;
            background: radial-gradient(circle, #00ff88 0%, #00ff88 40%, transparent 70%);
            border: 3px solid #00ff88;
            border-radius: 50%;
            box-shadow: 0 0 10px #00ff88;
            animation: pulse-marker 2s infinite;
        }
        @keyframes pulse-marker {
            0%, 100% { transform: scale(1); opacity: 1; }
            50% { transform: scale(1.2); opacity: 0.8; }
        }
    `;
    document.head.appendChild(style);

    // Add any geofence circles that were set before map init
    for (const c of osmGeofenceCircles) {
        c.addTo(osmMap);
    }
}

function updateOSMMap(lat, lon) {
    if (!osmMap) {
        initOSMMap();
    }
    
    // Update marker position
    const latlng = [lat, lon];
    osmMarker.setLatLng(latlng);
    
    // Update trail
    osmTrailCoords.push(latlng);
    if (osmTrailCoords.length > 100) {
        osmTrailCoords.shift();
    }
    osmTrail.setLatLngs(osmTrailCoords);
    
    // Center map on marker (only if first update or map not manually panned)
    if (osmTrailCoords.length === 1) {
        osmMap.setView(latlng, 18);
    } else {
        // Smooth pan to keep marker in view
        if (!osmMap.getBounds().contains(latlng)) {
            osmMap.panTo(latlng);
        }
    }
    
    // Update coordinates display
    document.getElementById('osm-lat').textContent = `${lat.toFixed(6)}°`;
    document.getElementById('osm-lon').textContent = `${lon.toFixed(6)}°`;
    document.getElementById('osm-zoom').textContent = osmMap.getZoom();
}

function updateOSMGeofences(geofences) {
    // Remove old circles
    for (const c of osmGeofenceCircles) {
        c.remove();
    }
    osmGeofenceCircles = [];

    if (!geofences || geofences.length === 0) return;

    for (let i = 0; i < geofences.length; i++) {
        const gf = geofences[i];
        const circle = L.circle([gf.lat, gf.lon], {
            radius: gf.radius,
            color: '#00b4ff',
            weight: 2,
            dashArray: '6 4',
            fillColor: '#00b4ff',
            fillOpacity: 0.08,
        });
        circle.bindTooltip(`GF${i + 1} (${gf.radius}m)`, { permanent: false });
        osmGeofenceCircles.push(circle);

        // Add to map if already initialized
        if (osmMap) {
            circle.addTo(osmMap);
        }
    }
}

function applyGeofencesToMaps(config) {
    const geo = config && config.geofencing;
    const fences = (geo && geo.geofences) || [];

    // OSM map
    updateOSMGeofences(fences);

    // Relative canvas map
    if (map) {
        map.setGeofences(fences);
    }
}

// =======================
// Tab Switching
// =======================

function setupTabs() {
    const tabs = document.querySelectorAll('.map-tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const tabName = tab.dataset.tab;
            
            // Update active tab
            tabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');
            
            // Update active map view
            document.querySelectorAll('.map-view').forEach(view => {
                view.classList.remove('active');
                view.style.display = 'none';
            });
            
            const activeView = document.getElementById(`${tabName}-map`);
            if (activeView) {
                activeView.classList.add('active');
                activeView.style.display = tabName === 'skyview' ? 'flex' : 'block';
                
                // Re-setup relative map canvas when switching back
                if (tabName === 'relative' && map) {
                    setTimeout(() => map.setupCanvas(), 50);
                }

                // Initialize OSM map when terrain tab is first opened
                if (tabName === 'terrain' && !osmMap) {
                    setTimeout(() => {
                        initOSMMap();
                        if (window.lastGPSData && window.lastGPSData.pvt) {
                            updateOSMMap(
                                window.lastGPSData.pvt.latitude,
                                window.lastGPSData.pvt.longitude
                            );
                        }
                    }, 100);
                }
                
                // Invalidate Leaflet map size when switching to terrain
                if (tabName === 'terrain' && osmMap) {
                    setTimeout(() => osmMap.invalidateSize(), 100);
                }

                // Re-draw sky plot when switching to skyview (canvas needs resize)
                if (tabName === 'skyview' && window.lastGPSData && window.lastGPSData.satellites) {
                    setTimeout(() => updateSkyView(window.lastGPSData.satellites), 50);
                }

                // Re-draw spectrum chart when switching to RF Analyzer
                if (tabName === 'rfanalyzer' && window.lastGPSData) {
                    setTimeout(() => updateRfAnalyzer(window.lastGPSData), 50);
                }
            }
        });
    });
}


// ─── Data Panel Tabs (Navigation / Configuration) ──────────────────────────

function setupDataTabs() {
    const tabs = document.querySelectorAll('.data-tab');
    if (!tabs.length) return;

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const tabName = tab.dataset.dtab;

            tabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');

            document.querySelectorAll('.data-pane').forEach(p => {
                p.classList.remove('active');
                p.style.display = 'none';
            });

            const pane = document.getElementById(`${tabName}-pane`);
            if (pane) {
                pane.classList.add('active');
                pane.style.display = 'block';
            }
        });
    });
}


// ─── Configuration Panel Logic ─────────────────────────────────────────────

function setupConfigPanel() {
    if (window.APP_MODE !== 'native' && window.APP_MODE !== 'ros2') return;

    const tpActive = document.getElementById('cfg-tp-active');
    const tpDetails = document.getElementById('cfg-tp-details');
    const tpNofixEn = document.getElementById('cfg-tp-nofix-en');
    const tpNofixDetails = document.getElementById('cfg-tp-nofix-details');
    const geoEn = document.getElementById('cfg-geo-en');
    const geoDetails = document.getElementById('cfg-geo-details');

    if (!tpActive) return; // config panel not in DOM

    // Toggle timepulse details
    tpActive.addEventListener('change', () => {
        tpDetails.style.display = tpActive.checked ? '' : 'none';
    });

    // Toggle no-fix pulse details
    tpNofixEn.addEventListener('change', () => {
        tpNofixDetails.style.display = tpNofixEn.checked ? '' : 'none';
    });

    // Toggle geofencing details
    if (geoEn && geoDetails) {
        geoEn.addEventListener('change', () => {
            geoDetails.style.display = geoEn.checked ? '' : 'none';
        });
    }

    // Toggle PIO pin details
    const geoPinEn = document.getElementById('cfg-geo-pin-en');
    const geoPinDetails = document.getElementById('cfg-geo-pin-details');
    if (geoPinEn && geoPinDetails) {
        geoPinEn.addEventListener('change', () => {
            geoPinDetails.style.display = geoPinEn.checked ? '' : 'none';
        });
    }

    // Add geofence button
    const geoAddBtn = document.getElementById('cfg-geo-add');
    if (geoAddBtn) {
        geoAddBtn.addEventListener('click', () => {
            addGeofenceRow();
        });
    }

    // RTK toggles
    const rtkEn = document.getElementById('cfg-rtk-en');
    const rtkDetails = document.getElementById('cfg-rtk-details');
    const rtkMode = document.getElementById('cfg-rtk-mode');
    const rtkBaseDetails = document.getElementById('cfg-rtk-base-details');
    const rtkBaseMode = document.getElementById('cfg-rtk-basemode');
    const rtkSiDetails = document.getElementById('cfg-rtk-si-details');
    const rtkFpDetails = document.getElementById('cfg-rtk-fp-details');
    const rtkFpType = document.getElementById('cfg-rtk-fp-type');
    const rtkEcefDetails = document.getElementById('cfg-rtk-ecef-details');
    const rtkLlaDetails = document.getElementById('cfg-rtk-lla-details');

    if (rtkEn) {
        rtkEn.addEventListener('change', () => {
            rtkDetails.style.display = rtkEn.checked ? '' : 'none';
        });

        rtkMode.addEventListener('change', () => {
            rtkBaseDetails.style.display = rtkMode.value === '0' ? '' : 'none';
        });

        rtkBaseMode.addEventListener('change', () => {
            rtkSiDetails.style.display = rtkBaseMode.value === '0' ? '' : 'none';
            rtkFpDetails.style.display = rtkBaseMode.value === '1' ? '' : 'none';
        });

        rtkFpType.addEventListener('change', () => {
            rtkEcefDetails.style.display = rtkFpType.value === '0' ? '' : 'none';
            rtkLlaDetails.style.display = rtkFpType.value === '1' ? '' : 'none';
        });
    }

    // NTRIP connect / disconnect buttons (native mode, RTK HAT)
    const ntripConnectBtn = document.getElementById('ntrip-connect-btn');
    const ntripDisconnectBtn = document.getElementById('ntrip-disconnect-btn');
    if (ntripConnectBtn) {
        ntripConnectBtn.addEventListener('click', ntripConnect);
        ntripDisconnectBtn.addEventListener('click', ntripDisconnect);
    }

    // Time Base toggles
    const tbEn = document.getElementById('cfg-tb-en');
    const tbDetails = document.getElementById('cfg-tb-details');
    const tbBaseMode = document.getElementById('cfg-tb-basemode');
    const tbSiDetails = document.getElementById('cfg-tb-si-details');
    const tbFpDetails = document.getElementById('cfg-tb-fp-details');
    const tbFpType = document.getElementById('cfg-tb-fp-type');
    const tbEcefDetails = document.getElementById('cfg-tb-ecef-details');
    const tbLlaDetails = document.getElementById('cfg-tb-lla-details');

    if (tbEn) {
        tbEn.addEventListener('change', () => {
            tbDetails.style.display = tbEn.checked ? '' : 'none';
        });

        tbBaseMode.addEventListener('change', () => {
            tbSiDetails.style.display = tbBaseMode.value === '0' ? '' : 'none';
            tbFpDetails.style.display = tbBaseMode.value === '1' ? '' : 'none';
        });

        tbFpType.addEventListener('change', () => {
            tbEcefDetails.style.display = tbFpType.value === '0' ? '' : 'none';
            tbLlaDetails.style.display = tbFpType.value === '1' ? '' : 'none';
        });
    }

    // Load config button
    document.getElementById('cfg-load-btn').addEventListener('click', loadConfig);

    // Send config button
    document.getElementById('cfg-send-btn').addEventListener('click', sendConfig);
}

let geoFenceCount = 0;

function addGeofenceRow(lat, lon, radius) {
    if (geoFenceCount >= 4) return;

    const container = document.getElementById('cfg-geo-fences');
    if (!container) return;

    geoFenceCount++;

    // Add labels row if first fence
    if (geoFenceCount === 1) {
        const labels = document.createElement('div');
        labels.className = 'cfg-geo-labels';
        labels.innerHTML = '<span>Lat (°)</span><span>Lon (°)</span><span>Radius (m)</span><span></span>';
        container.prepend(labels);
    }

    const row = document.createElement('div');
    row.className = 'cfg-geo-fence';
    row.innerHTML = `
        <input type="number" step="0.000001" min="-90" max="90" value="${lat ?? ''}" placeholder="Lat">
        <input type="number" step="0.000001" min="-180" max="180" value="${lon ?? ''}" placeholder="Lon">
        <input type="number" step="1" min="1" value="${radius ?? ''}" placeholder="Radius">
        <button class="cfg-fence-remove" title="Remove">✕</button>
    `;

    row.querySelector('.cfg-fence-remove').addEventListener('click', () => {
        row.remove();
        geoFenceCount--;
        // Remove labels if no fences left
        if (geoFenceCount === 0) {
            const labels = container.querySelector('.cfg-geo-labels');
            if (labels) labels.remove();
        }
        updateAddFenceBtn();
    });

    container.appendChild(row);
    updateAddFenceBtn();
}

function updateAddFenceBtn() {
    const btn = document.getElementById('cfg-geo-add');
    if (!btn) return;
    btn.disabled = geoFenceCount >= 4;
    btn.textContent = geoFenceCount >= 4 ? 'Max 4 geofences' : '+ Add Geofence';
}

function clearGeofenceRows() {
    const container = document.getElementById('cfg-geo-fences');
    if (!container) return;
    container.innerHTML = '';
    geoFenceCount = 0;
}

function populateFormFromConfig(config) {
    // Rate
    document.getElementById('cfg-rate').value = config.measurement_rate_hz || 1;

    // Dynamic model
    document.getElementById('cfg-dynmodel').value = config.dynamic_model ?? 2;

    // Timepulse
    const tp = config.timepulse_pin_config;
    const tpActive = document.getElementById('cfg-tp-active');
    if (tp && tp.active !== false) {
        tpActive.checked = true;
        document.getElementById('cfg-tp-details').style.display = '';
        if (tp.fixed_pulse) {
            document.getElementById('cfg-tp-freq').value = tp.fixed_pulse.frequency || 1;
            document.getElementById('cfg-tp-pw').value = tp.fixed_pulse.pulse_width ?? 0.1;
        }
        document.getElementById('cfg-tp-polarity').value = tp.polarity ?? 1;

        const nofixEn = document.getElementById('cfg-tp-nofix-en');
        if (tp.pulse_when_no_fix) {
            nofixEn.checked = true;
            document.getElementById('cfg-tp-nofix-details').style.display = '';
            document.getElementById('cfg-tp-nofix-freq').value = tp.pulse_when_no_fix.frequency || 1;
            document.getElementById('cfg-tp-nofix-pw').value = tp.pulse_when_no_fix.pulse_width ?? 0.1;
        } else {
            nofixEn.checked = false;
            document.getElementById('cfg-tp-nofix-details').style.display = 'none';
        }
    } else {
        tpActive.checked = false;
        document.getElementById('cfg-tp-details').style.display = 'none';
    }

    // Geofencing
    const geo = config.geofencing;
    const geoEn = document.getElementById('cfg-geo-en');
    if (geoEn) {
        clearGeofenceRows();
        if (geo && geo.geofences && geo.geofences.length > 0) {
            geoEn.checked = true;
            document.getElementById('cfg-geo-details').style.display = '';
            document.getElementById('cfg-geo-conf').value = geo.confidence_level ?? 3;
            const geoPinEn = document.getElementById('cfg-geo-pin-en');
            const geoPinDetails = document.getElementById('cfg-geo-pin-details');
            if (geo.pin_polarity !== undefined && geo.pin_polarity !== null) {
                geoPinEn.checked = true;
                geoPinDetails.style.display = '';
                document.getElementById('cfg-geo-pin-pol').value = geo.pin_polarity;
            } else {
                geoPinEn.checked = false;
                geoPinDetails.style.display = 'none';
            }
            for (const f of geo.geofences) {
                addGeofenceRow(f.lat, f.lon, f.radius);
            }
        } else {
            geoEn.checked = false;
            document.getElementById('cfg-geo-details').style.display = 'none';
        }
    }

    // RTK
    const rtk = config.rtk;
    const rtkEn = document.getElementById('cfg-rtk-en');
    if (rtkEn) {
        if (rtk && rtk.mode !== undefined && rtk.mode !== null) {
            rtkEn.checked = true;
            document.getElementById('cfg-rtk-details').style.display = '';
            document.getElementById('cfg-rtk-mode').value = rtk.mode;

            const isBase = parseInt(rtk.mode) === 0;
            document.getElementById('cfg-rtk-base-details').style.display = isBase ? '' : 'none';

            if (isBase && rtk.base) {
                const baseMode = rtk.base.base_mode ?? 0;
                document.getElementById('cfg-rtk-basemode').value = baseMode;

                document.getElementById('cfg-rtk-si-details').style.display = parseInt(baseMode) === 0 ? '' : 'none';
                document.getElementById('cfg-rtk-fp-details').style.display = parseInt(baseMode) === 1 ? '' : 'none';

                if (parseInt(baseMode) === 0 && rtk.base.survey_in) {
                    document.getElementById('cfg-rtk-si-obs').value = rtk.base.survey_in.minimum_observation_time_s ?? 120;
                    document.getElementById('cfg-rtk-si-acc').value = rtk.base.survey_in.required_position_accuracy_m ?? 50.0;
                }

                if (parseInt(baseMode) === 1 && rtk.base.fixed_position) {
                    const fp = rtk.base.fixed_position;
                    const posType = fp.position_type ?? 1;
                    document.getElementById('cfg-rtk-fp-type').value = posType;
                    document.getElementById('cfg-rtk-fp-acc').value = fp.position_accuracy_m ?? 0.5;

                    document.getElementById('cfg-rtk-ecef-details').style.display = parseInt(posType) === 0 ? '' : 'none';
                    document.getElementById('cfg-rtk-lla-details').style.display = parseInt(posType) === 1 ? '' : 'none';

                    if (parseInt(posType) === 0 && fp.ecef) {
                        document.getElementById('cfg-rtk-ecef-x').value = fp.ecef.x_m ?? 0;
                        document.getElementById('cfg-rtk-ecef-y').value = fp.ecef.y_m ?? 0;
                        document.getElementById('cfg-rtk-ecef-z').value = fp.ecef.z_m ?? 0;
                    }
                    if (parseInt(posType) === 1 && fp.lla) {
                        document.getElementById('cfg-rtk-lla-lat').value = fp.lla.latitude_deg ?? 0;
                        document.getElementById('cfg-rtk-lla-lon').value = fp.lla.longitude_deg ?? 0;
                        document.getElementById('cfg-rtk-lla-h').value = fp.lla.height_m ?? 0;
                    }
                }
            }
        } else {
            rtkEn.checked = false;
            document.getElementById('cfg-rtk-details').style.display = 'none';
        }
    }

    // Timing (enable_time_mark + time_base)
    const timing = config.timing;
    const tbEn = document.getElementById('cfg-tb-en');
    if (tbEn) {
        const tb = timing && timing.time_base ? timing.time_base : null;
        if (tb && tb.base_mode !== undefined && tb.base_mode !== null) {
            tbEn.checked = true;
            document.getElementById('cfg-tb-details').style.display = '';
            const baseMode = tb.base_mode ?? 0;
            document.getElementById('cfg-tb-basemode').value = baseMode;

            document.getElementById('cfg-tb-si-details').style.display = parseInt(baseMode) === 0 ? '' : 'none';
            document.getElementById('cfg-tb-fp-details').style.display = parseInt(baseMode) === 1 ? '' : 'none';

            if (parseInt(baseMode) === 0 && tb.survey_in) {
                document.getElementById('cfg-tb-si-obs').value = tb.survey_in.minimum_observation_time_s ?? 120;
                document.getElementById('cfg-tb-si-acc').value = tb.survey_in.required_position_accuracy_m ?? 50.0;
            }

            if (parseInt(baseMode) === 1 && tb.fixed_position) {
                const fp = tb.fixed_position;
                const posType = fp.position_type ?? 1;
                document.getElementById('cfg-tb-fp-type').value = posType;
                document.getElementById('cfg-tb-fp-acc').value = fp.position_accuracy_m ?? 0.5;

                document.getElementById('cfg-tb-ecef-details').style.display = parseInt(posType) === 0 ? '' : 'none';
                document.getElementById('cfg-tb-lla-details').style.display = parseInt(posType) === 1 ? '' : 'none';

                if (parseInt(posType) === 0 && fp.ecef) {
                    document.getElementById('cfg-tb-ecef-x').value = fp.ecef.x_m ?? 0;
                    document.getElementById('cfg-tb-ecef-y').value = fp.ecef.y_m ?? 0;
                    document.getElementById('cfg-tb-ecef-z').value = fp.ecef.z_m ?? 0;
                }
                if (parseInt(posType) === 1 && fp.lla) {
                    document.getElementById('cfg-tb-lla-lat').value = fp.lla.latitude_deg ?? 0;
                    document.getElementById('cfg-tb-lla-lon').value = fp.lla.longitude_deg ?? 0;
                    document.getElementById('cfg-tb-lla-h').value = fp.lla.height_m ?? 0;
                }
            }
        } else {
            tbEn.checked = false;
            document.getElementById('cfg-tb-details').style.display = 'none';
        }
    }

    // Enable Time Mark
    const tmEn = document.getElementById('cfg-tm-en');
    if (tmEn) {
        tmEn.checked = !!(timing && timing.enable_time_mark);
    }

    // Save to Flash
    const saveFlashEl = document.getElementById('cfg-save-flash');
    if (saveFlashEl) {
        saveFlashEl.checked = !!config.save_to_flash;
    }

    // Enable L5 GPS
    const l5El = document.getElementById('cfg-l5-en');
    if (l5El) {
        if (config.enable_l5_gps === null || config.enable_l5_gps === undefined) {
            l5El.value = 'auto';
        } else {
            l5El.value = config.enable_l5_gps ? 'on' : 'off';
        }
    }

    // ROS 2 specific fields
    const ros2StdTopics = document.getElementById('cfg-ros2-stdtopics');
    if (ros2StdTopics) {
        ros2StdTopics.checked = config.publish_standard_topics !== false;
    }
    const ros2Ntrip = document.getElementById('cfg-ros2-ntrip');
    if (ros2Ntrip) {
        ros2Ntrip.checked = !!config.use_ntrip_rtcm;
    }
    const ros2SaveYaml = document.getElementById('cfg-ros2-save-yaml');
    if (ros2SaveYaml) {
        ros2SaveYaml.checked = !!config.save_to_yaml;
    }
}

function buildConfigFromForm() {
    const config = {
        measurement_rate_hz: parseInt(document.getElementById('cfg-rate').value) || 1,
        dynamic_model: parseInt(document.getElementById('cfg-dynmodel').value) || 2,
    };

    // Timepulse
    if (document.getElementById('cfg-tp-active').checked) {
        config.timepulse_pin_config = {
            active: true,
            fixed_pulse: {
                frequency: parseInt(document.getElementById('cfg-tp-freq').value) || 1,
                pulse_width: parseFloat(document.getElementById('cfg-tp-pw').value) || 0.1,
            },
            polarity: parseInt(document.getElementById('cfg-tp-polarity').value) || 1,
        };

        if (document.getElementById('cfg-tp-nofix-en').checked) {
            config.timepulse_pin_config.pulse_when_no_fix = {
                frequency: parseInt(document.getElementById('cfg-tp-nofix-freq').value) || 1,
                pulse_width: parseFloat(document.getElementById('cfg-tp-nofix-pw').value) || 0.1,
            };
        }
    } else {
        config.timepulse_pin_config = null;
    }

    // Geofencing
    const geoEnEl = document.getElementById('cfg-geo-en');
    if (geoEnEl && geoEnEl.checked) {
        const fenceRows = document.querySelectorAll('.cfg-geo-fence');
        const fences = [];
        fenceRows.forEach(row => {
            const inputs = row.querySelectorAll('input[type="number"]');
            const lat = parseFloat(inputs[0].value);
            const lon = parseFloat(inputs[1].value);
            const radius = parseFloat(inputs[2].value);
            if (!isNaN(lat) && !isNaN(lon) && !isNaN(radius) && radius > 0) {
                fences.push({ lat, lon, radius });
            }
        });
        if (fences.length > 0) {
            config.geofencing = {
                geofences: fences,
                confidence_level: parseInt(document.getElementById('cfg-geo-conf').value) || 3,
            };
            if (document.getElementById('cfg-geo-pin-en').checked) {
                config.geofencing.pin_polarity = parseInt(document.getElementById('cfg-geo-pin-pol').value) || 0;
            }
        } else {
            config.geofencing = null;
        }
    } else if (geoEnEl) {
        config.geofencing = null;
    }

    // RTK
    const rtkEn = document.getElementById('cfg-rtk-en');
    if (rtkEn && rtkEn.checked) {
        const rtkMode = parseInt(document.getElementById('cfg-rtk-mode').value);
        const rtk = { mode: rtkMode };

        if (rtkMode === 0) { // Base
            const baseMode = parseInt(document.getElementById('cfg-rtk-basemode').value);
            const base = { base_mode: baseMode };

            if (baseMode === 0) { // Survey-In
                base.survey_in = {
                    minimum_observation_time_s: parseInt(document.getElementById('cfg-rtk-si-obs').value) || 120,
                    required_position_accuracy_m: parseFloat(document.getElementById('cfg-rtk-si-acc').value) || 50.0,
                };
            } else { // Fixed Position
                const posType = parseInt(document.getElementById('cfg-rtk-fp-type').value);
                const fp = {
                    position_type: posType,
                    position_accuracy_m: parseFloat(document.getElementById('cfg-rtk-fp-acc').value) || 0.5,
                };
                if (posType === 0) { // ECEF
                    fp.ecef = {
                        x_m: parseFloat(document.getElementById('cfg-rtk-ecef-x').value) || 0,
                        y_m: parseFloat(document.getElementById('cfg-rtk-ecef-y').value) || 0,
                        z_m: parseFloat(document.getElementById('cfg-rtk-ecef-z').value) || 0,
                    };
                } else { // LLA
                    fp.lla = {
                        latitude_deg: parseFloat(document.getElementById('cfg-rtk-lla-lat').value) || 0,
                        longitude_deg: parseFloat(document.getElementById('cfg-rtk-lla-lon').value) || 0,
                        height_m: parseFloat(document.getElementById('cfg-rtk-lla-h').value) || 0,
                    };
                }
                base.fixed_position = fp;
            }
            rtk.base = base;
        }
        config.rtk = rtk;
    } else if (rtkEn) {
        config.rtk = null;
    }

    // Timing (enable_time_mark + time_base)
    const tbEn = document.getElementById('cfg-tb-en');
    const tmEnEl = document.getElementById('cfg-tm-en');
    const hasTimeBase = tbEn && tbEn.checked;
    const hasTimeMark = tmEnEl && tmEnEl.checked;

    if (hasTimeBase || hasTimeMark) {
        const timing = {
            enable_time_mark: !!hasTimeMark,
        };

        if (hasTimeBase) {
            const baseMode = parseInt(document.getElementById('cfg-tb-basemode').value);
            const tb = { base_mode: baseMode };

            if (baseMode === 0) { // Survey-In
                tb.survey_in = {
                    minimum_observation_time_s: parseInt(document.getElementById('cfg-tb-si-obs').value) || 120,
                    required_position_accuracy_m: parseFloat(document.getElementById('cfg-tb-si-acc').value) || 50.0,
                };
            } else { // Fixed Position
                const posType = parseInt(document.getElementById('cfg-tb-fp-type').value);
                const fp = {
                    position_type: posType,
                    position_accuracy_m: parseFloat(document.getElementById('cfg-tb-fp-acc').value) || 0.5,
                };
                if (posType === 0) { // ECEF
                    fp.ecef = {
                        x_m: parseFloat(document.getElementById('cfg-tb-ecef-x').value) || 0,
                        y_m: parseFloat(document.getElementById('cfg-tb-ecef-y').value) || 0,
                        z_m: parseFloat(document.getElementById('cfg-tb-ecef-z').value) || 0,
                    };
                } else { // LLA
                    fp.lla = {
                        latitude_deg: parseFloat(document.getElementById('cfg-tb-lla-lat').value) || 0,
                        longitude_deg: parseFloat(document.getElementById('cfg-tb-lla-lon').value) || 0,
                        height_m: parseFloat(document.getElementById('cfg-tb-lla-h').value) || 0,
                    };
                }
                tb.fixed_position = fp;
            }
            timing.time_base = tb;
        } else {
            timing.time_base = null;
        }

        config.timing = timing;
    } else {
        config.timing = null;
    }

    // Save to Flash
    const saveFlashEl = document.getElementById('cfg-save-flash');
    if (saveFlashEl) {
        config.save_to_flash = saveFlashEl.checked;
    }

    // Enable L5 GPS
    const l5El = document.getElementById('cfg-l5-en');
    if (l5El) {
        const v = l5El.value;
        config.enable_l5_gps = v === 'auto' ? null : v === 'on';
    }

    // ROS 2 specific fields
    const ros2StdTopics = document.getElementById('cfg-ros2-stdtopics');
    if (ros2StdTopics) {
        config.publish_standard_topics = ros2StdTopics.checked;
    }
    const ros2Ntrip = document.getElementById('cfg-ros2-ntrip');
    if (ros2Ntrip) {
        config.use_ntrip_rtcm = ros2Ntrip.checked;
    }
    const ros2SaveYaml = document.getElementById('cfg-ros2-save-yaml');
    if (ros2SaveYaml) {
        config.save_to_yaml = ros2SaveYaml.checked;
    }

    return config;
}

function showConfigStatus(message, isError) {
    const el = document.getElementById('config-status');
    el.textContent = message;
    el.className = 'config-status ' + (isError ? 'error' : 'success');
    el.style.display = 'block';
    setTimeout(() => { el.style.display = 'none'; }, 8000);
}

const PROGRESS_STEPS = {
    'stop':    15,
    'destroy': 30,
    'create':  45,
    'reset':   60,
    'config':  80,
    'reader':  95,
    'done':    100,
    'error':   100,
};

function handleConfigProgress(data) {
    const bar = document.getElementById('config-progress-bar');
    const msg = document.getElementById('config-progress-msg');
    const overlay = document.getElementById('config-progress');

    overlay.style.display = 'flex';
    bar.style.width = (PROGRESS_STEPS[data.step] || 0) + '%';
    msg.textContent = data.message;

    if (data.step === 'done') {
        bar.style.background = 'linear-gradient(90deg, var(--accent-cyan), var(--accent-green))';
        setTimeout(() => {
            overlay.style.display = 'none';
            bar.style.width = '0%';
            showConfigStatus(data.message, false);
            document.getElementById('cfg-send-btn').disabled = false;
        }, 1200);
        // Update geofences on maps — use the stored config from sendConfig
        // (avoids re-reading form which could see stale state due to timing)
        const cfgForMaps = _lastSentConfig || buildConfigFromForm();
        _lastSentConfig = null;
        applyGeofencesToMaps(cfgForMaps);
    } else if (data.step === 'error') {
        bar.style.background = '#e57373';
        setTimeout(() => {
            overlay.style.display = 'none';
            bar.style.width = '0%';
            bar.style.background = '';
            showConfigStatus(data.message, true);
            document.getElementById('cfg-send-btn').disabled = false;
        }, 2000);
    }
}

async function loadConfig(silent) {
    try {
        const resp = await fetch('/api/config');
        if (!resp.ok) {
            const err = await resp.json();
            if (!silent) showConfigStatus('Load failed: ' + (err.error || resp.statusText), true);
            return;
        }
        const config = await resp.json();
        populateFormFromConfig(config);
        applyGeofencesToMaps(config);
        if (!silent) showConfigStatus('Configuration loaded from device', false);
    } catch (e) {
        if (!silent) showConfigStatus('Load failed: ' + e.message, true);
    }
}

let _lastSentConfig = null;

async function sendConfig() {
    const config = buildConfigFromForm();

    // Basic validation
    if (config.measurement_rate_hz < 1 || config.measurement_rate_hz > 25) {
        showConfigStatus('Measurement rate must be 1-25 Hz', true);
        return;
    }

    _lastSentConfig = config;

    document.getElementById('cfg-send-btn').disabled = true;
    const overlay = document.getElementById('config-progress');
    const bar = document.getElementById('config-progress-bar');
    const msg = document.getElementById('config-progress-msg');
    overlay.style.display = 'flex';
    bar.style.width = '5%';
    bar.style.background = '';
    msg.textContent = 'Sending configuration...';

    try {
        const resp = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config),
        });

        if (!resp.ok) {
            const err = await resp.json().catch(() => ({}));
            // Progress handler will show error via socket
            if (!err.error) {
                handleConfigProgress({ step: 'error', message: 'Server error: ' + resp.statusText });
            }
        } else {
            // Fallback: apply geofences from HTTP success in case socket 'done' was missed
            applyGeofencesToMaps(config);
        }
    } catch (e) {
        handleConfigProgress({ step: 'error', message: 'Network error: ' + e.message });
    }
}


// ─── NTRIP Client (native mode, RTK HAT) ──────────────────────────────────

function updateNtripUI(status, errorMsg) {
    const badge = document.getElementById('ntrip-status');
    const connectBtn = document.getElementById('ntrip-connect-btn');
    const disconnectBtn = document.getElementById('ntrip-disconnect-btn');
    if (!badge) return;

    // Remove all state classes
    badge.classList.remove('connected', 'disconnected', 'connecting', 'error');

    if (status === 'connected') {
        badge.textContent = 'Connected';
        badge.classList.add('connected');
        connectBtn.disabled = true;
        disconnectBtn.disabled = false;
    } else if (status === 'connecting') {
        badge.textContent = 'Connecting...';
        badge.classList.add('connecting');
        connectBtn.disabled = true;
        disconnectBtn.disabled = true;
    } else if (status === 'error') {
        badge.textContent = errorMsg ? 'Error: ' + errorMsg : 'Error';
        badge.classList.add('error');
        connectBtn.disabled = false;
        disconnectBtn.disabled = true;
    } else {
        // disconnected (default)
        badge.textContent = 'Disconnected';
        badge.classList.add('disconnected');
        connectBtn.disabled = false;
        disconnectBtn.disabled = true;
    }
}

async function ntripConnect() {
    const caster = document.getElementById('cfg-ntrip-caster').value.trim();
    const port = parseInt(document.getElementById('cfg-ntrip-port').value) || 2101;
    const mountpoint = document.getElementById('cfg-ntrip-mount').value.trim();
    const user = document.getElementById('cfg-ntrip-user').value.trim();
    const password = document.getElementById('cfg-ntrip-pass').value;
    const version = document.getElementById('cfg-ntrip-version').value;
    const https = document.getElementById('cfg-ntrip-https').checked;

    if (!caster || !mountpoint) {
        updateNtripUI('error', 'Caster and mountpoint are required');
        return;
    }

    updateNtripUI('connecting');

    try {
        const resp = await fetch('/api/ntrip/start', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ caster, port, mountpoint, username: user, password, version, https }),
        });
        const data = await resp.json();
        if (!resp.ok) {
            updateNtripUI('error', data.error || 'Connection failed');
        }
        // Success updates arrive via ntrip_status WebSocket event
    } catch (e) {
        updateNtripUI('error', e.message);
    }
}

async function ntripDisconnect() {
    const disconnectBtn = document.getElementById('ntrip-disconnect-btn');
    if (disconnectBtn) disconnectBtn.disabled = true;

    try {
        await fetch('/api/ntrip/stop', { method: 'POST' });
        // UI updates arrive via ntrip_status WebSocket event
    } catch (e) {
        updateNtripUI('error', e.message);
    }
}

async function restoreNtripStatus() {
    // On page load, check if NTRIP is already connected (survives page refresh)
    if (window.APP_MODE !== 'native' || window.HAT_NAME !== 'L1/L5 GNSS RTK HAT') return;

    try {
        const resp = await fetch('/api/ntrip/status');
        if (!resp.ok) return;
        const data = await resp.json();
        if (data.connected) {
            updateNtripUI('connected');
            // Restore form fields from server-side config
            if (data.config) {
                const c = data.config;
                const setVal = (id, val) => { const el = document.getElementById(id); if (el && val !== undefined) el.value = val; };
                setVal('cfg-ntrip-caster', c.caster);
                setVal('cfg-ntrip-port', c.port);
                setVal('cfg-ntrip-mount', c.mountpoint);
                setVal('cfg-ntrip-user', c.username);
                setVal('cfg-ntrip-version', c.version);
                const httpsEl = document.getElementById('cfg-ntrip-https');
                if (httpsEl) httpsEl.checked = !!c.https;
            }
        }
    } catch (e) {
        // Silently ignore — not critical
    }
}


// Initialize tabs on page load
window.addEventListener('DOMContentLoaded', () => {
    setupTabs();
    setupDataTabs();
    setupConfigPanel();
    restoreNtripStatus();

    // Auto-fetch config from the GNSS module/node on startup
    // This restores geofence visualization after page refresh
    if (window.APP_MODE === 'native' || window.APP_MODE === 'ros2') {
        loadConfig(true);
    }
});
