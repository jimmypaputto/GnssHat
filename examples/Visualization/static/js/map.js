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
            return;
        }
        
        this.position = { x, y };
        
        // Add to trail - NEVER clear it (persistent trail)
        this.trail.push({ x, y });
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
        // Reset origin to current position and clear trail
        this.trail = [{ x: this.position.x, y: this.position.y }]; // Start new trail from current pos
        this.originSet = true;
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
        map.resetOrigin();
    });

    // Config progress events (native mode)
    socket.on('config_progress', function(data) {
        handleConfigProgress(data);
    });
}

function setupUIHandlers() {
    const resetBtn = document.getElementById('reset-btn');
    if (resetBtn) {
        resetBtn.addEventListener('click', function() {
            console.log('Reset Origin button clicked');
            socket.emit('reset_reference');
            map.resetOrigin();
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
    if (window.APP_MODE === 'native') {
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
        }

        // RF Blocks
        if (data.rf_blocks) {
            updateRfBlocks(data.rf_blocks);
        }

        // Satellites → sky view
        if (data.satellites) {
            updateSkyView(data.satellites);
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

function updateDataField(id, value) {
    const element = document.getElementById(id);
    if (element) {
        element.textContent = value;
    }
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
}

function updateSkyView(satellites) {
    drawSkyPlot(satellites);
    updateSatTable(satellites);
}

// =======================
// OpenStreetMap Integration
// =======================

let osmMap = null;
let osmMarker = null;
let osmTrail = null;
let osmTrailCoords = [];

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
                activeView.style.display = 'block';
                
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
    if (window.APP_MODE !== 'native') return;

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
    geoEn.addEventListener('change', () => {
        geoDetails.style.display = geoEn.checked ? '' : 'none';
    });

    // Add geofence button
    document.getElementById('cfg-geo-add').addEventListener('click', () => {
        addGeofenceRow();
    });

    // Load config button
    document.getElementById('cfg-load-btn').addEventListener('click', loadConfig);

    // Send config button
    document.getElementById('cfg-send-btn').addEventListener('click', sendConfig);
}

let geoFenceCount = 0;

function addGeofenceRow(lat, lon, radius) {
    if (geoFenceCount >= 4) return;
    geoFenceCount++;

    const container = document.getElementById('cfg-geo-fences');

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
    btn.disabled = geoFenceCount >= 4;
    btn.textContent = geoFenceCount >= 4 ? 'Max 4 geofences' : '+ Add Geofence';
}

function clearGeofenceRows() {
    const container = document.getElementById('cfg-geo-fences');
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
    clearGeofenceRows();
    if (geo && geo.geofences && geo.geofences.length > 0) {
        geoEn.checked = true;
        document.getElementById('cfg-geo-details').style.display = '';
        document.getElementById('cfg-geo-conf').value = geo.confidence_level ?? 3;
        for (const f of geo.geofences) {
            addGeofenceRow(f.lat, f.lon, f.radius);
        }
    } else {
        geoEn.checked = false;
        document.getElementById('cfg-geo-details').style.display = 'none';
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
    if (document.getElementById('cfg-geo-en').checked) {
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
        } else {
            config.geofencing = null;
        }
    } else {
        config.geofencing = null;
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

async function loadConfig() {
    try {
        const resp = await fetch('/api/config');
        if (!resp.ok) {
            const err = await resp.json();
            showConfigStatus('Load failed: ' + (err.error || resp.statusText), true);
            return;
        }
        const config = await resp.json();
        populateFormFromConfig(config);
        showConfigStatus('Configuration loaded from device', false);
    } catch (e) {
        showConfigStatus('Load failed: ' + e.message, true);
    }
}

async function sendConfig() {
    const config = buildConfigFromForm();

    // Basic validation
    if (config.measurement_rate_hz < 1 || config.measurement_rate_hz > 25) {
        showConfigStatus('Measurement rate must be 1-25 Hz', true);
        return;
    }

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
        }
        // Success path handled by socket config_progress events
    } catch (e) {
        handleConfigProgress({ step: 'error', message: 'Network error: ' + e.message });
    }
}


// Initialize tabs on page load
window.addEventListener('DOMContentLoaded', () => {
    setupTabs();
    setupDataTabs();
    setupConfigPanel();
});
