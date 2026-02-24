#!/usr/bin/env python3
# Jimmy Paputto 2025
# GPS Visualization Web Application
# Modes: native (GnssHat library) or external_tty (NMEA serial)

import sys
import os
import argparse
import threading
import time
import json

# Ensure GnssHat Python bindings are findable when running as root (sudo)
# The module is installed in the pi user's site-packages
_pi_user_site = '/home/pi/.local/lib/python3.13/site-packages'
if _pi_user_site not in sys.path and os.path.isdir(_pi_user_site):
    sys.path.insert(0, _pi_user_site)

from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
from geopy.distance import geodesic

app = Flask(__name__)
app.config['SECRET_KEY'] = 'gnss-visualization-secret'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Serial port configuration (external_tty mode)
SERIAL_PORT = '/dev/jimmypaputto/gnss'
BAUD_RATE = 9600

# Operating mode: 'native' or 'external_tty'
RUN_MODE = 'native'

# Global state
gps_state = {
    'serial_port': None,
    'hat': None,           # GnssHat object (native mode)
    'running': False,
    'thread': None,
    'reference_position': None,  # (lat, lon) of starting position
    'current_data': None,
    'last_gga': None,  # Last GGA sentence
    'last_rmc': None,  # Last RMC sentence
    'last_gsa': None,  # Last GSA sentence
    'last_gsv': None,  # Last GSV sentences
    'current_config': None,  # Current GnssHat config dict
    'config_lock': threading.Lock(),  # Lock for config changes
}


def calculate_offset_meters(ref_pos, current_pos):
    """
    Calculate offset in meters from reference position.
    Returns: (x_meters, y_meters) where x is East-West, y is North-South
    """
    if ref_pos is None or current_pos is None:
        return (0.0, 0.0)
    
    ref_lat, ref_lon = ref_pos
    curr_lat, curr_lon = current_pos
    
    # North-South distance (y-axis)
    north_point = (curr_lat, ref_lon)
    y_meters = geodesic(ref_pos, north_point).meters
    if curr_lat < ref_lat:
        y_meters = -y_meters
    
    # East-West distance (x-axis)
    east_point = (ref_lat, curr_lon)
    x_meters = geodesic(ref_pos, east_point).meters
    if curr_lon < ref_lon:
        x_meters = -x_meters
    
    return (x_meters, y_meters)


def parse_nmea_data():
    """Combine data from NMEA sentences to create GPS data dictionary"""
    gga = gps_state['last_gga']
    rmc = gps_state['last_rmc']
    gsa = gps_state['last_gsa']
    
    if not gga and not rmc:
        return None
    
    # Use GGA as primary source, fallback to RMC
    primary = gga if gga else rmc

    latitude = float(primary.latitude) if hasattr(primary, 'latitude') and primary.latitude else 0.0
    longitude = float(primary.longitude) if hasattr(primary, 'longitude') and primary.longitude else 0.0
    
    # Extract altitude (only from GGA)
    altitude_msl = float(gga.altitude) if gga and hasattr(gga, 'altitude') and gga.altitude else 0.0
    
    # Extract speed and heading (from RMC)
    speed_knots = float(rmc.spd_over_grnd) if rmc and hasattr(rmc, 'spd_over_grnd') and rmc.spd_over_grnd else 0.0
    speed_mps = speed_knots * 0.514444  # Convert knots to m/s
    
    heading = float(rmc.true_course) if rmc and hasattr(rmc, 'true_course') and rmc.true_course else 0.0
    
    # Extract fix quality (from GGA)
    gps_qual = int(gga.gps_qual) if gga and hasattr(gga, 'gps_qual') else 0
    fix_quality_map = {
        0: "Invalid",
        1: "2D3DFix",
        2: "DGPS Fix",
        4: "RTK Fixed",
        5: "RTK Float",
        6: "Dead Reckoning"
    }
    fix_quality = fix_quality_map.get(gps_qual, "Unknown")
    
    # Extract number of satellites (from GGA)
    num_sats = int(gga.num_sats) if gga and hasattr(gga, 'num_sats') and gga.num_sats else 0
    
    # Extract HDOP (from GGA or GSA)
    hdop = float(gga.horizontal_dil) if gga and hasattr(gga, 'horizontal_dil') and gga.horizontal_dil else 0.0
    
    # Extract fix type from GSA (2D/3D)
    fix_type = "No Fix"
    if gsa and hasattr(gsa, 'mode_fix_type'):
        fix_type_map = {
            '1': 'No Fix',
            '2': '2D Fix',
            '3': '3D Fix'
        }
        fix_type = fix_type_map.get(str(gsa.mode_fix_type), "Unknown")
    
    # Extract time (from GGA or RMC)
    utc_time = "N/A"
    if primary and hasattr(primary, 'timestamp') and primary.timestamp:
        utc_time = primary.timestamp.strftime("%H:%M:%S")
    
    # Extract date (from RMC)
    date = "N/A"
    if rmc and hasattr(rmc, 'datestamp') and rmc.datestamp:
        date = rmc.datestamp.strftime("%Y-%m-%d")
    
    # Status (A=Active, V=Void from RMC)
    fix_status = "Void"
    if rmc and hasattr(rmc, 'status') and rmc.status == 'A':
        fix_status = "Active"
    elif gga and gps_qual > 0:
        fix_status = "Active"
    
    return {
        'latitude': float(latitude),
        'longitude': float(longitude),
        'altitude_msl': float(altitude_msl),
        'speed_over_ground': float(speed_mps),
        'heading': float(heading),
        'visible_satellites': int(num_sats),
        'hdop': float(hdop),
        'fix_quality': fix_quality,
        'fix_status': fix_status,
        'fix_type': fix_type,
        'utc_time': utc_time,
        'date': date,
    }


