/*
 * Jimmy Paputto 2025
 */

#include <Python.h>
#include <structmember.h>
#include <stdio.h>

#include "GnssHat.h"


static PyTypeObject GnssHatType;
static PyTypeObject PositionVelocityTimeType;
static PyTypeObject NavigationType;
static PyTypeObject DilutionOverPrecisionType;
static PyTypeObject GeofencingType;
static PyTypeObject GeofencingCfgType;
static PyTypeObject GeofencingNavType;
static PyTypeObject GeofenceType;
static PyTypeObject RfBlockType;
static PyTypeObject SatelliteInfoType;
static PyTypeObject PulseType;
static PyTypeObject TimepulsePinConfigType;
static PyTypeObject UtcTimeType;
static PyTypeObject DateType;

typedef struct
{
    PyObject_HEAD
    jp_gnss_hat_t* hat;
    PyObject* callback;
} GnssHat;

typedef struct
{
    PyObject_HEAD
    double latitude;
    double longitude;
    float altitude;
    float altitude_msl;
    float speed_over_ground;
    float speed_accuracy;
    float heading;
    float heading_accuracy;
    uint8_t visible_satellites;
    float horizontal_accuracy;
    float vertical_accuracy;
    int fix_quality;
    int fix_status;
    int fix_type;
    PyObject* utc_time;   /* UtcTime object */
    PyObject* date;       /* Date object    */
} PositionVelocityTime;

typedef struct
{
    PyObject_HEAD
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    PyObject* valid;      /* bool */
    int32_t accuracy;
} UtcTime;

typedef struct
{
    PyObject_HEAD
    uint8_t day;
    uint8_t month;
    uint16_t year;
    PyObject* valid;      /* bool */
} Date;

typedef struct
{
    PyObject_HEAD
    float geometric;
    float position;
    float time;
    float vertical;
    float horizontal;
    float northing;
    float easting;
} DilutionOverPrecision;

typedef struct
{
    PyObject_HEAD
    PyObject* dop;
    PyObject* pvt;
    PyObject* geofencing;
    PyObject* rf_blocks;
    PyObject* satellites;
} Navigation;

typedef struct
{
    PyObject_HEAD
    float lat;
    float lon;
    float radius;
} Geofence;

typedef struct
{
    PyObject_HEAD
    int id;
    int jamming_state;
    int antenna_status;
    int antenna_power;
    uint32_t post_status;
    uint16_t noise_per_ms;
    float agc_monitor;
    float cw_interference_suppression_level;
    int8_t ofs_i;
    uint8_t mag_i;
    int8_t ofs_q;
    uint8_t mag_q;
} RfBlock;

typedef struct
{
    PyObject_HEAD
    uint8_t confidence_level;
    PyObject* geofences;
} GeofencingCfg;

typedef struct
{
    PyObject_HEAD
    uint32_t iTOW;
    int status;
    uint8_t number_of_geofences;
    int combined_state;
    PyObject* geofences;
} GeofencingNav;

typedef struct
{
    PyObject_HEAD
    PyObject* cfg;
    PyObject* nav;
} Geofencing;

typedef struct
{
    PyObject_HEAD
    int gnss_id;
    uint8_t sv_id;
    uint8_t cno;
    int8_t elevation;
    int16_t azimuth;
    int quality;
    PyObject* used_in_fix;
    PyObject* healthy;
    PyObject* diff_corr;
    PyObject* eph_avail;
    PyObject* alm_avail;
} SatelliteInfo;

typedef struct
{
    PyObject_HEAD
    uint32_t frequency;
    float pulse_width;
} Pulse;

typedef struct
{
    PyObject_HEAD
    PyObject* active;
    Pulse* fixed_pulse;
    Pulse* pulse_when_no_fix;
    long polarity;
} TimepulsePinConfig;

typedef struct
{
    PyObject_HEAD
    uint16_t measurement_rate_hz;
    PyObject* dynamic_model;
    PyObject* timepulse_config;
    PyObject* geofences;
} GnssConfig;

/* ---- UtcTime ---- */

static PyObject* UtcTime_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    UtcTime* self = (UtcTime*)type->tp_alloc(type, 0);
    if (self)
    {
        self->hours = 0;
        self->minutes = 0;
        self->seconds = 0;
        self->valid = Py_False;
        Py_INCREF(Py_False);
        self->accuracy = 0;
    }
    return (PyObject*)self;
}

static void UtcTime_dealloc(UtcTime* self)
{
    Py_XDECREF(self->valid);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* UtcTime_str(UtcTime* self)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
        "%02d:%02d:%02d (valid=%s, accuracy=%d ns)",
        self->hours, self->minutes, self->seconds,
        PyObject_IsTrue(self->valid) ? "True" : "False",
        self->accuracy);
    return PyUnicode_FromString(buffer);
}

static PyMemberDef UtcTime_members[] = {
    {"hours",    T_UBYTE, offsetof(UtcTime, hours),    0, "Hours (0-23)"},
    {"minutes",  T_UBYTE, offsetof(UtcTime, minutes),  0, "Minutes (0-59)"},
    {"seconds",  T_UBYTE, offsetof(UtcTime, seconds),  0, "Seconds (0-59)"},
    {"valid",    T_OBJECT_EX, offsetof(UtcTime, valid), 0, "Time validity flag"},
    {"accuracy", T_INT,   offsetof(UtcTime, accuracy), 0, "Time accuracy in ns"},
    {NULL}
};

static PyTypeObject UtcTimeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.UtcTime",
    .tp_doc = "UTC time",
    .tp_basicsize = sizeof(UtcTime),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = UtcTime_new,
    .tp_dealloc = (destructor)UtcTime_dealloc,
    .tp_str = (reprfunc)UtcTime_str,
    .tp_repr = (reprfunc)UtcTime_str,
    .tp_members = UtcTime_members,
};

/* ---- Date ---- */

static PyObject* Date_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    Date* self = (Date*)type->tp_alloc(type, 0);
    if (self)
    {
        self->day = 0;
        self->month = 0;
        self->year = 0;
        self->valid = Py_False;
        Py_INCREF(Py_False);
    }
    return (PyObject*)self;
}

static void Date_dealloc(Date* self)
{
    Py_XDECREF(self->valid);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Date_str(Date* self)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d (valid=%s)",
        self->year, self->month, self->day,
        PyObject_IsTrue(self->valid) ? "True" : "False");
    return PyUnicode_FromString(buffer);
}

static PyMemberDef Date_members[] = {
    {"day",   T_UBYTE,  offsetof(Date, day),   0, "Day of month (1-31)"},
    {"month", T_UBYTE,  offsetof(Date, month), 0, "Month (1-12)"},
    {"year",  T_USHORT, offsetof(Date, year),  0, "Year"},
    {"valid", T_OBJECT_EX, offsetof(Date, valid), 0, "Date validity flag"},
    {NULL}
};

static PyTypeObject DateType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.Date",
    .tp_doc = "Date",
    .tp_basicsize = sizeof(Date),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = Date_new,
    .tp_dealloc = (destructor)Date_dealloc,
    .tp_str = (reprfunc)Date_str,
    .tp_repr = (reprfunc)Date_str,
    .tp_members = Date_members,
};

static PyObject* Geofence_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    Geofence* self = (Geofence*)type->tp_alloc(type, 0);
    if (self)
    {
        self->lat = 0.0;
        self->lon = 0.0;
        self->radius = 0.0;
    }
    return (PyObject*)self;
}

static PyMemberDef Geofence_members[] = {
    {
        "lat",
        T_FLOAT,
        offsetof(Geofence, lat),
        0,
        "Latitude in degrees"
    },
    {
        "lon",
        T_FLOAT,
        offsetof(Geofence, lon),
        0,
        "Longitude in degrees"
    },
    {
        "radius",
        T_FLOAT,
        offsetof(Geofence, radius),
        0,
        "Radius in meters"
    },
    {NULL}
};

static PyTypeObject GeofenceType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.Geofence",
    .tp_doc = "Geofence configuration",
    .tp_basicsize = sizeof(Geofence),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Geofence_new,
    .tp_members = Geofence_members,
};

static PyObject* RfBlock_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    RfBlock* self = (RfBlock*)type->tp_alloc(type, 0);
    if (self)
    {
        self->id = 0;
        self->jamming_state = 0;
        self->antenna_status = 0;
        self->antenna_power = 0;
        self->post_status = 0;
        self->noise_per_ms = 0;
        self->agc_monitor = 0.0f;
        self->cw_interference_suppression_level = 0.0f;
        self->ofs_i = 0;
        self->mag_i = 0;
        self->ofs_q = 0;
        self->mag_q = 0;
    }
    return (PyObject*)self;
}

static PyMemberDef RfBlock_members[] = {
    {
        "id",
        T_INT,
        offsetof(RfBlock, id),
        0,
        "RF band ID"
    },
    {
        "jamming_state",
        T_INT,
        offsetof(RfBlock, jamming_state),
        0,
        "Jamming state"
    },
    {
        "antenna_status",
        T_INT,
        offsetof(RfBlock, antenna_status),
        0,
        "Antenna status"
    },
    {
        "antenna_power",
        T_INT,
        offsetof(RfBlock, antenna_power),
        0,
        "Antenna power"
    },
    {
        "post_status",
        T_UINT,
        offsetof(RfBlock, post_status),
        0,
        "POST status"
    },
    {
        "noise_per_ms",
        T_USHORT,
        offsetof(RfBlock, noise_per_ms),
        0,
        "Noise level per millisecond"
    },
    {
        "agc_monitor",
        T_FLOAT,
        offsetof(RfBlock, agc_monitor),
        0,
        "AGC monitor percentage"
    },
    {
        "cw_interference_suppression_level",
        T_FLOAT,
        offsetof(RfBlock, cw_interference_suppression_level),
        0,
        "CW interference suppression level"
    },
    {
        "ofs_i",
        T_BYTE,
        offsetof(RfBlock, ofs_i),
        0,
        "I offset"
    },
    {
        "mag_i",
        T_UBYTE,
        offsetof(RfBlock, mag_i),
        0,
        "I magnitude"
    },
    {
        "ofs_q",
        T_BYTE,
        offsetof(RfBlock, ofs_q),
        0,
        "Q offset"
    },
    {
        "mag_q",
        T_UBYTE,
        offsetof(RfBlock, mag_q),
        0,
        "Q magnitude"
    },
    {NULL}
};

static PyObject* RfBlock_str(RfBlock* self)
{
    char buffer[1024];
    snprintf(
        buffer,
        sizeof(buffer),
        "RfBlock(\n"
        "    id=%d\n"
        "    jamming_state=%d\n"
        "    antenna_status=%d\n"
        "    antenna_power=%d\n"
        "    post_status=%u\n"
        "    noise_per_ms=%u\n"
        "    agc_monitor=%.2f%%\n"
        "    cw_interference_suppression_level=%.2f\n"
        "    ofs_i=%d  mag_i=%u\n"
        "    ofs_q=%d  mag_q=%u\n"
        ")",
        self->id,
        self->jamming_state,
        self->antenna_status,
        self->antenna_power,
        self->post_status,
        self->noise_per_ms,
        self->agc_monitor,
        self->cw_interference_suppression_level,
        self->ofs_i,
        self->mag_i,
        self->ofs_q,
        self->mag_q
    );
    return PyUnicode_FromString(buffer);
}

static PyTypeObject RfBlockType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.RfBlock",
    .tp_doc = "RF Block information for jamming detection",
    .tp_basicsize = sizeof(RfBlock),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = RfBlock_new,
    .tp_str = (reprfunc)RfBlock_str,
    .tp_repr = (reprfunc)RfBlock_str,
    .tp_members = RfBlock_members,
};

/* ---- SatelliteInfo ---- */

static PyObject* SatelliteInfo_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    SatelliteInfo* self = (SatelliteInfo*)type->tp_alloc(type, 0);
    if (self)
    {
        self->gnss_id = 0;
        self->sv_id = 0;
        self->cno = 0;
        self->elevation = 0;
        self->azimuth = 0;
        self->quality = 0;
        self->used_in_fix = Py_False;
        Py_INCREF(Py_False);
        self->healthy = Py_False;
        Py_INCREF(Py_False);
        self->diff_corr = Py_False;
        Py_INCREF(Py_False);
        self->eph_avail = Py_False;
        Py_INCREF(Py_False);
        self->alm_avail = Py_False;
        Py_INCREF(Py_False);
    }
    return (PyObject*)self;
}

