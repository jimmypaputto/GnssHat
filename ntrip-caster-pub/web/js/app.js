/*
 * Status page front-end.  Polls /api/status every second and renders.
 */
(function () {
    'use strict';

    const POLL_MS = 1000;
    const $ = (id) => document.getElementById(id);

    function fmtBytes(n) {
        if (n == null) return '—';
        if (n < 1024) return n + ' B';
        const units = ['KiB', 'MiB', 'GiB', 'TiB'];
        let v = n / 1024, i = 0;
        while (v >= 1024 && i < units.length - 1) { v /= 1024; i++; }
        return v.toFixed(2) + ' ' + units[i];
    }

    function fmtMs(ms) {
        if (ms == null) return '—';
        if (ms < 1000) return ms + ' ms';
        const s = Math.floor(ms / 1000);
        if (s < 60)   return s + ' s';
        if (s < 3600) return Math.floor(s / 60) + 'm ' + (s % 60) + 's';
        const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
        return h + 'h ' + m + 'm';
    }

    function classifyMountpoint(s) {
        if (!s.mountpoint) return ['bad', 'no source'];
        if (s.last_frame_age_ms > 5000) return ['bad', 'stale'];
        if (s.last_frame_age_ms > 1500) return ['', 'slow'];
        return ['ok', 'streaming'];
    }

    function showError(msg) {
        const el = $('error-banner');
        el.textContent = msg;
        el.classList.add('visible');
    }
    function hideError() {
        $('error-banner').classList.remove('visible');
    }

    function render(s) {
        const [cls, label] = classifyMountpoint(s);

        $('mp-name').textContent     = s.mountpoint || '(none)';
        const pill = document.createElement('span');
        pill.className = 'status-pill ' + cls;
        pill.textContent = label;
        $('mp-status').replaceChildren(pill);

        $('mp-clients').textContent  = s.clients;
        $('mp-uptime').textContent   = fmtMs(s.uptime_ms);
        $('mp-last').textContent     = s.mountpoint
            ? fmtMs(s.last_frame_age_ms) + ' ago'
            : '—';

        const link = $('mp-link');
        if (s.mountpoint) {
            link.href = '/mountpoint.html?name=' +
                        encodeURIComponent(s.mountpoint);
            link.style.pointerEvents = 'auto';
            link.style.opacity = '1';
        } else {
            link.href = '#';
            link.style.pointerEvents = 'none';
            link.style.opacity = '0.4';
        }

        $('bytes-tx').textContent  = fmtBytes(s.bytes_tx);
        $('bytes-rx').textContent  = fmtBytes(s.bytes_rx);
        $('frames-tx').textContent = (s.frames_tx ?? 0).toLocaleString();
        $('avg-if').textContent    = (s.avg_inter_frame_ms || 0).toFixed(1) + ' ms';
        $('max-if').textContent    = (s.max_inter_frame_ms || 0).toFixed(1) + ' ms';

        const grid = $('msg-grid');
        const types = s.message_types || {};
        const keys = Object.keys(types).sort((a, b) => +a - +b);
        if (keys.length === 0) {
            grid.innerHTML = '<div class="v muted">no frames yet</div>';
        } else {
            grid.innerHTML = '';
            for (const k of keys) {
                const chip = document.createElement('div');
                chip.className = 'msg-chip';
                chip.innerHTML =
                    '<span class="type">' + k + '</span>' +
                    '<span class="count">' + types[k].toLocaleString() + '</span>';
                grid.appendChild(chip);
            }
        }
    }

    async function poll() {
        try {
            const r = await fetch('/api/status', { credentials: 'same-origin' });
            if (!r.ok) throw new Error('HTTP ' + r.status);
            const j = await r.json();
            hideError();
            render(j);
        } catch (e) {
            showError('Status fetch failed: ' + e.message);
        } finally {
            setTimeout(poll, POLL_MS);
        }
    }

    poll();
})();