# ─── Native mode: GnssHat reader thread ─────────────────────────────────────

def create_default_config():
    """Create default GnssHat configuration"""
    from jimmypaputto import gnsshat
    return {
        'measurement_rate_hz': 1,
        'dynamic_model': gnsshat.DynamicModel.STATIONARY,
        'timepulse_pin_config': {
            'active': True,
            'fixed_pulse': {
                'frequency': 1,
                'pulse_width': 0.1
            },
            'polarity': gnsshat.TimepulsePolarity.RISING_EDGE
        },
        'geofencing': None
    }


def nav_to_pvt_data(nav):
    """Convert Navigation object from GnssHat to pvt data dict for the frontend"""
    from jimmypaputto import gnsshat

    pvt = nav.pvt
    dop = nav.dop

    fix_quality_map = {
        int(gnsshat.FixQuality.INVALID): "Invalid",
        int(gnsshat.FixQuality.GPS_FIX_2D_3D): "Gps Fix 2D3D",
        int(gnsshat.FixQuality.DGNSS): "DGPS Fix",
        int(gnsshat.FixQuality.PPS_FIX): "PPS Fix",
        int(gnsshat.FixQuality.FIXED_RTK): "RTK Fixed",
        int(gnsshat.FixQuality.FLOAT_RTK): "RTK Float",
        int(gnsshat.FixQuality.DEAD_RECKONING): "Dead Reckoning",
    }

    fix_status_map = {
        int(gnsshat.FixStatus.VOID): "Void",
        int(gnsshat.FixStatus.ACTIVE): "Active",
    }

    fix_type_map = {
        int(gnsshat.FixType.NO_FIX): "No Fix",
        int(gnsshat.FixType.DEAD_RECKONING_ONLY): "Dead Reckoning Only",
        int(gnsshat.FixType.FIX_2D): "Fix 2D",
        int(gnsshat.FixType.FIX_3D): "Fix 3D",
        int(gnsshat.FixType.GNSS_WITH_DEAD_RECKONING): "GNSS+DR",
        int(gnsshat.FixType.TIME_ONLY_FIX): "Time Only Fix",
    }

    utc_time = "N/A"
    if pvt.utc_time and pvt.utc_time.valid:
        utc_time = f"{pvt.utc_time.hours:02d}:{pvt.utc_time.minutes:02d}:{pvt.utc_time.seconds:02d}"

    date = "N/A"
    if pvt.date and pvt.date.valid:
        date = f"{pvt.date.year:04d}-{pvt.date.month:02d}-{pvt.date.day:02d}"

    pvt_data = {
        'latitude': float(pvt.latitude),
        'longitude': float(pvt.longitude),
        'altitude': float(pvt.altitude),
        'altitude_msl': float(pvt.altitude_msl),
        'speed_over_ground': float(pvt.speed_over_ground),
        'heading': float(pvt.heading),
        'visible_satellites': int(pvt.visible_satellites),
        'hdop': float(dop.horizontal),
        'fix_quality': fix_quality_map.get(pvt.fix_quality, "Unknown"),
        'fix_status': fix_status_map.get(pvt.fix_status, "Void"),
        'fix_type': fix_type_map.get(pvt.fix_type, "Unknown"),
        'utc_time': utc_time,
        'date': date,
        'horizontal_accuracy': float(pvt.horizontal_accuracy),
        'vertical_accuracy': float(pvt.vertical_accuracy),
        'speed_accuracy': float(pvt.speed_accuracy),
        'heading_accuracy': float(pvt.heading_accuracy),
    }

    return pvt_data