static void SatelliteInfo_dealloc(SatelliteInfo* self)
{
    Py_XDECREF(self->used_in_fix);
    Py_XDECREF(self->healthy);
    Py_XDECREF(self->diff_corr);
    Py_XDECREF(self->eph_avail);
    Py_XDECREF(self->alm_avail);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* SatelliteInfo_str(SatelliteInfo* self)
{
    char buffer[512];
    snprintf(
        buffer,
        sizeof(buffer),
        "SatelliteInfo(\n"
        "    gnss_id=%d\n"
        "    sv_id=%u\n"
        "    cno=%u\n"
        "    elevation=%d\n"
        "    azimuth=%d\n"
        "    quality=%d\n"
        "    used_in_fix=%s\n"
        "    healthy=%s\n"
        "    diff_corr=%s\n"
        "    eph_avail=%s\n"
        "    alm_avail=%s\n"
        ")",
        self->gnss_id,
        self->sv_id,
        self->cno,
        self->elevation,
        self->azimuth,
        self->quality,
        PyObject_IsTrue(self->used_in_fix) ? "True" : "False",
        PyObject_IsTrue(self->healthy) ? "True" : "False",
        PyObject_IsTrue(self->diff_corr) ? "True" : "False",
        PyObject_IsTrue(self->eph_avail) ? "True" : "False",
        PyObject_IsTrue(self->alm_avail) ? "True" : "False"
    );
    return PyUnicode_FromString(buffer);
}

static PyMemberDef SatelliteInfo_members[] = {
    {
        "gnss_id",
        T_INT,
        offsetof(SatelliteInfo, gnss_id),
        0,
        "GNSS constellation ID (use GnssId IntEnum to interpret)"
    },
    {
        "sv_id",
        T_UBYTE,
        offsetof(SatelliteInfo, sv_id),
        0,
        "Satellite vehicle ID"
    },
    {
        "cno",
        T_UBYTE,
        offsetof(SatelliteInfo, cno),
        0,
        "Carrier-to-noise ratio (dBHz)"
    },
    {
        "elevation",
        T_BYTE,
        offsetof(SatelliteInfo, elevation),
        0,
        "Elevation in degrees (-90..90)"
    },
    {
        "azimuth",
        T_SHORT,
        offsetof(SatelliteInfo, azimuth),
        0,
        "Azimuth in degrees (0..360)"
    },
    {
        "quality",
        T_INT,
        offsetof(SatelliteInfo, quality),
        0,
        "Signal quality indicator (use SvQuality IntEnum to interpret)"
    },
    {
        "used_in_fix",
        T_OBJECT_EX,
        offsetof(SatelliteInfo, used_in_fix),
        0,
        "Whether satellite is used in navigation fix"
    },
    {
        "healthy",
        T_OBJECT_EX,
        offsetof(SatelliteInfo, healthy),
        0,
        "Satellite health flag"
    },
    {
        "diff_corr",
        T_OBJECT_EX,
        offsetof(SatelliteInfo, diff_corr),
        0,
        "Differential correction available"
    },
    {
        "eph_avail",
        T_OBJECT_EX,
        offsetof(SatelliteInfo, eph_avail),
        0,
        "Ephemeris data available"
    },
    {
        "alm_avail",
        T_OBJECT_EX,
        offsetof(SatelliteInfo, alm_avail),
        0,
        "Almanac data available"
    },
    {NULL}
};

static PyTypeObject SatelliteInfoType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.SatelliteInfo",
    .tp_doc = "Satellite information from NAV-SAT",
    .tp_basicsize = sizeof(SatelliteInfo),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = SatelliteInfo_new,
    .tp_dealloc = (destructor)SatelliteInfo_dealloc,
    .tp_str = (reprfunc)SatelliteInfo_str,
    .tp_repr = (reprfunc)SatelliteInfo_str,
    .tp_members = SatelliteInfo_members,
};

/* ---- DilutionOverPrecision ---- */

static PyObject* DilutionOverPrecision_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    DilutionOverPrecision* self =
        (DilutionOverPrecision*)type->tp_alloc(type, 0);
    if (self)
    {
        self->geometric = 0.0f;
        self->position = 0.0f;
        self->time = 0.0f;
        self->vertical = 0.0f;
        self->horizontal = 0.0f;
        self->northing = 0.0f;
        self->easting = 0.0f;
    }
    return (PyObject*)self;
}

static PyObject* DilutionOverPrecision_str(DilutionOverPrecision* self)
{
    char buffer[512];
    snprintf(
        buffer,
        sizeof(buffer),
        "DilutionOverPrecision(\n"
        "    geometric=%.2f\n"
        "    position=%.2f\n"
        "    time=%.2f\n"
        "    vertical=%.2f\n"
        "    horizontal=%.2f\n"
        "    northing=%.2f\n"
        "    easting=%.2f\n"
        ")",
        self->geometric,
        self->position,
        self->time,
        self->vertical,
        self->horizontal,
        self->northing,
        self->easting
    );
    return PyUnicode_FromString(buffer);
}

static PyMemberDef DilutionOverPrecision_members[] = {
    {
        "geometric",
        T_FLOAT,
        offsetof(DilutionOverPrecision, geometric),
        0,
        "Geometric DOP"
    },
    {
        "position",
        T_FLOAT,
        offsetof(DilutionOverPrecision, position),
        0,
        "Position DOP"
    },
    {
        "time",
        T_FLOAT,
        offsetof(DilutionOverPrecision, time),
        0,
        "Time DOP"
    },
    {
        "vertical",
        T_FLOAT,
        offsetof(DilutionOverPrecision, vertical),
        0,
        "Vertical DOP"
    },
    {
        "horizontal",
        T_FLOAT,
        offsetof(DilutionOverPrecision, horizontal),
        0,
        "Horizontal DOP"
    },
    {
        "northing",
        T_FLOAT,
        offsetof(DilutionOverPrecision, northing),
        0,
        "Northing DOP"
    },
    {
        "easting",
        T_FLOAT,
        offsetof(DilutionOverPrecision, easting),
        0,
        "Easting DOP"
    },
    {NULL}
};

static PyTypeObject DilutionOverPrecisionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.DilutionOverPrecision",
    .tp_doc = "Dilution of Precision (DOP) values",
    .tp_basicsize = sizeof(DilutionOverPrecision),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = DilutionOverPrecision_new,
    .tp_str = (reprfunc)DilutionOverPrecision_str,
    .tp_repr = (reprfunc)DilutionOverPrecision_str,
    .tp_members = DilutionOverPrecision_members,
};

static PyObject* GeofencingCfg_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    GeofencingCfg* self = (GeofencingCfg*)type->tp_alloc(type, 0);
    if (self)
    {
        self->confidence_level = 0;
        self->geofences = PyList_New(0);
    }
    return (PyObject*)self;
}

static void GeofencingCfg_dealloc(GeofencingCfg* self)
{
    Py_XDECREF(self->geofences);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef GeofencingCfg_members[] = {
    {
        "confidence_level",
        T_UBYTE,
        offsetof(GeofencingCfg, confidence_level),
        0,
        "Confidence level"
    },
    {
        "geofences",
        T_OBJECT_EX,
        offsetof(GeofencingCfg, geofences),
        0,
        "List of geofences"
    },
    {NULL}
};

static PyTypeObject GeofencingCfgType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.GeofencingCfg",
    .tp_doc = "Geofencing configuration",
    .tp_basicsize = sizeof(GeofencingCfg),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = GeofencingCfg_new,
    .tp_dealloc = (destructor)GeofencingCfg_dealloc,
    .tp_members = GeofencingCfg_members,
};

static PyObject* GeofencingNav_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    GeofencingNav* self = (GeofencingNav*)type->tp_alloc(type, 0);
    if (self)
    {
        self->iTOW = 0;
        self->status = 0;
        self->number_of_geofences = 0;
        self->combined_state = 0;
        self->geofences = PyList_New(0);
    }
    return (PyObject*)self;
}

static void GeofencingNav_dealloc(GeofencingNav* self)
{
    Py_XDECREF(self->geofences);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef GeofencingNav_members[] = {
    {
        "iTOW",
        T_UINT,
        offsetof(GeofencingNav, iTOW),
        0,
        "Time of week"
    },
    {
        "status",
        T_INT,
        offsetof(GeofencingNav, status),
        0,
        "Geofencing status"
    },
    {
        "number_of_geofences",
        T_UBYTE,
        offsetof(GeofencingNav, number_of_geofences),
        0,
        "Number of geofences"
    },
    {
        "combined_state",
        T_INT,
        offsetof(GeofencingNav, combined_state),
        0,
        "Combined geofence state"
    },
    {
        "geofences",
        T_OBJECT_EX,
        offsetof(GeofencingNav, geofences),
        0,
        "Geofence statuses"
    },
    {NULL}
};

static PyTypeObject GeofencingNavType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.GeofencingNav",
    .tp_doc = "Geofencing navigation data",
    .tp_basicsize = sizeof(GeofencingNav),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = GeofencingNav_new,
    .tp_dealloc = (destructor)GeofencingNav_dealloc,
    .tp_members = GeofencingNav_members,
};

static PyObject* Geofencing_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    Geofencing* self = (Geofencing*)type->tp_alloc(type, 0);
    if (self)
    {
        self->cfg = NULL;
        self->nav = NULL;
    }
    return (PyObject*)self;
}

static void Geofencing_dealloc(Geofencing* self)
{
    Py_XDECREF(self->cfg);
    Py_XDECREF(self->nav);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef Geofencing_members[] = {
    {
        "cfg",
        T_OBJECT_EX,
        offsetof(Geofencing, cfg),
        0,
        "Geofencing configuration"
    },
    {
        "nav",
        T_OBJECT_EX,
        offsetof(Geofencing, nav),
        0,
        "Geofencing navigation data"
    },
    {NULL}
};

static PyTypeObject GeofencingType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.Geofencing",
    .tp_doc = "Geofencing",
    .tp_basicsize = sizeof(Geofencing),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Geofencing_new,
    .tp_dealloc = (destructor)Geofencing_dealloc,
    .tp_members = Geofencing_members,
};

static PyObject* PositionVelocityTime_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    PositionVelocityTime* self = (PositionVelocityTime*)type->tp_alloc(type, 0);
    if (self)
    {
        self->latitude = 0.0;
        self->longitude = 0.0;
        self->altitude = 0.0;
        self->altitude_msl = 0.0;
        self->speed_over_ground = 0.0;
        self->speed_accuracy = 0.0;
        self->heading = 0.0;
        self->heading_accuracy = 0.0;
        self->visible_satellites = 0;
        self->horizontal_accuracy = 0.0;
        self->vertical_accuracy = 0.0;
        self->fix_quality = 0;
        self->fix_status = 0;
        self->fix_type = 0;
        self->utc_time = UtcTimeType.tp_alloc(&UtcTimeType, 0);
        if (self->utc_time) {
            ((UtcTime*)self->utc_time)->valid = Py_False;
            Py_INCREF(Py_False);
        }
        self->date = DateType.tp_alloc(&DateType, 0);
        if (self->date) {
            ((Date*)self->date)->valid = Py_False;
            Py_INCREF(Py_False);
        }
    }
    return (PyObject*)self;
}

