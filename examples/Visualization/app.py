#!/usr/bin/env python3
# Jimmy Paputto 2025
# GPS Visualization Web Application
# Modes: native (GnssHat library) or external_tty (NMEA serial)

import sys
import os
import argparse
import threading
import time

# Ensure GnssHat Python bindings are findable when running as root (sudo)
# The module is installed in the pi user's site-packages
_pi_user_site = '/home/pi/.local/lib/python3.13/site-packages'
if _pi_user_site not in sys.path and os.path.isdir(_pi_user_site):
    sys.path.insert(0, _pi_user_site)

from flask import Flask, render_template, jsonify
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
        1: "GPS Fix",
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
        int(gnsshat.FixQuality.GPS_FIX_2D_3D): "GPS Fix",
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
        int(gnsshat.FixType.DEAD_RECKONING_ONLY): "Dead Reckoning",
        int(gnsshat.FixType.FIX_2D): "2D Fix",
        int(gnsshat.FixType.FIX_3D): "3D Fix",
        int(gnsshat.FixType.GNSS_WITH_DEAD_RECKONING): "GNSS+DR",
        int(gnsshat.FixType.TIME_ONLY_FIX): "Time Only",
    }

    utc_time = "N/A"
    if pvt.utc_time and pvt.utc_time.valid:
        utc_time = f"{pvt.utc_time.hours:02d}:{pvt.utc_time.minutes:02d}:{pvt.utc_time.seconds:02d}"

    date = "N/A"
    if pvt.date and pvt.date.valid:
        date = f"{pvt.date.year:04d}-{pvt.date.month:02d}-{pvt.date.day:02d}"

    return {
        'latitude': float(pvt.latitude),
        'longitude': float(pvt.longitude),
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

            data = {
                'pvt': pvt_data,
                'offset_x': x_offset,
                'offset_y': y_offset,
                'has_reference': gps_state['reference_position'] is not None
            }

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
    return render_template('index.html')


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