def nav_to_full_data(nav):
    """Serialize full Navigation object (DOP, Geofencing, RF Blocks) for native mode"""
    from jimmypaputto import gnsshat

    dop = nav.dop
    dop_data = {
        'geometric': float(dop.geometric),
        'position': float(dop.position),
        'time': float(dop.time),
        'vertical': float(dop.vertical),
        'horizontal': float(dop.horizontal),
        'northing': float(dop.northing),
        'easting': float(dop.easting),
    }

    geofencing_nav = nav.geofencing.nav
    geofencing_status_map = {
        int(gnsshat.GeofencingStatus.NOT_AVAILABLE): "Not Available",
        int(gnsshat.GeofencingStatus.ACTIVE): "Active",
    }
    geofencing_data = {
        'status': geofencing_status_map.get(geofencing_nav.status, "Unknown"),
        'number_of_geofences': int(geofencing_nav.number_of_geofences),
    }

    jamming_map = {
        int(gnsshat.JammingState.UNKNOWN): "Unknown",
        int(gnsshat.JammingState.OK_NO_SIGNIFICANT_JAMMING): "OK",
        int(gnsshat.JammingState.WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK): "Warning",
        int(gnsshat.JammingState.CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX): "Critical",
    }
    antenna_status_map = {
        int(gnsshat.AntennaStatus.INIT): "Init",
        int(gnsshat.AntennaStatus.DONT_KNOW): "Unknown",
        int(gnsshat.AntennaStatus.OK): "OK",
        int(gnsshat.AntennaStatus.SHORT): "Short",
        int(gnsshat.AntennaStatus.OPEN): "Open",
    }
    antenna_power_map = {
        int(gnsshat.AntennaPower.OFF): "Off",
        int(gnsshat.AntennaPower.ON): "On",
        int(gnsshat.AntennaPower.DONT_KNOW): "Unknown",
    }
    band_map = {
        int(gnsshat.RfBand.L1): "L1",
        int(gnsshat.RfBand.L2_OR_L5): "L2/L5",
    }

    rf_blocks_data = []
    for rf in nav.rf_blocks:
        rf_blocks_data.append({
            'band': band_map.get(rf.id, "Unknown"),
            'jamming_state': jamming_map.get(rf.jamming_state, "Unknown"),
            'antenna_status': antenna_status_map.get(rf.antenna_status, "Unknown"),
            'antenna_power': antenna_power_map.get(rf.antenna_power, "Unknown"),
            'noise_per_ms': int(rf.noise_per_ms),
            'agc_monitor': float(rf.agc_monitor),
            'cw_suppression': float(rf.cw_interference_suppression_level),
        })

    gnss_id_map = {
        int(gnsshat.GnssId.GPS): "GPS",
        int(gnsshat.GnssId.SBAS): "SBAS",
        int(gnsshat.GnssId.GALILEO): "Galileo",
        int(gnsshat.GnssId.BEIDOU): "BeiDou",
        int(gnsshat.GnssId.IMES): "IMES",
        int(gnsshat.GnssId.QZSS): "QZSS",
        int(gnsshat.GnssId.GLONASS): "GLONASS",
    }
    quality_map = {
        int(gnsshat.SvQuality.NO_SIGNAL): "No Signal",
        int(gnsshat.SvQuality.SEARCHING): "Searching",
        int(gnsshat.SvQuality.SIGNAL_ACQUIRED): "Acquired",
        int(gnsshat.SvQuality.SIGNAL_DETECTED_BUT_UNUSABLE): "Unusable",
        int(gnsshat.SvQuality.CODE_LOCKED_AND_TIME_SYNCHRONIZED): "Code Locked",
        int(gnsshat.SvQuality.CODE_AND_CARRIER_LOCKED_1): "Carrier Lock 1",
        int(gnsshat.SvQuality.CODE_AND_CARRIER_LOCKED_2): "Carrier Lock 2",
        int(gnsshat.SvQuality.CODE_AND_CARRIER_LOCKED_3): "Carrier Lock 3",
    }

    satellites_data = []
    for sat in nav.satellites:
        satellites_data.append({
            'gnss_id': gnss_id_map.get(sat.gnss_id, "Unknown"),
            'sv_id': int(sat.sv_id),
            'cno': int(sat.cno),
            'elevation': int(sat.elevation),
            'azimuth': int(sat.azimuth),
            'quality': quality_map.get(sat.quality, "Unknown"),
            'used_in_fix': bool(sat.used_in_fix),
            'healthy': bool(sat.healthy),
        })

    return {
        'dop': dop_data,
        'geofencing': geofencing_data,
        'rf_blocks': rf_blocks_data,
        'satellites': satellites_data,
    }