static void PositionVelocityTime_dealloc(PositionVelocityTime* self)
{
    Py_XDECREF(self->utc_time);
    Py_XDECREF(self->date);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PositionVelocityTime_str(PositionVelocityTime* self)
{
    /* utc_time / date __str__ */
    PyObject* utc_repr = NULL;
    PyObject* date_repr = NULL;
    const char* utc_str = "N/A";
    const char* date_str = "N/A";

    if (self->utc_time && Py_TYPE(self->utc_time) == &UtcTimeType)
    {
        utc_repr = UtcTime_str((UtcTime*)self->utc_time);
        if (utc_repr) utc_str = PyUnicode_AsUTF8(utc_repr);
    }
    if (self->date && Py_TYPE(self->date) == &DateType)
    {
        date_repr = Date_str((Date*)self->date);
        if (date_repr) date_str = PyUnicode_AsUTF8(date_repr);
    }

    char buffer[4096];
    snprintf(
        buffer,
        sizeof(buffer),
        "PositionVelocityTime(\n"
        "    fix_quality=%d\n"
        "    fix_status=%d\n"
        "    fix_type=%d\n"
        "    utc_time=%s\n"
        "    date=%s\n"
        "    altitude=%.2fm\n"
        "    altitude_msl=%.2fm\n"
        "    latitude=%.8f°\n"
        "    longitude=%.8f°\n"
        "    speed_over_ground=%.2fm/s\n"
        "    speed_accuracy=%.2fm/s\n"
        "    heading=%.2f°\n"
        "    heading_accuracy=%.2f°\n"
        "    visible_satellites=%d\n"
        "    horizontal_accuracy=%.2fm\n"
        "    vertical_accuracy=%.2fm\n"
        ")",
        self->fix_quality,
        self->fix_status,
        self->fix_type,
        utc_str,
        date_str,
        self->altitude,
        self->altitude_msl,
        self->latitude,
        self->longitude,
        self->speed_over_ground,
        self->speed_accuracy,
        self->heading,
        self->heading_accuracy,
        self->visible_satellites,
        self->horizontal_accuracy,
        self->vertical_accuracy
    );

    Py_XDECREF(utc_repr);
    Py_XDECREF(date_repr);

    return PyUnicode_FromString(buffer);
}

static PyMemberDef PositionVelocityTime_members[] = {
    {
        "latitude",
        T_DOUBLE,
        offsetof(PositionVelocityTime, latitude),
        0,
        "Latitude in degrees"
    },
    {
        "longitude",
        T_DOUBLE,
        offsetof(PositionVelocityTime, longitude),
        0,
        "Longitude in degrees"
    },
    {
        "altitude",
        T_FLOAT,
        offsetof(PositionVelocityTime, altitude),
        0,
        "Altitude above ellipsoid in meters"
    },
    {
        "altitude_msl",
        T_FLOAT,
        offsetof(PositionVelocityTime, altitude_msl),
        0,
        "Altitude above MSL in meters"
    },
    {
        "speed_over_ground",
        T_FLOAT,
        offsetof(PositionVelocityTime, speed_over_ground),
        0,
        "Speed over ground in m/s"
    },
    {
        "speed_accuracy",
        T_FLOAT,
        offsetof(PositionVelocityTime, speed_accuracy),
        0,
        "Speed accuracy in m/s"
    },
    {
        "heading",
        T_FLOAT,
        offsetof(PositionVelocityTime, heading),
        0,
        "Heading in degrees"
    },
    {
        "heading_accuracy",
        T_FLOAT,
        offsetof(PositionVelocityTime, heading_accuracy),
        0,
        "Heading accuracy in degrees"
    },
    {
        "visible_satellites",
        T_UBYTE,
        offsetof(PositionVelocityTime, visible_satellites),
        0,
        "Number of visible satellites"
    },
    {
        "horizontal_accuracy",
        T_FLOAT,
        offsetof(PositionVelocityTime, horizontal_accuracy),
        0,
        "Horizontal accuracy in meters"
    },
    {
        "vertical_accuracy",
        T_FLOAT,
        offsetof(PositionVelocityTime, vertical_accuracy),
        0,
        "Vertical accuracy in meters"
    },
    {
        "fix_quality",
        T_INT,
        offsetof(PositionVelocityTime, fix_quality),
        0,
        "Fix quality (use FixQuality IntEnum to interpret)"
    },
    {
        "fix_status",
        T_INT,
        offsetof(PositionVelocityTime, fix_status),
        0,
        "Fix status (use FixStatus IntEnum to interpret)"
    },
    {
        "fix_type",
        T_INT,
        offsetof(PositionVelocityTime, fix_type),
        0,
        "Fix type (use FixType IntEnum to interpret)"
    },
    {
        "utc_time",
        T_OBJECT_EX,
        offsetof(PositionVelocityTime, utc_time),
        0,
        "UTC time information"
    },
    {
        "date",
        T_OBJECT_EX,
        offsetof(PositionVelocityTime, date),
        0,
        "Date information"
    },
    {NULL}
};

static PyTypeObject PositionVelocityTimeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.PositionVelocityTime",
    .tp_doc = "Position, Velocity and Time information from GNSS",
    .tp_basicsize = sizeof(PositionVelocityTime),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PositionVelocityTime_new,
    .tp_dealloc = (destructor)PositionVelocityTime_dealloc,
    .tp_str = (reprfunc)PositionVelocityTime_str,
    .tp_repr = (reprfunc)PositionVelocityTime_str,
    .tp_members = PositionVelocityTime_members,
};

static PyObject* Pulse_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    Pulse* self = (Pulse*)type->tp_alloc(type, 0);
    if (self)
    {
        self->frequency = 0;
        self->pulse_width = 0.0f;
    }
    return (PyObject*)self;
}

static PyMemberDef Pulse_members[] = {
    {
        "frequency",
        T_UINT,
        offsetof(Pulse, frequency),
        0,
        "Pulse frequency in Hz"
    },
    {
        "pulse_width",
        T_FLOAT,
        offsetof(Pulse, pulse_width),
        0,
        "Pulse width as a duty cycle or in seconds"
    },
    {NULL}
};

static PyTypeObject PulseType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.Pulse",
    .tp_doc = "Timepulse pulse configuration",
    .tp_basicsize = sizeof(Pulse),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Pulse_new,
    .tp_members = Pulse_members,
};

static PyObject* TimepulsePinConfig_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    TimepulsePinConfig* self = (TimepulsePinConfig*)type->tp_alloc(type, 0);
    if (self)
    {
        self->active = Py_False;
        Py_INCREF(self->active);
        self->fixed_pulse =
            (Pulse*)PulseType.tp_alloc(&PulseType, 0);
        self->pulse_when_no_fix =
            (Pulse*)PulseType.tp_alloc(&PulseType, 0);
        self->polarity = JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE;
    }
    return (PyObject*)self;
}

static void TimepulsePinConfig_dealloc(TimepulsePinConfig* self)
{
    Py_XDECREF(self->active);
    Py_XDECREF(self->fixed_pulse);
    Py_XDECREF(self->pulse_when_no_fix);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef TimepulsePinConfig_members[] = {
    {
        "active",
        T_OBJECT_EX,
        offsetof(TimepulsePinConfig, active),
        0,
        "Is timepulse active"
    },
    {
        "fixed_pulse",
        T_OBJECT_EX,
        offsetof(TimepulsePinConfig, fixed_pulse),
        0,
        "Pulse configuration when there is a fix"
    },
    {
        "pulse_when_no_fix",
        T_OBJECT_EX,
        offsetof(TimepulsePinConfig, pulse_when_no_fix),
        0,
        "Pulse configuration when there is no fix"
    },
    {
        "polarity",
        T_LONG,
        offsetof(TimepulsePinConfig, polarity),
        0,
        "Pulse polarity"
    },
    {NULL}
};

static PyTypeObject TimepulsePinConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.TimepulsePinConfig",
    .tp_doc = "Timepulse pin configuration",
    .tp_basicsize = sizeof(TimepulsePinConfig),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = TimepulsePinConfig_new,
    .tp_dealloc = (destructor)TimepulsePinConfig_dealloc,
    .tp_members = TimepulsePinConfig_members,
};

static PyObject* GnssHat_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    GnssHat* self = (GnssHat*)type->tp_alloc(type, 0);
    if (self)
    {
        self->hat = NULL;
        self->callback = NULL;
    }
    return (PyObject*)self;
}

static int GnssHat_init(GnssHat* self, PyObject* args, PyObject* kwds)
{
    self->hat = jp_gnss_hat_create();
    if (self->hat)
        return 0;

    PyErr_SetString(PyExc_RuntimeError, "Failed to create GNSS HAT instance");
    return -1;
}

