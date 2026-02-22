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
            }
        });
    });
}

// Initialize tabs on page load
window.addEventListener('DOMContentLoaded', () => {
    setupTabs();
});