def native_reader_thread():
    """Background thread that reads navigation data from GnssHat (blocking)"""
    print("Native GnssHat reader thread started")

    hat = gps_state['hat']
    while gps_state['running']:
        try:
            nav = hat.wait_and_get_fresh_navigation()
            pvt_data = nav_to_pvt_data(nav)

            if not pvt_data:
                continue

            # Set reference position on first valid position (non-zero lat/lon)
            if gps_state['reference_position'] is None:
                lat, lon = pvt_data['latitude'], pvt_data['longitude']
                if lat != 0.0 and lon != 0.0:
                    gps_state['reference_position'] = (lat, lon)
                    print(f"Reference position set: {gps_state['reference_position']}")

            # Calculate offset from reference
            current_pos = (pvt_data['latitude'], pvt_data['longitude'])
            x_offset, y_offset = calculate_offset_meters(
                gps_state['reference_position'],
                current_pos
            )

            data = {
                'pvt': pvt_data,
                'offset_x': x_offset,
                'offset_y': y_offset,
                'has_reference': gps_state['reference_position'] is not None
            }

            # Add full navigation data (DOP, Geofencing, RF) for native mode
            try:
                extra = nav_to_full_data(nav)
                data.update(extra)
            except Exception as e:
                print(f"Error serializing extra nav data: {e}")

            gps_state['current_data'] = data
            socketio.emit('gps_update', data, namespace='/')

        except Exception as e:
            print(f"Error in native reader thread: {e}")
            if gps_state['running']:
                time.sleep(1)

    print("Native GnssHat reader thread stopped")


# ─── External TTY mode: NMEA serial reader thread ───────────────────────────