static void GnssHat_dealloc(GnssHat* self)
{
    if (self->hat)
    {
        jp_gnss_hat_destroy(self->hat);
        self->hat = NULL;
    }
    Py_XDECREF(self->callback);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static bool validate_config(PyObject* config_dict)
{
    if (!config_dict)
    {
        PyErr_SetString(PyExc_TypeError, "config is required");
        return false;
    }

    if (!PyDict_Check(config_dict))
    {
        PyErr_SetString(PyExc_TypeError, "config must be a dictionary");
        return false;
    }

    PyObject* rate = PyDict_GetItemString(config_dict, "measurement_rate_hz");
    if (rate && !PyLong_Check(rate))
    {
        PyErr_SetString(PyExc_TypeError,
            "measurement_rate_hz must be an integer");
        return false;
    }

    PyObject* model = PyDict_GetItemString(config_dict, "dynamic_model");
    if (model && !PyLong_Check(model))
    {
        PyErr_SetString(PyExc_TypeError,
            "dynamic_model must be an integer");
        return false;
    }

    PyObject* timepulse_dict = PyDict_GetItemString(config_dict,
        "timepulse_pin_config");
    if (timepulse_dict && timepulse_dict != Py_None &&
        !PyDict_Check(timepulse_dict))
    {
        PyErr_SetString(PyExc_TypeError,
            "timepulse_pin_config must be a dictionary or None");
        return false;
    }

    if (timepulse_dict && timepulse_dict != Py_None)
    {
        PyObject* fixed_pulse_dict = PyDict_GetItemString(timepulse_dict,
            "fixed_pulse");
        if (fixed_pulse_dict && !PyDict_Check(fixed_pulse_dict))
        {
            PyErr_SetString(PyExc_TypeError,
                "fixed_pulse must be a dictionary");
            return false;
        }

        if (fixed_pulse_dict)
        {
            PyObject* freq = PyDict_GetItemString(fixed_pulse_dict,
                "frequency");
            if (freq && !PyLong_Check(freq))
            {
                PyErr_SetString(PyExc_TypeError,
                    "frequency must be an integer");
                return false;
            }

            PyObject* width = PyDict_GetItemString(fixed_pulse_dict,
                "pulse_width");
            if (width && !PyFloat_Check(width))
            {
                PyErr_SetString(PyExc_TypeError, "pulse_width must be a float");
                return false;
            }
        }

        PyObject* pulse_when_no_fix_dict = PyDict_GetItemString(timepulse_dict,
            "pulse_when_no_fix");
        if (pulse_when_no_fix_dict && pulse_when_no_fix_dict != Py_None &&
            !PyDict_Check(pulse_when_no_fix_dict))
        {
            PyErr_SetString(PyExc_TypeError,
                "pulse_when_no_fix must be a dictionary or None");
            return false;
        }

        if (pulse_when_no_fix_dict && pulse_when_no_fix_dict != Py_None)
        {
            PyObject* freq = PyDict_GetItemString(pulse_when_no_fix_dict,
                "frequency");
            if (freq && !PyLong_Check(freq))
            {
                PyErr_SetString(PyExc_TypeError,
                    "pulse_when_no_fix frequency must be an integer");
                return false;
            }

            PyObject* width = PyDict_GetItemString(pulse_when_no_fix_dict,
                "pulse_width");
            if (width && !PyFloat_Check(width))
            {
                PyErr_SetString(PyExc_TypeError,
                    "pulse_when_no_fix pulse_width must be a float");
                return false;
            }
        }

        PyObject* polarity = PyDict_GetItemString(timepulse_dict, "polarity");
        if (polarity && !PyLong_Check(polarity))
        {
            PyErr_SetString(PyExc_TypeError, "polarity must be an integer");
            return false;
        }
    }

    PyObject* geofencing_dict = PyDict_GetItemString(config_dict, "geofencing");
    if (geofencing_dict && geofencing_dict != Py_None &&
        !PyDict_Check(geofencing_dict))
    {
        PyErr_SetString(PyExc_TypeError,
            "geofencing must be a dictionary or None");
        return false;
    }

    if (geofencing_dict && geofencing_dict != Py_None)
    {
        PyObject* confidence_level = PyDict_GetItemString(geofencing_dict,
            "confidence_level");
        if (confidence_level && !PyLong_Check(confidence_level))
        {
            PyErr_SetString(PyExc_TypeError,
                "confidence_level must be an integer");
            return false;
        }

        PyObject* geofences_list = PyDict_GetItemString(geofencing_dict,
            "geofences");
        if (geofences_list && !PyList_Check(geofences_list))
        {
            PyErr_SetString(PyExc_TypeError,
                "geofences must be a list");
            return false;
        }

        if (geofences_list)
        {
            Py_ssize_t geofence_count = PyList_Size(geofences_list);
            if (geofence_count > UBLOX_MAX_GEOFENCES)
            {
                PyErr_Format(
                    PyExc_ValueError,
                    "Too many geofences: %zd (max: %d)", 
                    geofence_count,
                    UBLOX_MAX_GEOFENCES
                );
                return false;
            }

            for (Py_ssize_t i = 0; i < geofence_count; i++)
            {
                PyObject* fence_dict = PyList_GetItem(geofences_list, i);
                if (!fence_dict || !PyDict_Check(fence_dict))
                {
                    PyErr_Format(PyExc_TypeError,
                        "geofence[%zd] must be a dictionary", i);
                    return false;
                }

                PyObject* lat = PyDict_GetItemString(fence_dict, "lat");
                if (lat && !PyFloat_Check(lat))
                {
                    PyErr_Format(PyExc_TypeError,
                        "geofence[%zd] lat must be a float", i);
                    return false;
                }

                PyObject* lon = PyDict_GetItemString(fence_dict, "lon");
                if (lon && !PyFloat_Check(lon))
                {
                    PyErr_Format(PyExc_TypeError,
                        "geofence[%zd] lon must be a float", i);
                    return false;
                }

                PyObject* radius = PyDict_GetItemString(fence_dict, "radius");
                if (radius && !PyFloat_Check(radius) && !PyLong_Check(radius))
                {
                    PyErr_Format(PyExc_TypeError,
                        "geofence[%zd] radius must be a number", i);
                    return false;
                }
            }
        }
    }

    /* ── RTK config validation ──────────────────────────────────────── */
    PyObject* rtk_dict = PyDict_GetItemString(config_dict, "rtk");
    if (rtk_dict && rtk_dict != Py_None &&
        !PyDict_Check(rtk_dict))
    {
        PyErr_SetString(PyExc_TypeError,
            "rtk must be a dictionary or None");
        return false;
    }

    if (rtk_dict && rtk_dict != Py_None)
    {
        PyObject* rtk_mode = PyDict_GetItemString(rtk_dict, "mode");
        if (!rtk_mode || !PyLong_Check(rtk_mode))
        {
            PyErr_SetString(PyExc_TypeError,
                "rtk.mode is required and must be an integer (RtkMode)");
            return false;
        }

        int mode_val = (int)PyLong_AsLong(rtk_mode);
        if (mode_val != JP_GNSS_RTK_MODE_BASE &&
            mode_val != JP_GNSS_RTK_MODE_ROVER)
        {
            PyErr_SetString(PyExc_ValueError,
                "rtk.mode must be RtkMode.BASE or RtkMode.ROVER");
            return false;
        }

        PyObject* base_dict = PyDict_GetItemString(rtk_dict, "base");
        if (base_dict && base_dict != Py_None &&
            !PyDict_Check(base_dict))
        {
            PyErr_SetString(PyExc_TypeError,
                "rtk.base must be a dictionary or None");
            return false;
        }

        if (base_dict && base_dict != Py_None)
        {
            PyObject* base_mode = PyDict_GetItemString(base_dict, "base_mode");
            if (!base_mode || !PyLong_Check(base_mode))
            {
                PyErr_SetString(PyExc_TypeError,
                    "rtk.base.base_mode is required and must be an integer "
                    "(BaseMode)");
                return false;
            }

            int base_mode_val = (int)PyLong_AsLong(base_mode);
            if (base_mode_val != JP_GNSS_BASE_MODE_SURVEY_IN &&
                base_mode_val != JP_GNSS_BASE_MODE_FIXED_POSITION)
            {
                PyErr_SetString(PyExc_ValueError,
                    "rtk.base.base_mode must be BaseMode.SURVEY_IN "
                    "or BaseMode.FIXED_POSITION");
                return false;
            }

            if (base_mode_val == JP_GNSS_BASE_MODE_SURVEY_IN)
            {
                PyObject* si_dict = PyDict_GetItemString(base_dict,
                    "survey_in");
                if (!si_dict || !PyDict_Check(si_dict))
                {
                    PyErr_SetString(PyExc_TypeError,
                        "rtk.base.survey_in is required and must be a "
                        "dictionary when base_mode is SURVEY_IN");
                    return false;
                }

                PyObject* obs_time = PyDict_GetItemString(si_dict,
                    "minimum_observation_time_s");
                if (obs_time && !PyLong_Check(obs_time))
                {
                    PyErr_SetString(PyExc_TypeError,
                        "survey_in.minimum_observation_time_s must be "
                        "an integer");
                    return false;
                }

                PyObject* acc = PyDict_GetItemString(si_dict,
                    "required_position_accuracy_m");
                if (acc && !PyFloat_Check(acc) && !PyLong_Check(acc))
                {
                    PyErr_SetString(PyExc_TypeError,
                        "survey_in.required_position_accuracy_m must be "
                        "a number");
                    return false;
                }
            }
            else /* FIXED_POSITION */
            {
                PyObject* fp_dict = PyDict_GetItemString(base_dict,
                    "fixed_position");
                if (!fp_dict || !PyDict_Check(fp_dict))
                {
                    PyErr_SetString(PyExc_TypeError,
                        "rtk.base.fixed_position is required and must be "
                        "a dictionary when base_mode is FIXED_POSITION");
                    return false;
                }

                PyObject* pos_type = PyDict_GetItemString(fp_dict,
                    "position_type");
                if (!pos_type || !PyLong_Check(pos_type))
                {
                    PyErr_SetString(PyExc_TypeError,
                        "fixed_position.position_type is required and must "
                        "be an integer (FixedPositionType)");
                    return false;
                }

                int pt_val = (int)PyLong_AsLong(pos_type);
                if (pt_val != JP_GNSS_FIXED_POSITION_ECEF &&
                    pt_val != JP_GNSS_FIXED_POSITION_LLA)
                {
                    PyErr_SetString(PyExc_ValueError,
                        "position_type must be FixedPositionType.ECEF "
                        "or FixedPositionType.LLA");
                    return false;
                }

                if (pt_val == JP_GNSS_FIXED_POSITION_ECEF)
                {
                    PyObject* ecef_dict = PyDict_GetItemString(fp_dict,
                        "ecef");
                    if (!ecef_dict || !PyDict_Check(ecef_dict))
                    {
                        PyErr_SetString(PyExc_TypeError,
                            "fixed_position.ecef is required when "
                            "position_type is ECEF");
                        return false;
                    }
                }
                else
                {
                    PyObject* lla_dict = PyDict_GetItemString(fp_dict, "lla");
                    if (!lla_dict || !PyDict_Check(lla_dict))
                    {
                        PyErr_SetString(PyExc_TypeError,
                            "fixed_position.lla is required when "
                            "position_type is LLA");
                        return false;
                    }
                }

                PyObject* pos_acc = PyDict_GetItemString(fp_dict,
                    "position_accuracy_m");
                if (pos_acc && !PyFloat_Check(pos_acc) &&
                    !PyLong_Check(pos_acc))
                {
                    PyErr_SetString(PyExc_TypeError,
                        "fixed_position.position_accuracy_m must be "
                        "a number");
                    return false;
                }
            }
        }
    }

    return true;
}

static void populate_config_from_dict(PyObject* config_dict, jp_gnss_gnss_config_t* config)
{
    config->timepulse_pin_config.active = false;
    config->has_geofencing = false;

    PyObject* rate = PyDict_GetItemString(config_dict, "measurement_rate_hz");
    if (rate)
        config->measurement_rate_hz = (uint16_t)PyLong_AsLong(rate);

    PyObject* model = PyDict_GetItemString(config_dict, "dynamic_model");
    if (model)
        config->dynamic_model = (jp_gnss_dynamic_model_t)PyLong_AsLong(model);

    PyObject* timepulse_dict = PyDict_GetItemString(config_dict, "timepulse_pin_config");
    if (timepulse_dict && timepulse_dict != Py_None)
    {
        jp_gnss_timepulse_pin_config_t* tpc = &config->timepulse_pin_config;

        PyObject* active = PyDict_GetItemString(timepulse_dict, "active");
        if (active)
            tpc->active = PyObject_IsTrue(active);

        PyObject* polarity = PyDict_GetItemString(timepulse_dict, "polarity");
        if (polarity)
            tpc->polarity = (jp_gnss_timepulse_polarity_t)PyLong_AsLong(polarity);

        PyObject* fixed_pulse_dict = PyDict_GetItemString(timepulse_dict, "fixed_pulse");
        if (fixed_pulse_dict)
        {
            PyObject* freq = PyDict_GetItemString(fixed_pulse_dict, "frequency");
            if (freq)
                tpc->fixed_pulse.frequency = (uint32_t)PyLong_AsLong(freq);

            PyObject* width = PyDict_GetItemString(fixed_pulse_dict, "pulse_width");
            if (width)
                tpc->fixed_pulse.pulse_width = (float)PyFloat_AsDouble(width);
        }

        PyObject* pulse_when_no_fix_dict = PyDict_GetItemString(timepulse_dict, "pulse_when_no_fix");
        if (pulse_when_no_fix_dict && pulse_when_no_fix_dict != Py_None)
        {
            tpc->has_pulse_when_no_fix = true;
            
            PyObject* freq = PyDict_GetItemString(pulse_when_no_fix_dict, "frequency");
            if (freq)
                tpc->pulse_when_no_fix.frequency = (uint32_t)PyLong_AsLong(freq);

            PyObject* width = PyDict_GetItemString(pulse_when_no_fix_dict, "pulse_width");
            if (width)
                tpc->pulse_when_no_fix.pulse_width = (float)PyFloat_AsDouble(width);
        }
        else
        {
            tpc->has_pulse_when_no_fix = false;
        }
    }

    PyObject* geofencing_dict = PyDict_GetItemString(config_dict, "geofencing");
    if (geofencing_dict && geofencing_dict != Py_None)
    {
        config->has_geofencing = true;
        
        PyObject* confidence_level = PyDict_GetItemString(geofencing_dict, "confidence_level");
        if (confidence_level)
            config->geofencing.confidence_level = (uint8_t)PyLong_AsLong(confidence_level);
        
        PyObject* geofences_list = PyDict_GetItemString(geofencing_dict, "geofences");
        if (geofences_list)
        {
            Py_ssize_t geofence_count = PyList_Size(geofences_list);
            if (geofence_count > UBLOX_MAX_GEOFENCES)
                geofence_count = UBLOX_MAX_GEOFENCES;
            
            config->geofencing.geofence_count = (uint8_t)geofence_count;
            
            for (Py_ssize_t i = 0; i < geofence_count; i++)
            {
                PyObject* fence_dict = PyList_GetItem(geofences_list, i);
                
                PyObject* lat = PyDict_GetItemString(fence_dict, "lat");
                if (lat)
                    config->geofencing.geofences[i].lat = (float)PyFloat_AsDouble(lat);
                
                PyObject* lon = PyDict_GetItemString(fence_dict, "lon");
                if (lon)
                    config->geofencing.geofences[i].lon = (float)PyFloat_AsDouble(lon);
                
                PyObject* radius = PyDict_GetItemString(fence_dict, "radius");
                if (radius)
                {
                    if (PyFloat_Check(radius))
                        config->geofencing.geofences[i].radius = (float)PyFloat_AsDouble(radius);
                    else if (PyLong_Check(radius))
                        config->geofencing.geofences[i].radius = (float)PyLong_AsDouble(radius);
                }
            }
        }
    }
    else
    {
        config->has_geofencing = false;
    }

    /* ── RTK config ─────────────────────────────────────────────────── */
    config->has_rtk = false;
    PyObject* rtk_dict = PyDict_GetItemString(config_dict, "rtk");
    if (rtk_dict && rtk_dict != Py_None)
    {
        config->has_rtk = true;
        memset(&config->rtk, 0, sizeof(config->rtk));

        PyObject* rtk_mode = PyDict_GetItemString(rtk_dict, "mode");
        if (rtk_mode)
            config->rtk.mode =
                (jp_gnss_rtk_mode_t)PyLong_AsLong(rtk_mode);

        config->rtk.has_base_config = false;
        PyObject* base_dict = PyDict_GetItemString(rtk_dict, "base");
        if (base_dict && base_dict != Py_None)
        {
            config->rtk.has_base_config = true;

            PyObject* base_mode = PyDict_GetItemString(base_dict,
                "base_mode");
            if (base_mode)
                config->rtk.base.base_mode =
                    (jp_gnss_base_mode_t)PyLong_AsLong(base_mode);

            if (config->rtk.base.base_mode == JP_GNSS_BASE_MODE_SURVEY_IN)
            {
                PyObject* si_dict = PyDict_GetItemString(base_dict,
                    "survey_in");
                if (si_dict)
                {
                    PyObject* obs_time = PyDict_GetItemString(si_dict,
                        "minimum_observation_time_s");
                    if (obs_time)
                        config->rtk.base.survey_in
                            .minimum_observation_time_s =
                                (uint32_t)PyLong_AsLong(obs_time);

                    PyObject* acc = PyDict_GetItemString(si_dict,
                        "required_position_accuracy_m");
                    if (acc)
                    {
                        if (PyFloat_Check(acc))
                            config->rtk.base.survey_in
                                .required_position_accuracy_m =
                                    PyFloat_AsDouble(acc);
                        else if (PyLong_Check(acc))
                            config->rtk.base.survey_in
                                .required_position_accuracy_m =
                                    PyLong_AsDouble(acc);
                    }
                }
            }
            else /* FIXED_POSITION */
            {
                PyObject* fp_dict = PyDict_GetItemString(base_dict,
                    "fixed_position");
                if (fp_dict)
                {
                    PyObject* pos_type = PyDict_GetItemString(fp_dict,
                        "position_type");
                    if (pos_type)
                        config->rtk.base.fixed_position.position_type =
                            (jp_gnss_fixed_position_type_t)
                                PyLong_AsLong(pos_type);

                    if (config->rtk.base.fixed_position.position_type ==
                        JP_GNSS_FIXED_POSITION_ECEF)
                    {
                        PyObject* ecef_dict = PyDict_GetItemString(
                            fp_dict, "ecef");
                        if (ecef_dict)
                        {
                            PyObject* x = PyDict_GetItemString(
                                ecef_dict, "x_m");
                            if (x) config->rtk.base.fixed_position
                                .ecef.x_m = PyFloat_AsDouble(x);

                            PyObject* y = PyDict_GetItemString(
                                ecef_dict, "y_m");
                            if (y) config->rtk.base.fixed_position
                                .ecef.y_m = PyFloat_AsDouble(y);

                            PyObject* z = PyDict_GetItemString(
                                ecef_dict, "z_m");
                            if (z) config->rtk.base.fixed_position
                                .ecef.z_m = PyFloat_AsDouble(z);
                        }
                    }
                    else /* LLA */
                    {
                        PyObject* lla_dict = PyDict_GetItemString(
                            fp_dict, "lla");
                        if (lla_dict)
                        {
                            PyObject* lat = PyDict_GetItemString(
                                lla_dict, "latitude_deg");
                            if (lat) config->rtk.base.fixed_position
                                .lla.latitude_deg =
                                    PyFloat_AsDouble(lat);

                            PyObject* lon = PyDict_GetItemString(
                                lla_dict, "longitude_deg");
                            if (lon) config->rtk.base.fixed_position
                                .lla.longitude_deg =
                                    PyFloat_AsDouble(lon);

                            PyObject* h = PyDict_GetItemString(
                                lla_dict, "height_m");
                            if (h) config->rtk.base.fixed_position
                                .lla.height_m = PyFloat_AsDouble(h);
                        }
                    }

                    PyObject* pos_acc = PyDict_GetItemString(fp_dict,
                        "position_accuracy_m");
                    if (pos_acc)
                    {
                        if (PyFloat_Check(pos_acc))
                            config->rtk.base.fixed_position
                                .position_accuracy_m =
                                    PyFloat_AsDouble(pos_acc);
                        else if (PyLong_Check(pos_acc))
                            config->rtk.base.fixed_position
                                .position_accuracy_m =
                                    PyLong_AsDouble(pos_acc);
                    }
                }
            }
        }
    }
}

#define CHECK_HAT(self) do { \
    if (!(self)->hat) { \
        PyErr_SetString(PyExc_RuntimeError, "GNSS HAT not initialized"); \
        return NULL; \
    } \
} while (0)

