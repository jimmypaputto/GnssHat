/* System tab — UBX-MON-SYS receiver health + UBX-MON-VER firmware identity. */
(function () {
    'use strict';

    // BootType IntEnum mirror (jp_gnss_module bindings)
    const BOOT_TYPE_LABELS = {
        0: 'Unknown',
        1: 'Cold start',
        2: 'Watchdog reset',
        3: 'Hardware reset',
        4: 'Hardware backup',
        5: 'Software backup',
        6: 'Software reset',
        7: 'VIO supply fail',
        8: 'VDDx supply fail',
        9: 'VDD_RF supply fail',
        10: 'V_CORE high fail',
        11: 'System reset',
    };

    function fmtRunTime(seconds) {
        if (!Number.isFinite(seconds) || seconds < 0) return '—';
        const d = Math.floor(seconds / 86400);
        const h = Math.floor((seconds % 86400) / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;
        const parts = [];
        if (d) parts.push(`${d}d`);
        if (d || h) parts.push(`${h}h`);
        if (d || h || m) parts.push(`${m}m`);
        parts.push(`${s}s`);
        return parts.join(' ');
    }

    function loadClass(pct) {
        if (!Number.isFinite(pct)) return 'load-idle';
        if (pct >= 85) return 'load-critical';
        if (pct >= 60) return 'load-warn';
        return 'load-ok';
    }

    function setStat(id, value, max) {
        const root = document.getElementById(id);
        if (!root) return;
        const valEl = root.querySelector('.sys-val');
        const maxEl = root.querySelector('.sys-max');
        const fill  = root.querySelector('.system-stat-bar-fill');

        if (valEl) valEl.textContent = Number.isFinite(value) ? value : '—';
        if (maxEl) maxEl.textContent = Number.isFinite(max) ? max : '—';

        root.classList.remove('load-ok', 'load-warn', 'load-critical', 'load-idle');
        root.classList.add(loadClass(value));

        if (fill) {
            const pct = Math.max(0, Math.min(100, Number(value) || 0));
            fill.style.width = pct + '%';
        }
    }

    function setText(id, value) {
        const el = document.getElementById(id);
        if (el) el.textContent = value;
    }

    class SystemPanel {
        constructor() {
            this.lastSystem = null;   // last MON-SYS payload
            this.lastVersion = null;  // cached MON-VER payload
        }

        update(payload) {
            if (!payload) return;
            if (payload.system) this.lastSystem = payload.system;
            if (payload.version) this.lastVersion = payload.version;
            if (window.activeMapTab === 'system') this.render();
        }

        render() {
            this.renderHealth();
            this.renderVersion();
        }

        renderHealth() {
            const s = this.lastSystem;
            if (!s) return;

            setStat('sys-cpu',  s.cpu_load,    s.cpu_load_max);
            setStat('sys-mem',  s.mem_usage,   s.mem_usage_max);
            setStat('sys-io',   s.io_usage,    s.io_usage_max);

            // Temperature card has no max/bar.
            const temp = document.getElementById('sys-temp');
            if (temp) {
                const valEl = temp.querySelector('.sys-val');
                if (valEl) {
                    valEl.textContent = Number.isFinite(s.temperature_c)
                        ? s.temperature_c : '—';
                }
                temp.classList.remove(
                    'load-ok', 'load-warn', 'load-critical', 'load-idle');
                if (Number.isFinite(s.temperature_c)) {
                    if (s.temperature_c >= 80) temp.classList.add('load-critical');
                    else if (s.temperature_c >= 60) temp.classList.add('load-warn');
                    else temp.classList.add('load-ok');
                }
            }

            setText('system-runtime',
                Number.isFinite(s.run_time_s)
                    ? `up ${fmtRunTime(s.run_time_s)}`
                    : '—');
            setText('sys-boot-type',
                BOOT_TYPE_LABELS[s.boot_type] !== undefined
                    ? `${BOOT_TYPE_LABELS[s.boot_type]} (${s.boot_type})`
                    : `Unknown (${s.boot_type})`);
            setText('sys-notice-count', s.notice_count ?? 0);
            setText('sys-warn-count', s.warn_count ?? 0);
            setText('sys-error-count', s.error_count ?? 0);
            setText('sys-msg-version', s.msg_version ?? '—');

            // Highlight non-zero error/warn counts.
            const warnRow = document.getElementById('sys-warn-count');
            const errRow  = document.getElementById('sys-error-count');
            if (warnRow) warnRow.classList.toggle('non-zero', (s.warn_count || 0) > 0);
            if (errRow)  errRow.classList.toggle('critical', (s.error_count || 0) > 0);
        }

        renderVersion() {
            const v = this.lastVersion;
            if (!v) return;

            setText('sys-sw-version', v.sw_version || '—');
            setText('sys-hw-version', v.hw_version || '—');

            const list = document.getElementById('sys-extensions');
            if (!list) return;
            list.innerHTML = '';
            const exts = Array.isArray(v.extensions) ? v.extensions : [];
            if (exts.length === 0) {
                const li = document.createElement('li');
                li.className = 'sys-no-data';
                li.textContent = 'No extensions reported';
                list.appendChild(li);
                return;
            }
            for (const ext of exts) {
                const li = document.createElement('li');
                li.textContent = ext;
                list.appendChild(li);
            }
        }
    }

    function init() {
        if (typeof socket === 'undefined' || !socket) {
            // Socket not yet available — defer.
            setTimeout(init, 100);
            return;
        }
        const panel = new SystemPanel();
        window.systemPanel = panel;
        socket.on('system_update', (payload) => panel.update(payload));
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }
})();
