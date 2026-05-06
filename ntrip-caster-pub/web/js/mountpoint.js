/*
 * Mountpoint detail page front-end.  Polls /api/mountpoint/:name
 * and renders satellite / RTCM message details.
 */
(function () {
    'use strict';

    const POLL_MS = 1000;
    const $ = (id) => document.getElementById(id);

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

    function fmtBytes(n) {
        if (n == null) return '—';
        if (n < 1024) return n + ' B';
        const u = ['KiB', 'MiB', 'GiB', 'TiB'];
        let v = n / 1024, i = 0;
        while (v >= 1024 && i < u.length - 1) { v /= 1024; i++; }
        return v.toFixed(2) + ' ' + u[i];
    }
    function fmtMs(ms) {
        if (ms == null) return '—';
        if (ms < 1000) return ms + ' ms';
        const s = Math.floor(ms / 1000);
        if (s < 60) return s + ' s';
        if (s < 3600) return Math.floor(s / 60) + 'm ' + (s % 60) + 's';
        const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
        return h + 'h ' + m + 'm';
    }

    function getName() {
        const p = new URLSearchParams(window.location.search);
        return p.get('name') || '';
    }

    function showError(msg) {
        const el = $('error-banner');
        el.textContent = msg;
        el.classList.add('visible');
    }
    function hideError() { $('error-banner').classList.remove('visible'); }

    function renderArp(arp) {
        const el = $('arp-block');
        if (!arp) {
            el.innerHTML = '<span class="v muted">no RTCM 1005/1006 frame seen yet</span>';
            return;
        }
        el.innerHTML =
            '<div class="kv">' +
            '<div class="k">Latitude</div><div class="v">'  + arp.lat_deg.toFixed(7)  + '°</div>' +
            '<div class="k">Longitude</div><div class="v">' + arp.lon_deg.toFixed(7) + '°</div>' +
            '<div class="k">Height</div><div class="v">'    + arp.height_m.toFixed(2) + ' m</div>' +
            '</div>';
    }

    function renderConstellations(list) {
        const tbody = $('sat-tbody');
        if (!list || list.length === 0) {
            tbody.innerHTML =
                '<tr><td colspan="5" class="v muted">no MSM frames seen yet</td></tr>';
            return;
        }
        const rows = list.map(c => {
            const color = GNSS_COLORS[c.gnss] || GNSS_COLORS.Unknown;
            const sats  = c.sats || [];
            const sigs  = c.signal_ids || [];
            const chips = sats.map(s => {
                const placed = (s.az_deg != null && s.el_deg != null);
                const tag = placed
                    ? s.sv_id + '<sup>' + Math.round(s.el_deg) + '°</sup>'
                    : s.sv_id;
                const opacity = placed ? '1' : '0.55';
                return '<span class="sv-chip" style="border-color:' + color +
                    ';color:' + color + ';opacity:' + opacity + '">' +
                    tag + '</span>';
            }).join(' ');
            return '<tr>' +
                '<td><span class="gnss-dot" style="background:' + color + '"></span>' +
                    c.gnss + '</td>' +
                '<td>MSM' + c.msm + ' (' + c.last_msg_type + ')</td>' +
                '<td>' + sats.length + '</td>' +
                '<td>' + (sigs.length ? sigs.join(', ') : '—') + '</td>' +
                '<td><div class="sv-chips">' + (chips || '—') + '</div></td>' +
                '</tr>';
        }).join('');
        tbody.innerHTML = rows;
    }

    function flattenSats(constellations) {
        const out = [];
        for (const c of constellations || []) {
            for (const s of c.sats || []) {
                out.push({
                    gnss:      c.gnss,
                    sv_id:     s.sv_id,
                    az_deg:    s.az_deg,
                    el_deg:    s.el_deg,
                    eph_age_s: s.eph_age_s,
                });
            }
        }
        return out;
    }

    function renderSky(constellations) {
        const sats = flattenSats(constellations);
        if (window.SkyPlot) window.SkyPlot.draw('sky-canvas', sats);
        const placed  = sats.filter(s => s.az_deg != null && s.el_deg != null && s.el_deg >= 0).length;
        const pending = sats.length - placed;
        const note = $('sky-status');
        if (sats.length === 0) {
            note.textContent = 'waiting for MSM frames…';
        } else if (placed === 0) {
            note.textContent = 'waiting for ephemerides (' + sats.length + ' SVs visible)';
        } else {
            note.textContent = placed + ' satellite' + (placed === 1 ? '' : 's') +
                ' positioned' + (pending ? ', ' + pending + ' pending ephemeris' : '');
        }
    }

    function renderMsgTypes(types) {
        const grid = $('msg-grid');
        const keys = Object.keys(types || {}).sort((a, b) => +a - +b);
        if (keys.length === 0) {
            grid.innerHTML = '<div class="v muted">no frames yet</div>';
            return;
        }
        grid.innerHTML = '';
        for (const k of keys) {
            const t = types[k];
            const chip = document.createElement('div');
            chip.className = 'msg-chip';
            const ageStr = (t.last_age_ms != null)
                ? ' · ' + fmtMs(t.last_age_ms) + ' ago'
                : '';
            chip.innerHTML =
                '<span class="type">' + k + '</span>' +
                '<span class="count">' + t.count.toLocaleString() + ageStr + '</span>';
            grid.appendChild(chip);
        }
    }

    function render(j) {
        $('mp-title').textContent = j.name;
        $('mp-clients').textContent = j.clients;
        $('mp-uptime').textContent  = fmtMs(j.uptime_ms);
        $('mp-last').textContent    = fmtMs(j.last_frame_age_ms) + ' ago';
        $('mp-bytes').textContent   = fmtBytes(j.bytes_tx);
        $('mp-frames').textContent  = (j.frames_tx ?? 0).toLocaleString();

        renderArp(j.arp);
        renderConstellations(j.constellations);
        renderSky(j.constellations);
        renderMsgTypes(j.message_types);
    }

    async function poll() {
        const name = getName();
        if (!name) {
            showError('Missing ?name= query parameter.');
            return;
        }
        try {
            const r = await fetch(
                '/api/mountpoint/' + encodeURIComponent(name),
                { credentials: 'same-origin' });
            if (r.status === 404) {
                showError('Mountpoint "' + name + '" is not active.');
            } else if (!r.ok) {
                throw new Error('HTTP ' + r.status);
            } else {
                const j = await r.json();
                hideError();
                render(j);
            }
        } catch (e) {
            showError('Fetch failed: ' + e.message);
        } finally {
            setTimeout(poll, POLL_MS);
        }
    }

    poll();
})();