static PyObject* GnssHat_start(GnssHat* self, PyObject* args, PyObject* kwargs)
{
    CHECK_HAT(self);
    PyObject* config_dict = NULL;

    static char* kwlist[] = {"config", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &config_dict))
        return NULL;

    if (!validate_config(config_dict))
        return NULL;

    jp_gnss_gnss_config_t config;
    populate_config_from_dict(config_dict, &config);

    bool result = jp_gnss_hat_start(self->hat, &config);
    if (!result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to start GNSS HAT");
        return NULL;
    }

    Py_RETURN_TRUE;
}

/* ---- Fast allocators (bypass tp_new/tp_init overhead) ---- */

static inline UtcTime* UtcTime_alloc(void)
{
    UtcTime* self = (UtcTime*)UtcTimeType.tp_alloc(&UtcTimeType, 0);
    if (self) { self->valid = Py_False; Py_INCREF(Py_False); }
    return self;
}

static inline Date* Date_alloc(void)
{
    Date* self = (Date*)DateType.tp_alloc(&DateType, 0);
    if (self) { self->valid = Py_False; Py_INCREF(Py_False); }
    return self;
}

static inline PositionVelocityTime* PVT_alloc(void)
{
    PositionVelocityTime* self =
        (PositionVelocityTime*)PositionVelocityTimeType.tp_alloc(
            &PositionVelocityTimeType, 0);
    if (self) { self->utc_time = NULL; self->date = NULL; }
    return self;
}

static inline DilutionOverPrecision* DOP_alloc(void)
{
    return (DilutionOverPrecision*)DilutionOverPrecisionType.tp_alloc(
        &DilutionOverPrecisionType, 0);
}

static inline Geofence* Geofence_alloc(void)
{
    return (Geofence*)GeofenceType.tp_alloc(&GeofenceType, 0);
}

static inline GeofencingCfg* GeofencingCfg_alloc(void)
{
    GeofencingCfg* self =
        (GeofencingCfg*)GeofencingCfgType.tp_alloc(&GeofencingCfgType, 0);
    if (self) { self->geofences = NULL; }
    return self;
}

static inline GeofencingNav* GeofencingNav_alloc(void)
{
    GeofencingNav* self =
        (GeofencingNav*)GeofencingNavType.tp_alloc(&GeofencingNavType, 0);
    if (self) { self->geofences = NULL; }
    return self;
}

static inline Geofencing* Geofencing_alloc(void)
{
    Geofencing* self =
        (Geofencing*)GeofencingType.tp_alloc(&GeofencingType, 0);
    if (self) { self->cfg = NULL; self->nav = NULL; }
    return self;
}

static inline RfBlock* RfBlock_alloc(void)
{
    return (RfBlock*)RfBlockType.tp_alloc(&RfBlockType, 0);
}

static inline SatelliteInfo* SatelliteInfo_alloc(void)
{
    SatelliteInfo* self =
        (SatelliteInfo*)SatelliteInfoType.tp_alloc(&SatelliteInfoType, 0);
    if (self)
    {
        self->used_in_fix = Py_False; Py_INCREF(Py_False);
        self->healthy = Py_False; Py_INCREF(Py_False);
        self->diff_corr = Py_False; Py_INCREF(Py_False);
        self->eph_avail = Py_False; Py_INCREF(Py_False);
        self->alm_avail = Py_False; Py_INCREF(Py_False);
    }
    return self;
}

static inline Navigation* Navigation_alloc(void)
{
    Navigation* self =
        (Navigation*)NavigationType.tp_alloc(&NavigationType, 0);
    if (self)
    {
        self->dop = NULL;
        self->pvt = NULL;
        self->geofencing = NULL;
        self->rf_blocks = NULL;
        self->satellites = NULL;
    }
    return self;
}

/* Private function to convert C navigation data to Python objects */
static PyObject* convert_navigation_to_python(const jp_gnss_navigation_t* nav)
{
    Navigation* nav_obj = Navigation_alloc();
    if (!nav_obj)
        return NULL;

    /* Convert PVT data */
    PositionVelocityTime* pvt = PVT_alloc();

    if (!pvt)
    {
        Py_DECREF(nav_obj);
        return NULL;
    }

    pvt->latitude = nav->pvt.latitude;
    pvt->longitude = nav->pvt.longitude;
    pvt->altitude = nav->pvt.altitude;
    pvt->altitude_msl = nav->pvt.altitude_msl;
    pvt->speed_over_ground = nav->pvt.speed_over_ground;
    pvt->speed_accuracy = nav->pvt.speed_accuracy;
    pvt->heading = nav->pvt.heading;
    pvt->heading_accuracy = nav->pvt.heading_accuracy;
    pvt->visible_satellites = nav->pvt.visible_satellites;
    pvt->horizontal_accuracy = nav->pvt.horizontal_accuracy;
    pvt->vertical_accuracy = nav->pvt.vertical_accuracy;
    pvt->fix_quality = (int)nav->pvt.fix_quality;
    pvt->fix_status = (int)nav->pvt.fix_status;
    pvt->fix_type = (int)nav->pvt.fix_type;

    /* Populate UtcTime object */
    UtcTime* utc = UtcTime_alloc();
    if (utc)
    {
        utc->hours = nav->pvt.utc.hh;
        utc->minutes = nav->pvt.utc.mm;
        utc->seconds = nav->pvt.utc.ss;
        Py_DECREF(utc->valid);
        utc->valid = nav->pvt.utc.valid ? Py_True : Py_False;
        Py_INCREF(utc->valid);
        utc->accuracy = nav->pvt.utc.accuracy;
    }
    pvt->utc_time = (PyObject*)utc;

    /* Populate Date object */
    Date* dt = Date_alloc();
    if (dt)
    {
        dt->day = nav->pvt.date.day;
        dt->month = nav->pvt.date.month;
        dt->year = nav->pvt.date.year;
        Py_DECREF(dt->valid);
        dt->valid = nav->pvt.date.valid ? Py_True : Py_False;
        Py_INCREF(dt->valid);
    }
    pvt->date = (PyObject*)dt;

    nav_obj->pvt = (PyObject*)pvt;

    /* Convert DOP data */
    DilutionOverPrecision* dop_obj = DOP_alloc();
    if (!dop_obj)
    {
        Py_DECREF(nav_obj);
        return NULL;
    }

    dop_obj->geometric = nav->dop.geometric;
    dop_obj->position = nav->dop.position;
    dop_obj->time = nav->dop.time;
    dop_obj->vertical = nav->dop.vertical;
    dop_obj->horizontal = nav->dop.horizontal;
    dop_obj->northing = nav->dop.northing;
    dop_obj->easting = nav->dop.easting;

    nav_obj->dop = (PyObject*)dop_obj;

    Geofencing* geofencing_obj = Geofencing_alloc();
    if (!geofencing_obj)
    {
        Py_DECREF(nav_obj);
        return NULL;
    }

    GeofencingCfg* geofencing_cfg = GeofencingCfg_alloc();
    if (!geofencing_cfg)
    {
        Py_DECREF(nav_obj);
        Py_DECREF(geofencing_obj);
        return NULL;
    }
    
    geofencing_cfg->confidence_level = nav->geofencing.cfg.confidence_level;
    
    /* Create list of geofences */
    PyObject* geofences_list = PyList_New(nav->geofencing.cfg.geofence_count);
    if (!geofences_list)
    {
        Py_DECREF(nav_obj);
        Py_DECREF(geofencing_obj);
        Py_DECREF(geofencing_cfg);
        return NULL;
    }
    
    for (int i = 0; i < nav->geofencing.cfg.geofence_count; i++)
    {
        Geofence* geofence = Geofence_alloc();
        if (!geofence)
        {
            Py_DECREF(nav_obj);
            Py_DECREF(geofencing_obj);
            Py_DECREF(geofencing_cfg);
            Py_DECREF(geofences_list);
            return NULL;
        }
        
        geofence->lat = nav->geofencing.cfg.geofences[i].lat;
        geofence->lon = nav->geofencing.cfg.geofences[i].lon;
        geofence->radius = nav->geofencing.cfg.geofences[i].radius;
        
        PyList_SetItem(geofences_list, i, (PyObject*)geofence);
    }

    geofencing_cfg->geofences = geofences_list;

    /* Create geofencing nav */
    GeofencingNav* geofencing_nav = GeofencingNav_alloc();
    if (!geofencing_nav)
    {
        Py_DECREF(nav_obj);
        Py_DECREF(geofencing_obj);
        Py_DECREF(geofencing_cfg);
        return NULL;
    }

    geofencing_nav->iTOW = nav->geofencing.nav.iTOW;
    geofencing_nav->status = nav->geofencing.nav.geofencing_status;
    geofencing_nav->number_of_geofences = nav->geofencing.nav.number_of_geofences;
    geofencing_nav->combined_state = nav->geofencing.nav.combined_state;

    // Create list of geofence statuses
    PyObject* geofence_statuses = PyList_New(geofencing_nav->number_of_geofences);
    if (!geofence_statuses)
    {
        Py_DECREF(nav_obj);
        Py_DECREF(geofencing_obj);
        Py_DECREF(geofencing_cfg);
        Py_DECREF(geofencing_nav);
        return NULL;
    }

    for (int i = 0; i < nav->geofencing.nav.number_of_geofences; i++)
    {
        PyObject* status = PyLong_FromLong(nav->geofencing.nav.geofences_status[i]);
        PyList_SetItem(geofence_statuses, i, status);
    }

    geofencing_nav->geofences = geofence_statuses;

    geofencing_obj->cfg = (PyObject*)geofencing_cfg;
    geofencing_obj->nav = (PyObject*)geofencing_nav;
    nav_obj->geofencing = (PyObject*)geofencing_obj;

    // Create RF blocks list
    PyObject* rf_blocks_list = PyList_New(nav->num_rf_blocks);
    if (!rf_blocks_list)
    {
        Py_DECREF(nav_obj);
        return NULL;
    }

    for (int i = 0; i < nav->num_rf_blocks; i++)
    {
        RfBlock* rf_block = RfBlock_alloc();
        if (!rf_block)
        {
            Py_DECREF(nav_obj);
            Py_DECREF(rf_blocks_list);
            return NULL;
        }

        rf_block->id = nav->rf_blocks[i].id;
        rf_block->jamming_state = nav->rf_blocks[i].jamming_state;
        rf_block->antenna_status = nav->rf_blocks[i].antenna_status;
        rf_block->antenna_power = nav->rf_blocks[i].antenna_power;
        rf_block->post_status = nav->rf_blocks[i].post_status;
        rf_block->noise_per_ms = nav->rf_blocks[i].noise_per_ms;
        rf_block->agc_monitor = nav->rf_blocks[i].agc_monitor;
        rf_block->cw_interference_suppression_level =
            nav->rf_blocks[i].cw_interference_suppression_level;
        rf_block->ofs_i = nav->rf_blocks[i].ofs_i;
        rf_block->mag_i = nav->rf_blocks[i].mag_i;
        rf_block->ofs_q = nav->rf_blocks[i].ofs_q;
        rf_block->mag_q = nav->rf_blocks[i].mag_q;

        PyList_SetItem(rf_blocks_list, i, (PyObject*)rf_block);
    }

    nav_obj->rf_blocks = rf_blocks_list;

    /* Convert satellite data */
    PyObject* satellites_list = PyList_New(nav->num_satellites);
    if (!satellites_list)
    {
        Py_DECREF(nav_obj);
        return NULL;
    }

    for (int i = 0; i < nav->num_satellites; i++)
    {
        SatelliteInfo* sat = SatelliteInfo_alloc();
        if (!sat)
        {
            Py_DECREF(nav_obj);
            Py_DECREF(satellites_list);
            return NULL;
        }

        sat->gnss_id = nav->satellites[i].gnss_id;
        sat->sv_id = nav->satellites[i].sv_id;
        sat->cno = nav->satellites[i].cno;
        sat->elevation = nav->satellites[i].elevation;
        sat->azimuth = nav->satellites[i].azimuth;
        sat->quality = nav->satellites[i].quality;

        Py_DECREF(sat->used_in_fix);
        sat->used_in_fix = nav->satellites[i].used_in_fix ? Py_True : Py_False;
        Py_INCREF(sat->used_in_fix);

        Py_DECREF(sat->healthy);
        sat->healthy = nav->satellites[i].healthy ? Py_True : Py_False;
        Py_INCREF(sat->healthy);

        Py_DECREF(sat->diff_corr);
        sat->diff_corr = nav->satellites[i].diff_corr ? Py_True : Py_False;
        Py_INCREF(sat->diff_corr);

        Py_DECREF(sat->eph_avail);
        sat->eph_avail = nav->satellites[i].eph_avail ? Py_True : Py_False;
        Py_INCREF(sat->eph_avail);

        Py_DECREF(sat->alm_avail);
        sat->alm_avail = nav->satellites[i].alm_avail ? Py_True : Py_False;
        Py_INCREF(sat->alm_avail);

        PyList_SetItem(satellites_list, i, (PyObject*)sat);
    }

    nav_obj->satellites = satellites_list;

    return (PyObject*)nav_obj;
}