def gps_reader_thread():
    """Background thread that reads NMEA sentences from serial port"""
    import serial
    import pynmea2

    print("GPS NMEA reader thread started")
    
    while gps_state['running']:
        try:
            if not gps_state['serial_port'] or not gps_state['serial_port'].is_open:
                print("Serial port not open, waiting...")
                time.sleep(1)
                continue
            
            # Read line from serial port
            line = gps_state['serial_port'].readline()
            
            if not line:
                continue
            
            try:
                # Decode and parse NMEA sentence
                line_str = line.decode('ascii', errors='ignore').strip()
                
                if not line_str.startswith('$'):
                    continue
                
                msg = pynmea2.parse(line_str)
                
                # Store sentences by type
                if isinstance(msg, pynmea2.types.talker.GGA):
                    gps_state['last_gga'] = msg
                elif isinstance(msg, pynmea2.types.talker.RMC):
                    gps_state['last_rmc'] = msg
                elif isinstance(msg, pynmea2.types.talker.GSA):
                    gps_state['last_gsa'] = msg
                elif isinstance(msg, pynmea2.types.talker.GSV):
                    gps_state['last_gsv'] = msg
                
                # Parse combined data
                pvt_data = parse_nmea_data()
                
                if not pvt_data:
                    continue
                
                # Set reference position on first valid fix
                if gps_state['reference_position'] is None:
                    if pvt_data['fix_status'] == 'Active':
                        gps_state['reference_position'] = (
                            pvt_data['latitude'],
                            pvt_data['longitude']
                        )
                        print(f"Reference position set: {gps_state['reference_position']}")
                
                # Calculate offset from reference
                current_pos = (pvt_data['latitude'], pvt_data['longitude'])
                x_offset, y_offset = calculate_offset_meters(
                    gps_state['reference_position'],
                    current_pos
                )
                
                # Prepare data for transmission
                data = {
                    'pvt': pvt_data,
                    'offset_x': x_offset,
                    'offset_y': y_offset,
                    'has_reference': gps_state['reference_position'] is not None
                }
                
                gps_state['current_data'] = data
                
                # Emit to all connected clients (throttle to ~1Hz for UI updates)
                if isinstance(msg, pynmea2.types.talker.GGA):  # Send on GGA (usually 1Hz)
                    socketio.emit('gps_update', data, namespace='/')
                
            except pynmea2.ParseError as e:
                # Ignore parse errors (incomplete/corrupted sentences)
                pass
            except Exception as e:
                print(f"Error parsing NMEA: {e}")
            
        except serial.SerialException as e:
            print(f"Serial error: {e}")
            if gps_state['running']:
                time.sleep(1)
        except Exception as e:
            print(f"Error in GPS reader thread: {e}")
            if gps_state['running']:
                time.sleep(1)
    
    print("GPS NMEA reader thread stopped")


@app.route('/')
def index():
    """Serve main page"""
    return render_template('index.html', mode=RUN_MODE)


@app.route('/api/status')
def api_status():
    """Get current GPS status"""
    return jsonify({
        'running': gps_state['running'],
        'has_reference': gps_state['reference_position'] is not None,
        'reference_position': gps_state['reference_position']
    })


@app.route('/api/current')
def api_current():
    """Get current GPS data"""
    if gps_state['current_data']:
        return jsonify(gps_state['current_data'])
    return jsonify({'error': 'No data available'}), 404


@socketio.on('connect')
def handle_connect():
    """Handle client connection"""
    print(f"Client connected")
    emit('connection_response', {'status': 'connected'})
    
    # Send current data if available
    if gps_state['current_data']:
        emit('gps_update', gps_state['current_data'])


@socketio.on('disconnect')
def handle_disconnect():
    """Handle client disconnection"""
    print(f"Client disconnected")


@socketio.on('reset_reference')
def handle_reset_reference():
    """Reset reference position to current location"""
    if gps_state['current_data'] and gps_state['current_data']['pvt']:
        pvt = gps_state['current_data']['pvt']
        gps_state['reference_position'] = (pvt['latitude'], pvt['longitude'])
        print(f"Reference position reset to: {gps_state['reference_position']}")
        emit('reference_reset', {'position': gps_state['reference_position']}, broadcast=True)


# ─── Configuration API (native mode only) ───────────────────────────────────

def config_to_json_safe(config):
    """Convert config dict (with IntEnum values) to plain JSON-serializable dict"""
    def convert(obj):
        if isinstance(obj, dict):
            return {k: convert(v) for k, v in obj.items()}
        elif isinstance(obj, (list, tuple)):
            return [convert(i) for i in obj]
        elif isinstance(obj, int):
            return int(obj)
        elif isinstance(obj, float):
            return float(obj)
        elif isinstance(obj, bool):
            return bool(obj)
        elif obj is None:
            return None
        else:
            return obj
    return convert(config)