static PyObject* GnssHat_get_navigation(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    jp_gnss_navigation_t nav;
    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = jp_gnss_hat_get_navigation(self->hat, &nav);
    Py_END_ALLOW_THREADS

    if (!result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get navigation data");
        return NULL;
    }

    return convert_navigation_to_python(&nav);
}

static PyObject* GnssHat_wait_and_get_fresh_navigation(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    jp_gnss_navigation_t nav;
    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = jp_gnss_hat_wait_and_get_fresh_navigation(self->hat, &nav);
    Py_END_ALLOW_THREADS

    if (!result)
    {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Failed to wait for navigation data"
        );
        return NULL;
    }

    return convert_navigation_to_python(&nav);
}

static PyObject* GnssHat_enable_timepulse(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    bool result = jp_gnss_hat_enable_timepulse(self->hat);
    if (!result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to enable timepulse");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* GnssHat_disable_timepulse(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    jp_gnss_hat_disable_timepulse(self->hat);
    Py_RETURN_NONE;
}

static PyObject* GnssHat_start_forward_for_gpsd(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    bool result = jp_gnss_hat_start_forward_for_gpsd(self->hat);
    if (!result)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to start GPSD forwarding");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* GnssHat_stop_forward_for_gpsd(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    jp_gnss_hat_stop_forward_for_gpsd(self->hat);
    Py_RETURN_NONE;
}

static PyObject* GnssHat_join_forward_for_gpsd(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    Py_BEGIN_ALLOW_THREADS
    jp_gnss_hat_join_forward_for_gpsd(self->hat);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject* GnssHat_get_gpsd_device_path(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    const char* path = jp_gnss_hat_get_gpsd_device_path(self->hat);
    if (!path)
        Py_RETURN_NONE;

    return PyUnicode_FromString(path);
}

static PyObject* GnssHat_hard_reset_cold_start(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    jp_gnss_hat_hard_reset_cold_start(self->hat);
    Py_RETURN_NONE;
}

static PyObject* GnssHat_soft_reset_hot_start(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);
    jp_gnss_hat_soft_reset_hot_start(self->hat);
    Py_RETURN_NONE;
}

static PyObject* GnssHat_timepulse(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);

    Py_BEGIN_ALLOW_THREADS
    jp_gnss_hat_timepulse(self->hat);
    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

static PyObject* GnssHat_rtk_get_full_corrections(GnssHat* self,
    PyObject* args)
{
    CHECK_HAT(self);

    jp_gnss_rtk_corrections_t* corrections;

    Py_BEGIN_ALLOW_THREADS
    corrections = jp_gnss_rtk_get_full_corrections(self->hat);
    Py_END_ALLOW_THREADS

    if (!corrections)
    {
        PyErr_SetString(PyExc_RuntimeError,
            "Failed to get RTK corrections (RTK not available or no data)");
        return NULL;
    }

    PyObject* list = PyList_New(corrections->count);
    if (!list)
    {
        jp_gnss_rtk_corrections_free(corrections);
        return NULL;
    }

    for (uint32_t i = 0; i < corrections->count; i++)
    {
        PyObject* frame_bytes = PyBytes_FromStringAndSize(
            (const char*)corrections->frames[i].data,
            corrections->frames[i].size
        );
        if (!frame_bytes)
        {
            Py_DECREF(list);
            jp_gnss_rtk_corrections_free(corrections);
            return NULL;
        }
        PyList_SetItem(list, i, frame_bytes);
    }

    jp_gnss_rtk_corrections_free(corrections);
    return list;
}

static PyObject* GnssHat_rtk_get_tiny_corrections(GnssHat* self,
    PyObject* args)
{
    CHECK_HAT(self);

    jp_gnss_rtk_corrections_t* corrections;

    Py_BEGIN_ALLOW_THREADS
    corrections = jp_gnss_rtk_get_tiny_corrections(self->hat);
    Py_END_ALLOW_THREADS

    if (!corrections)
    {
        PyErr_SetString(PyExc_RuntimeError,
            "Failed to get RTK corrections (RTK not available or no data)");
        return NULL;
    }

    PyObject* list = PyList_New(corrections->count);
    if (!list)
    {
        jp_gnss_rtk_corrections_free(corrections);
        return NULL;
    }

    for (uint32_t i = 0; i < corrections->count; i++)
    {
        PyObject* frame_bytes = PyBytes_FromStringAndSize(
            (const char*)corrections->frames[i].data,
            corrections->frames[i].size
        );
        if (!frame_bytes)
        {
            Py_DECREF(list);
            jp_gnss_rtk_corrections_free(corrections);
            return NULL;
        }
        PyList_SetItem(list, i, frame_bytes);
    }

    jp_gnss_rtk_corrections_free(corrections);
    return list;
}

static PyObject* GnssHat_rtk_get_rtcm3_frame(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);

    int frame_id;
    if (!PyArg_ParseTuple(args, "i", &frame_id))
        return NULL;

    jp_gnss_rtcm3_frame_t* frame;

    Py_BEGIN_ALLOW_THREADS
    frame = jp_gnss_rtk_get_rtcm3_frame(self->hat, (uint16_t)frame_id);
    Py_END_ALLOW_THREADS

    if (!frame)
    {
        PyErr_SetString(PyExc_RuntimeError,
            "Failed to get RTCM3 frame (RTK not available or no data)");
        return NULL;
    }

    PyObject* result = PyBytes_FromStringAndSize(
        (const char*)frame->data, frame->size
    );

    jp_gnss_rtcm3_frame_free(frame);
    return result;
}

static PyObject* GnssHat_rtk_apply_corrections(GnssHat* self, PyObject* args)
{
    CHECK_HAT(self);

    PyObject* corrections_list;
    if (!PyArg_ParseTuple(args, "O", &corrections_list))
        return NULL;

    if (!PyList_Check(corrections_list))
    {
        PyErr_SetString(PyExc_TypeError,
            "corrections must be a list of bytes objects");
        return NULL;
    }

    Py_ssize_t count = PyList_Size(corrections_list);
    if (count == 0)
    {
        PyErr_SetString(PyExc_ValueError, "corrections list is empty");
        return NULL;
    }

    jp_gnss_rtcm3_frame_t* frames = (jp_gnss_rtcm3_frame_t*)calloc(
        count, sizeof(jp_gnss_rtcm3_frame_t)
    );
    if (!frames)
    {
        PyErr_NoMemory();
        return NULL;
    }

    for (Py_ssize_t i = 0; i < count; i++)
    {
        PyObject* item = PyList_GetItem(corrections_list, i);
        if (!PyBytes_Check(item))
        {
            free(frames);
            PyErr_Format(PyExc_TypeError,
                "corrections[%zd] must be a bytes object", i);
            return NULL;
        }

        frames[i].data = (uint8_t*)PyBytes_AsString(item);
        frames[i].size = (uint32_t)PyBytes_Size(item);
    }

    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = jp_gnss_rtk_apply_corrections(
        self->hat, frames, (uint32_t)count
    );
    Py_END_ALLOW_THREADS

    free(frames);

    if (!result)
    {
        PyErr_SetString(PyExc_RuntimeError,
            "Failed to apply RTK corrections (RTK rover not available)");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* GnssHat_enter(GnssHat* self, PyObject* args)
{
    Py_INCREF(self);
    return (PyObject*)self;
}

static PyObject* GnssHat_exit(GnssHat* self, PyObject* args)
{
    if (self->hat)
    {
        jp_gnss_hat_stop_forward_for_gpsd(self->hat);
        jp_gnss_hat_disable_timepulse(self->hat);
    }
    Py_RETURN_NONE;
}

static PyMethodDef GnssHat_methods[] = {
    {
        "start",
        (PyCFunction)GnssHat_start,
        METH_VARARGS | METH_KEYWORDS, 
        "Start GNSS HAT with configuration"
    },
    {
        "get_navigation",
        (PyCFunction)GnssHat_get_navigation,
        METH_NOARGS,
        "Get current navigation data"
    },
    {
        "wait_and_get_fresh_navigation",
        (PyCFunction)GnssHat_wait_and_get_fresh_navigation,
        METH_NOARGS,
        "Wait for fresh navigation data"
    },
    {
        "enable_timepulse",
        (PyCFunction)GnssHat_enable_timepulse,
        METH_NOARGS,
        "Enable timepulse output"
    },
    {
        "disable_timepulse",
        (PyCFunction)GnssHat_disable_timepulse,
        METH_NOARGS,
        "Disable timepulse output"
    },
    {
        "start_forward_for_gpsd",
        (PyCFunction)GnssHat_start_forward_for_gpsd,
        METH_NOARGS,
        "Start forwarding NMEA data to GPSD"
    },
    {
        "stop_forward_for_gpsd",
        (PyCFunction)GnssHat_stop_forward_for_gpsd,
        METH_NOARGS,
        "Stop GPSD forwarding"
    },
    {
        "join_forward_for_gpsd",
        (PyCFunction)GnssHat_join_forward_for_gpsd,
        METH_NOARGS,
        "Join (wait for) GPSD forwarding thread"
    },
    {
        "get_gpsd_device_path",
        (PyCFunction)GnssHat_get_gpsd_device_path,
        METH_NOARGS,
        "Get virtual device path for GPSD"
    },
    {
        "hard_reset_cold_start",
        (PyCFunction)GnssHat_hard_reset_cold_start,
        METH_NOARGS,
        "Perform hard reset (cold start)"
    },
    {
        "soft_reset_hot_start",
        (PyCFunction)GnssHat_soft_reset_hot_start,
        METH_NOARGS,
        "Perform soft reset (hot start)"
    },
    {
        "timepulse",
        (PyCFunction)GnssHat_timepulse,
        METH_NOARGS,
        "Wait for the next timepulse interrupt. "
        "Call enable_timepulse() first."
    },
    {
        "rtk_get_full_corrections",
        (PyCFunction)GnssHat_rtk_get_full_corrections,
        METH_NOARGS,
        "Get full RTK base corrections (M7M). "
        "Returns a list of bytes objects (RTCM3 frames)."
    },
    {
        "rtk_get_tiny_corrections",
        (PyCFunction)GnssHat_rtk_get_tiny_corrections,
        METH_NOARGS,
        "Get tiny RTK base corrections (M4M). "
        "Returns a list of bytes objects (RTCM3 frames)."
    },
    {
        "rtk_get_rtcm3_frame",
        (PyCFunction)GnssHat_rtk_get_rtcm3_frame,
        METH_VARARGS,
        "Get a specific RTCM3 frame by message ID. "
        "Returns a bytes object."
    },
    {
        "rtk_apply_corrections",
        (PyCFunction)GnssHat_rtk_apply_corrections,
        METH_VARARGS,
        "Apply RTK corrections to a rover. "
        "Takes a list of bytes objects (RTCM3 frames)."
    },
    {
        "__enter__",
        (PyCFunction)GnssHat_enter,
        METH_NOARGS,
        "Context manager entry"
    },
    {
        "__exit__",
        (PyCFunction)GnssHat_exit,
        METH_VARARGS,
        "Context manager exit"
    },
    {NULL}
};

static PyTypeObject GnssHatType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.GnssHat",
    .tp_doc = "Jimmy Paputto GNSS HAT interface",
    .tp_basicsize = sizeof(GnssHat),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = GnssHat_new,
    .tp_init = (initproc)GnssHat_init,
    .tp_dealloc = (destructor)GnssHat_dealloc,
    .tp_methods = GnssHat_methods,
};

static PyObject* Navigation_new(PyTypeObject* type, PyObject* args,
    PyObject* kwds)
{
    Navigation* self = (Navigation*)type->tp_alloc(type, 0);
    if (self)
    {
        self->dop = (PyObject*)DOP_alloc();
        self->pvt = (PyObject*)PVT_alloc();
        self->geofencing = (PyObject*)Geofencing_alloc();
        self->rf_blocks = PyList_New(0);
        self->satellites = PyList_New(0);
    }
    return (PyObject*)self;
}

static void Navigation_dealloc(Navigation* self)
{
    Py_XDECREF(self->dop);
    Py_XDECREF(self->pvt);
    Py_XDECREF(self->geofencing);
    Py_XDECREF(self->rf_blocks);
    Py_XDECREF(self->satellites);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef Navigation_members[] = {
    {
        "dop",
        T_OBJECT_EX,
        offsetof(Navigation, dop),
        0,
        "Dilution of Precision"
    },
    {
        "pvt",
        T_OBJECT_EX,
        offsetof(Navigation, pvt),
        0,
        "Position, Velocity and Time"
    },
    {
        "geofencing",
        T_OBJECT_EX,
        offsetof(Navigation, geofencing),
        0,
        "Geofencing information"
    },
    {
        "rf_blocks",
        T_OBJECT_EX,
        offsetof(Navigation, rf_blocks),
        0,
        "RF blocks information"
    },
    {
        "satellites",
        T_OBJECT_EX,
        offsetof(Navigation, satellites),
        0,
        "Satellite information from NAV-SAT"
    },
    {NULL}
};

static PyObject* Navigation_str(Navigation* self)
{
    /* Build sub-object strings — always own a reference for cleanup */
    PyObject* pvt_str = NULL;
    PyObject* dop_str = NULL;

    if (self->pvt)
        pvt_str = PyObject_Str(self->pvt);
    if (!pvt_str)
        pvt_str = PyUnicode_FromString("N/A");

    if (self->dop)
        dop_str = PyObject_Str(self->dop);
    if (!dop_str)
        dop_str = PyUnicode_FromString("N/A");

    /* Build RF blocks string (safe against concat failures) */
    PyObject* rf_parts = PyUnicode_FromString("");
    if (rf_parts && self->rf_blocks && PyList_Check(self->rf_blocks))
    {
        Py_ssize_t n = PyList_Size(self->rf_blocks);
        for (Py_ssize_t i = 0; i < n; i++)
        {
            PyObject* item = PyList_GetItem(self->rf_blocks, i);
            PyObject* item_str = PyObject_Str(item);
            if (!item_str)
                break;

            PyObject* prefix = PyUnicode_FromFormat("\n  [%zd] ", i);
            if (!prefix) { Py_DECREF(item_str); break; }

            PyObject* tmp = PyUnicode_Concat(rf_parts, prefix);
            Py_DECREF(prefix);
            if (!tmp) { Py_DECREF(item_str); break; }

            Py_DECREF(rf_parts);
            rf_parts = PyUnicode_Concat(tmp, item_str);
            Py_DECREF(tmp);
            Py_DECREF(item_str);
            if (!rf_parts)
            {
                rf_parts = PyUnicode_FromString("(error)");
                break;
            }
        }
    }

    /* Build geofencing string */
    PyObject* geo_str = PyUnicode_FromString("N/A");
    if (self->geofencing)
    {
        Geofencing* g = (Geofencing*)self->geofencing;
        if (g->cfg && g->nav)
        {
            GeofencingNav* gnav = (GeofencingNav*)g->nav;
            GeofencingCfg* gcfg = (GeofencingCfg*)g->cfg;
            Py_DECREF(geo_str);
            geo_str = PyUnicode_FromFormat(
                "Geofencing(confidence=%u, fences=%u, status=%d, combined=%d)",
                gcfg->confidence_level,
                gnav->number_of_geofences,
                gnav->status,
                gnav->combined_state
            );
        }
    }

    /* Build satellites string */
    PyObject* sat_parts = PyUnicode_FromString("");
    if (sat_parts && self->satellites && PyList_Check(self->satellites))
    {
        Py_ssize_t n = PyList_Size(self->satellites);
        for (Py_ssize_t i = 0; i < n; i++)
        {
            PyObject* item = PyList_GetItem(self->satellites, i);
            PyObject* item_str = PyObject_Str(item);
            if (!item_str)
                break;

            PyObject* prefix = PyUnicode_FromFormat("\n  [%zd] ", i);
            if (!prefix) { Py_DECREF(item_str); break; }

            PyObject* tmp = PyUnicode_Concat(sat_parts, prefix);
            Py_DECREF(prefix);
            if (!tmp) { Py_DECREF(item_str); break; }

            Py_DECREF(sat_parts);
            sat_parts = PyUnicode_Concat(tmp, item_str);
            Py_DECREF(tmp);
            Py_DECREF(item_str);
            if (!sat_parts)
            {
                sat_parts = PyUnicode_FromString("(error)");
                break;
            }
        }
    }

    PyObject* result = PyUnicode_FromFormat(
        "===== Navigation =====\n"
        "\n--- PVT ---\n%U\n"
        "\n--- DOP ---\n%U\n"
        "\n--- Geofencing ---\n%U\n"
        "\n--- RF Blocks ---%U\n"
        "\n--- Satellites (%zd) ---%U\n"
        "=======================",
        pvt_str,
        dop_str,
        geo_str,
        rf_parts,
        self->satellites && PyList_Check(self->satellites)
            ? PyList_Size(self->satellites) : 0,
        sat_parts
    );

    Py_XDECREF(pvt_str);
    Py_XDECREF(dop_str);
    Py_DECREF(geo_str);
    Py_DECREF(rf_parts);
    Py_DECREF(sat_parts);

    return result;
}

static PyTypeObject NavigationType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "jimmypaputto.gnsshat.Navigation",
    .tp_doc = "Complete navigation information",
    .tp_basicsize = sizeof(Navigation),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Navigation_new,
    .tp_dealloc = (destructor)Navigation_dealloc,
    .tp_str = (reprfunc)Navigation_str,
    .tp_repr = (reprfunc)Navigation_str,
    .tp_members = Navigation_members,
};

static PyObject* jimmypaputto_gnss_version(PyObject* self, PyObject* args)
{
    return PyUnicode_FromString("1.0.0");
}

static PyObject* utc_time_iso8601(PyObject* self, PyObject* args)
{
    PyObject* obj;
    if (!PyArg_ParseTuple(args, "O", &obj))
        return NULL;

    PositionVelocityTime* pvt_obj = NULL;

    /* Accept either a Navigation object or a PositionVelocityTime object */
    if (Py_TYPE(obj) == &NavigationType)
    {
        Navigation* nav = (Navigation*)obj;
        if (!nav->pvt)
        {
            PyErr_SetString(PyExc_ValueError,
                "Navigation object has no PVT data");
            return NULL;
        }
        pvt_obj = (PositionVelocityTime*)nav->pvt;
    }
    else if (Py_TYPE(obj) == &PositionVelocityTimeType)
    {
        pvt_obj = (PositionVelocityTime*)obj;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError,
            "Expected a Navigation or PositionVelocityTime object");
        return NULL;
    }

    /* Extract UTC time and date from the Python PVT object */
    jp_gnss_position_velocity_time_t c_pvt;
    memset(&c_pvt, 0, sizeof(c_pvt));

    if (pvt_obj->utc_time && Py_TYPE(pvt_obj->utc_time) == &UtcTimeType)
    {
        UtcTime* utc = (UtcTime*)pvt_obj->utc_time;
        c_pvt.utc.hh = utc->hours;
        c_pvt.utc.mm = utc->minutes;
        c_pvt.utc.ss = utc->seconds;
    }

    if (pvt_obj->date && Py_TYPE(pvt_obj->date) == &DateType)
    {
        Date* dt = (Date*)pvt_obj->date;
        c_pvt.date.day = dt->day;
        c_pvt.date.month = dt->month;
        c_pvt.date.year = dt->year;
    }

    const char* iso_str = jp_gnss_utc_time_iso8601(&c_pvt);
    if (!iso_str)
    {
        PyErr_SetString(PyExc_RuntimeError,
            "Failed to format UTC time as ISO 8601");
        return NULL;
    }
    return PyUnicode_FromString(iso_str);
}