def json_to_native_config(data):
    """Convert JSON config from frontend to config dict with proper IntEnum types"""
    from jimmypaputto import gnsshat

    config = {
        'measurement_rate_hz': int(data['measurement_rate_hz']),
        'dynamic_model': int(data['dynamic_model']),
    }

    # Timepulse
    tp = data.get('timepulse_pin_config')
    if tp and tp.get('active'):
        config['timepulse_pin_config'] = {
            'active': True,
            'fixed_pulse': {
                'frequency': int(tp['fixed_pulse']['frequency']),
                'pulse_width': float(tp['fixed_pulse']['pulse_width']),
            },
            'polarity': int(tp.get('polarity', 1)),
        }
        if tp.get('pulse_when_no_fix') and tp['pulse_when_no_fix'].get('frequency') is not None:
            config['timepulse_pin_config']['pulse_when_no_fix'] = {
                'frequency': int(tp['pulse_when_no_fix']['frequency']),
                'pulse_width': float(tp['pulse_when_no_fix']['pulse_width']),
            }
    else:
        config['timepulse_pin_config'] = None

    # Geofencing
    geo = data.get('geofencing')
    if geo and geo.get('geofences') and len(geo['geofences']) > 0:
        fences = []
        for f in geo['geofences'][:4]:
            if f.get('lat') is not None and f.get('lon') is not None and f.get('radius') is not None:
                fences.append({
                    'lat': float(f['lat']),
                    'lon': float(f['lon']),
                    'radius': float(f['radius']),
                })
        if fences:
            config['geofencing'] = {
                'geofences': fences,
                'confidence_level': int(geo.get('confidence_level', 3)),
            }
        else:
            config['geofencing'] = None
    else:
        config['geofencing'] = None

    return config


@app.route('/api/config', methods=['GET'])
def api_get_config():
    """Get current GnssHat configuration"""
    if RUN_MODE != 'native':
        return jsonify({'error': 'Configuration only available in native mode'}), 400
    if gps_state['current_config']:
        return jsonify(config_to_json_safe(gps_state['current_config']))
    return jsonify({'error': 'No config loaded'}), 404


@app.route('/api/config', methods=['POST'])
def api_set_config():
    """Apply new GnssHat configuration — stops reader, destroys hat, creates new one"""
    if RUN_MODE != 'native':
        return jsonify({'error': 'Configuration only available in native mode'}), 400

    data = request.get_json()
    if not data:
        return jsonify({'error': 'No JSON body'}), 400

    with gps_state['config_lock']:
        try:
            # 1. Stop reader thread
            socketio.emit('config_progress', {'step': 'stop', 'message': 'Stopping reader...'}, namespace='/')
            gps_state['running'] = False
            if gps_state['thread']:
                gps_state['thread'].join(timeout=5)
                gps_state['thread'] = None

            # 2. Destroy old hat
            socketio.emit('config_progress', {'step': 'destroy', 'message': 'Destroying old GnssHat object...'}, namespace='/')
            if gps_state['hat']:
                del gps_state['hat']
                gps_state['hat'] = None
            time.sleep(1)

            # 3. Create new hat
            socketio.emit('config_progress', {'step': 'create', 'message': 'Creating new GnssHat object...'}, namespace='/')
            from jimmypaputto import gnsshat
            hat = gnsshat.GnssHat()

            # 4. Soft hot reset — preserves almanac/ephemeris for fast re-lock
            socketio.emit('config_progress', {'step': 'reset', 'message': 'Resetting module (hot start)...'}, namespace='/')
            hat.soft_reset_hot_start()
            time.sleep(1)

            # 5. Convert and apply config
            socketio.emit('config_progress', {'step': 'config', 'message': 'Applying configuration...'}, namespace='/')
            config = json_to_native_config(data)

            if not hat.start(config):
                del hat
                socketio.emit('config_progress', {'step': 'error', 'message': 'hat.start() failed!'}, namespace='/')
                # Try to restart with old config
                try:
                    start_gps_native()
                except Exception:
                    pass
                return jsonify({'error': 'hat.start() returned False — configuration rejected by module'}), 500

            # 6. Success — save state and restart reader
            socketio.emit('config_progress', {'step': 'reader', 'message': 'Starting GNSS reader thread...'}, namespace='/')
            gps_state['hat'] = hat
            gps_state['current_config'] = config
            gps_state['reference_position'] = None  # Reset so map re-calibrates
            gps_state['running'] = True
            gps_state['thread'] = threading.Thread(target=native_reader_thread, daemon=True)
            gps_state['thread'].start()

            socketio.emit('config_progress', {'step': 'done', 'message': 'Configuration applied successfully!'}, namespace='/')
            return jsonify({'success': True, 'config': config_to_json_safe(config)})

        except Exception as e:
            socketio.emit('config_progress', {'step': 'error', 'message': f'Error: {str(e)}'}, namespace='/')
            # Try to restart with previous config using hot start
            try:
                if not gps_state['hat'] and not gps_state['running']:
                    from jimmypaputto import gnsshat as gs
                    hat = gs.GnssHat()
                    hat.soft_reset_hot_start()
                    time.sleep(1)
                    old_cfg = gps_state['current_config'] or create_default_config()
                    if hat.start(old_cfg):
                        gps_state['hat'] = hat
                        gps_state['running'] = True
                        gps_state['thread'] = threading.Thread(target=native_reader_thread, daemon=True)
                        gps_state['thread'].start()
                        print("Recovery: restarted with previous config")
                    else:
                        del hat
                        print("Recovery: failed to restart")
            except Exception as recovery_err:
                print(f"Recovery failed: {recovery_err}")
            return jsonify({'error': str(e)}), 500


def start_gps():
    """Initialize and start GPS data source based on RUN_MODE"""
    if RUN_MODE == 'native':
        return start_gps_native()
    else:
        return start_gps_external_tty()


def start_gps_native():
    """Initialize GnssHat and start native reader thread"""
    from jimmypaputto import gnsshat

    print("Starting GnssHat in native mode...")
    try:
        hat = gnsshat.GnssHat()
        hat.soft_reset_hot_start()
        config = create_default_config()
        if not hat.start(config):
            print("Failed to start GnssHat")
            return False

        print("GnssHat started successfully!")
        gps_state['hat'] = hat
        gps_state['current_config'] = config
        gps_state['running'] = True
        gps_state['thread'] = threading.Thread(target=native_reader_thread, daemon=True)
        gps_state['thread'].start()
        return True

    except Exception as e:
        print(f"Error starting GnssHat: {e}")
        return False


def start_gps_external_tty():
    """Initialize and start GPS serial port reading (NMEA)"""
    import serial

    print(f"Opening serial port: {SERIAL_PORT} at {BAUD_RATE} baud...")
    
    try:
        gps_state['serial_port'] = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            timeout=1.0,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )
        
        if not gps_state['serial_port'].is_open:
            print("Failed to open serial port")
            return False
        
        print(f"Serial port {SERIAL_PORT} opened successfully!")
        
        gps_state['running'] = True
        gps_state['thread'] = threading.Thread(target=gps_reader_thread, daemon=True)
        gps_state['thread'].start()
        
        return True
        
    except Exception as e:
        print(f"Error opening serial port: {e}")
        return False


def stop_gps():
    """Stop GPS data source"""
    print("Stopping GPS...")
    gps_state['running'] = False
    
    if gps_state['thread']:
        gps_state['thread'].join(timeout=5)
    
    if gps_state['serial_port'] and gps_state['serial_port'].is_open:
        gps_state['serial_port'].close()
    
    gps_state['serial_port'] = None
    gps_state['hat'] = None
    gps_state['reference_position'] = None
    gps_state['current_data'] = None
    gps_state['last_gga'] = None
    gps_state['last_rmc'] = None
    gps_state['last_gsa'] = None
    gps_state['last_gsv'] = None
    
    print("GPS stopped")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='GPS Visualization Server')
    parser.add_argument(
        'mode',
        choices=['native', 'external_tty'],
        nargs='?',
        default='native',
        help='Data source mode: native (GnssHat library) or external_tty (NMEA serial). Default: native'
    )
    args = parser.parse_args()
    RUN_MODE = args.mode

    print("=" * 60)
    print("GPS Visualization Server - Jimmy Paputto 2025")
    print(f"Mode: {RUN_MODE}")
    if RUN_MODE == 'external_tty':
        print(f"Reading from: {SERIAL_PORT} @ {BAUD_RATE} baud")
    else:
        print("Using GnssHat native library")
    print("=" * 60)
    
    if not start_gps():
        print("Failed to initialize GPS. Exiting.")
        sys.exit(1)
    
    try:
        print("\nStarting web server...")
        print("Access the visualization at:")
        print("  - From this device: http://localhost:5000")
        print("  - From network: http://<raspberry-pi-ip>:5000")
        print("\nPress Ctrl+C to stop")
        print("=" * 60)
        
        socketio.run(app, host='0.0.0.0', port=5000, debug=False)
        
    except KeyboardInterrupt:
        print("\n\nShutdown requested...")
    finally:
        stop_gps()
        print("Server stopped. Goodbye!")