static PyMethodDef jimmypaputto_gnss_methods[] = {
    {
        "version",
        jimmypaputto_gnss_version,
        METH_NOARGS,
        "Get library version"
    },
    {
        "utc_time_iso8601",
        utc_time_iso8601,
        METH_VARARGS,
        "Convert PVT navigation data to ISO 8601 UTC time string. "
        "Takes a Navigation or PositionVelocityTime object."
    },
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef jimmypaputto_gnss_module = {
    PyModuleDef_HEAD_INIT,
    "jimmypaputto.gnsshat",
    "Python interface for Jimmy Paputto GNSS HAT",
    -1,
    jimmypaputto_gnss_methods
};

PyMODINIT_FUNC PyInit_gnsshat(void)
{
    PyObject* m;
    
    if (PyType_Ready(&GnssHatType) < 0)
        return NULL;
    if (PyType_Ready(&PositionVelocityTimeType) < 0)
        return NULL;
    if (PyType_Ready(&NavigationType) < 0)
        return NULL;
    if (PyType_Ready(&DilutionOverPrecisionType) < 0)
        return NULL;
    if (PyType_Ready(&GeofenceType) < 0)
        return NULL;
    if (PyType_Ready(&RfBlockType) < 0)
        return NULL;
    if (PyType_Ready(&SatelliteInfoType) < 0)
        return NULL;
    if (PyType_Ready(&GeofencingCfgType) < 0)
        return NULL;
    if (PyType_Ready(&GeofencingNavType) < 0)
        return NULL;
    if (PyType_Ready(&GeofencingType) < 0)
        return NULL;
    if (PyType_Ready(&PulseType) < 0)
        return NULL;
    if (PyType_Ready(&TimepulsePinConfigType) < 0)
        return NULL;
    if (PyType_Ready(&UtcTimeType) < 0)
        return NULL;
    if (PyType_Ready(&DateType) < 0)
        return NULL;
    
    m = PyModule_Create(&jimmypaputto_gnss_module);
    if (!m)
        return NULL;

    Py_INCREF(&GnssHatType);
    PyModule_AddObject(m, "GnssHat", (PyObject*)&GnssHatType);
    
    Py_INCREF(&PositionVelocityTimeType);
    PyModule_AddObject(m, "PositionVelocityTime",
        (PyObject*)&PositionVelocityTimeType);
    
    Py_INCREF(&NavigationType);
    PyModule_AddObject(m, "Navigation", (PyObject*)&NavigationType);

    Py_INCREF(&DilutionOverPrecisionType);
    PyModule_AddObject(m, "DilutionOverPrecision",
        (PyObject*)&DilutionOverPrecisionType);

    Py_INCREF(&RfBlockType);
    PyModule_AddObject(m, "RfBlock", (PyObject*)&RfBlockType);

    Py_INCREF(&SatelliteInfoType);
    PyModule_AddObject(m, "SatelliteInfo",
        (PyObject*)&SatelliteInfoType);

    Py_INCREF(&PulseType);
    PyModule_AddObject(m, "Pulse", (PyObject*)&PulseType);

    Py_INCREF(&TimepulsePinConfigType);
    PyModule_AddObject(m, "TimepulsePinConfig",
        (PyObject*)&TimepulsePinConfigType);

    Py_INCREF(&UtcTimeType);
    PyModule_AddObject(m, "UtcTime", (PyObject*)&UtcTimeType);

    Py_INCREF(&DateType);
    PyModule_AddObject(m, "Date", (PyObject*)&DateType);

    Py_INCREF(&GeofenceType);
    PyModule_AddObject(m, "Geofence", (PyObject*)&GeofenceType);

    Py_INCREF(&GeofencingCfgType);
    PyModule_AddObject(m, "GeofencingCfg", (PyObject*)&GeofencingCfgType);

    Py_INCREF(&GeofencingNavType);
    PyModule_AddObject(m, "GeofencingNav", (PyObject*)&GeofencingNavType);

    Py_INCREF(&GeofencingType);
    PyModule_AddObject(m, "Geofencing", (PyObject*)&GeofencingType);

    /* ── IntEnum helper ─────────────────────────────────────────────── */
    PyObject *enum_mod = PyImport_ImportModule("enum");
    if (!enum_mod) { Py_DECREF(m); return NULL; }
    PyObject *IntEnum = PyObject_GetAttrString(enum_mod, "IntEnum");
    Py_DECREF(enum_mod);
    if (!IntEnum) { Py_DECREF(m); return NULL; }

    #define MAKE_ENUM(py_name, ...)                                        \
    do {                                                                   \
        typedef struct { const char *n; int v; } _ep;                      \
        _ep _pairs[] = { __VA_ARGS__ };                                    \
        int _cnt = (int)(sizeof(_pairs)/sizeof(_pairs[0]));                \
        PyObject *_d = PyDict_New();                                       \
        for (int _i = 0; _i < _cnt; _i++) {                               \
            PyObject *_val = PyLong_FromLong(_pairs[_i].v);                \
            PyDict_SetItemString(_d, _pairs[_i].n, _val);                 \
            Py_DECREF(_val);                                               \
        }                                                                  \
        PyObject *_nm = PyUnicode_FromString(py_name);                     \
        PyObject *_ar = PyTuple_Pack(2, _nm, _d);                         \
        PyObject *_ec = PyObject_Call(IntEnum, _ar, NULL);                 \
        Py_DECREF(_nm); Py_DECREF(_ar); Py_DECREF(_d);                    \
        if (_ec) PyModule_AddObject(m, py_name, _ec);                     \
    } while(0)

    /* ── DynamicModel ───────────────────────────────────────────────── */
    MAKE_ENUM("DynamicModel",
        {"PORTABLE",    JP_GNSS_DYNAMIC_MODEL_PORTABLE},
        {"STATIONARY",  JP_GNSS_DYNAMIC_MODEL_STATIONARY},
        {"PEDESTRIAN",  JP_GNSS_DYNAMIC_MODEL_PEDESTRIAN},
        {"AUTOMOTIVE",  JP_GNSS_DYNAMIC_MODEL_AUTOMOTIVE},
        {"SEA",         JP_GNSS_DYNAMIC_MODEL_SEA},
        {"AIRBORNE_1G", JP_GNSS_DYNAMIC_MODEL_AIRBORNE_1G},
        {"AIRBORNE_2G", JP_GNSS_DYNAMIC_MODEL_AIRBORNE_2G},
        {"AIRBORNE_4G", JP_GNSS_DYNAMIC_MODEL_AIRBORNE_4G},
        {"WRIST",       JP_GNSS_DYNAMIC_MODEL_WRIST},
        {"BIKE",        JP_GNSS_DYNAMIC_MODEL_BIKE},
        {"MOWER",       JP_GNSS_DYNAMIC_MODEL_MOWER},
        {"ESCOOTER",    JP_GNSS_DYNAMIC_MODEL_ESCOOTER}
    );

    /* ── FixQuality ─────────────────────────────────────────────────── */
    MAKE_ENUM("FixQuality",
        {"INVALID",         JP_GNSS_FIX_QUALITY_INVALID},
        {"GPS_FIX_2D_3D",   JP_GNSS_FIX_QUALITY_GPS_FIX_2D_3D},
        {"DGNSS",           JP_GNSS_FIX_QUALITY_DGNSS},
        {"PPS_FIX",         JP_GNSS_FIX_QUALITY_PPS_FIX},
        {"FIXED_RTK",       JP_GNSS_FIX_QUALITY_FIXED_RTK},
        {"FLOAT_RTK",       JP_GNSS_FIX_QUALITY_FLOAT_RTK},
        {"DEAD_RECKONING",  JP_GNSS_FIX_QUALITY_DEAD_RECKONING}
    );

    /* ── FixStatus ──────────────────────────────────────────────────── */
    MAKE_ENUM("FixStatus",
        {"VOID",   JP_GNSS_FIX_STATUS_VOID},
        {"ACTIVE", JP_GNSS_FIX_STATUS_ACTIVE}
    );

    /* ── FixType ────────────────────────────────────────────────────── */
    MAKE_ENUM("FixType",
        {"NO_FIX",                   JP_GNSS_FIX_TYPE_NO_FIX},
        {"DEAD_RECKONING_ONLY",      JP_GNSS_FIX_TYPE_DEAD_RECKONING_ONLY},
        {"FIX_2D",                   JP_GNSS_FIX_TYPE_FIX_2D},
        {"FIX_3D",                   JP_GNSS_FIX_TYPE_FIX_3D},
        {"GNSS_WITH_DEAD_RECKONING", JP_GNSS_FIX_TYPE_GNSS_WITH_DEAD_RECKONING},
        {"TIME_ONLY_FIX",            JP_GNSS_FIX_TYPE_TIME_ONLY_FIX}
    );

    /* ── TimepulsePolarity ──────────────────────────────────────────── */
    MAKE_ENUM("TimepulsePolarity",
        {"FALLING_EDGE", JP_GNSS_TIMEPULSE_POLARITY_FALLING_EDGE},
        {"RISING_EDGE",  JP_GNSS_TIMEPULSE_POLARITY_RISING_EDGE}
    );

    /* ── PioPinPolarity ─────────────────────────────────────────────── */
    MAKE_ENUM("PioPinPolarity",
        {"LOW_MEANS_INSIDE",  JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_INSIDE},
        {"LOW_MEANS_OUTSIDE", JP_GNSS_PIO_PIN_POLARITY_LOW_MEANS_OUTSIDE}
    );

    /* ── GeofenceStatus ─────────────────────────────────────────────── */
    MAKE_ENUM("GeofenceStatus",
        {"UNKNOWN", JP_GNSS_GEOFENCE_STATUS_UNKNOWN},
        {"INSIDE",  JP_GNSS_GEOFENCE_STATUS_INSIDE},
        {"OUTSIDE", JP_GNSS_GEOFENCE_STATUS_OUTSIDE}
    );

    /* ── GeofencingStatus ───────────────────────────────────────────── */
    MAKE_ENUM("GeofencingStatus",
        {"NOT_AVAILABLE", JP_GNSS_GEOFENCING_STATUS_NOT_AVAILABLE},
        {"ACTIVE",        JP_GNSS_GEOFENCING_STATUS_ACTIVE}
    );

    /* ── RfBand ─────────────────────────────────────────────────────── */
    MAKE_ENUM("RfBand",
        {"L1",       JP_GNSS_RF_BAND_L1},
        {"L2_OR_L5", JP_GNSS_RF_BAND_L2_OR_L5}
    );

    /* ── JammingState ───────────────────────────────────────────────── */
    MAKE_ENUM("JammingState",
        {"UNKNOWN",                                  JP_GNSS_JAMMING_STATE_UNKNOWN},
        {"OK_NO_SIGNIFICANT_JAMMING",                JP_GNSS_JAMMING_STATE_OK_NO_SIGNIFICANT_JAMMING},
        {"WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK",  JP_GNSS_JAMMING_STATE_WARNING_INTERFERENCE_VISIBLE_BUT_FIX_OK},
        {"CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX", JP_GNSS_JAMMING_STATE_CRITICAL_INTERFERENCE_VISIBLE_AND_NO_FIX}
    );

    /* ── AntennaStatus ──────────────────────────────────────────────── */
    MAKE_ENUM("AntennaStatus",
        {"INIT",      JP_GNSS_ANTENNA_STATUS_INIT},
        {"DONT_KNOW", JP_GNSS_ANTENNA_STATUS_DONT_KNOW},
        {"OK",        JP_GNSS_ANTENNA_STATUS_OK},
        {"SHORT",     JP_GNSS_ANTENNA_STATUS_SHORT},
        {"OPEN",      JP_GNSS_ANTENNA_STATUS_OPEN}
    );

    /* ── AntennaPower ───────────────────────────────────────────────── */
    MAKE_ENUM("AntennaPower",
        {"OFF",       JP_GNSS_ANTENNA_POWER_OFF},
        {"ON",        JP_GNSS_ANTENNA_POWER_ON},
        {"DONT_KNOW", JP_GNSS_ANTENNA_POWER_DONT_KNOW}
    );

    /* ── RtkMode ────────────────────────────────────────────────────── */
    MAKE_ENUM("RtkMode",
        {"BASE",  JP_GNSS_RTK_MODE_BASE},
        {"ROVER", JP_GNSS_RTK_MODE_ROVER}
    );

    /* ── BaseMode ───────────────────────────────────────────────────── */
    MAKE_ENUM("BaseMode",
        {"SURVEY_IN",      JP_GNSS_BASE_MODE_SURVEY_IN},
        {"FIXED_POSITION", JP_GNSS_BASE_MODE_FIXED_POSITION}
    );

    /* ── FixedPositionType ──────────────────────────────────────────── */
    MAKE_ENUM("FixedPositionType",
        {"ECEF", JP_GNSS_FIXED_POSITION_ECEF},
        {"LLA",  JP_GNSS_FIXED_POSITION_LLA}
    );

    /* ── GnssId ─────────────────────────────────────────────────────── */
    MAKE_ENUM("GnssId",
        {"GPS",     JP_GNSS_GNSS_ID_GPS},
        {"SBAS",    JP_GNSS_GNSS_ID_SBAS},
        {"GALILEO", JP_GNSS_GNSS_ID_GALILEO},
        {"BEIDOU",  JP_GNSS_GNSS_ID_BEIDOU},
        {"IMES",    JP_GNSS_GNSS_ID_IMES},
        {"QZSS",    JP_GNSS_GNSS_ID_QZSS},
        {"GLONASS", JP_GNSS_GNSS_ID_GLONASS}
    );

    /* ── SvQuality ──────────────────────────────────────────────────── */
    MAKE_ENUM("SvQuality",
        {"NO_SIGNAL",                        JP_GNSS_SV_QUALITY_NO_SIGNAL},
        {"SEARCHING",                        JP_GNSS_SV_QUALITY_SEARCHING},
        {"SIGNAL_ACQUIRED",                  JP_GNSS_SV_QUALITY_SIGNAL_ACQUIRED},
        {"SIGNAL_DETECTED_BUT_UNUSABLE",     JP_GNSS_SV_QUALITY_SIGNAL_DETECTED_BUT_UNUSABLE},
        {"CODE_LOCKED_AND_TIME_SYNCHRONIZED", JP_GNSS_SV_QUALITY_CODE_LOCKED_AND_TIME_SYNCHRONIZED},
        {"CODE_AND_CARRIER_LOCKED_1",        JP_GNSS_SV_QUALITY_CODE_AND_CARRIER_LOCKED_1},
        {"CODE_AND_CARRIER_LOCKED_2",        JP_GNSS_SV_QUALITY_CODE_AND_CARRIER_LOCKED_2},
        {"CODE_AND_CARRIER_LOCKED_3",        JP_GNSS_SV_QUALITY_CODE_AND_CARRIER_LOCKED_3}
    );

    #undef MAKE_ENUM
    Py_DECREF(IntEnum);

    /* ── Hardware limits ────────────────────────────────────────────── */
    PyModule_AddIntConstant(m, "MAX_GEOFENCES", UBLOX_MAX_GEOFENCES);
    PyModule_AddIntConstant(m, "MAX_RF_BLOCKS", UBLOX_MAX_RF_BLOCKS);
    PyModule_AddIntConstant(m, "MAX_SATELLITES", UBLOX_MAX_SATELLITES);

    return m;
}
