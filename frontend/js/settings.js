/**
 * Unified Engineering Workbench - Settings & Diagnostics Controller
 * Module 8: Settings, Host Diagnostics & Process Lifecycle Management
 */

class SettingsController {
    constructor() {
        this.diagnosticsData = null;
        this.init();
    }

    init() {
        this.bindEvents();
        this.loadSettings();
    }

    bindEvents() {
        // Ping Server Button
        document.getElementById('btn-set-ping-server')?.addEventListener('click', () => this.pingServer());

        // Copy Diagnostics JSON
        document.getElementById('btn-set-copy-diag')?.addEventListener('click', () => this.copyDiagnostics());

        // Save Compute Presets
        document.getElementById('btn-set-save-solver')?.addEventListener('click', () => this.saveComputePresets());

        // Reset Compute Defaults
        document.getElementById('btn-set-reset-solver')?.addEventListener('click', () => this.resetComputeDefaults());

        // Apply Viewport Presets
        document.getElementById('btn-set-save-viewport')?.addEventListener('click', () => this.applyViewportPresets());

        // Clear Logs
        document.getElementById('btn-set-clear-logs')?.addEventListener('click', () => this.clearLogs());

        // Download Logs
        document.getElementById('btn-set-export-logs')?.addEventListener('click', () => this.exportLogs());

        // Shutdown Server
        document.getElementById('btn-set-shutdown-server')?.addEventListener('click', () => this.shutdownServer());
    }

    async loadSettings() {
        try {
            const res = await window.api.getSettings();
            if (res.success && res.data) {
                this.diagnosticsData = res.data;
                this.populateUI(res.data);
            }
        } catch (err) {
            console.error('Failed to load settings diagnostics:', err);
        }
    }

    populateUI(data) {
        const hw = data.host_hardware || {};
        const tc = data.toolchain || {};
        const srv = data.server || {};

        if (hw.cpu_model) document.getElementById('set-cpu-model').innerText = hw.cpu_model;
        if (hw.physical_cores && hw.logical_cores) {
            document.getElementById('set-core-counts').innerText = `${hw.physical_cores} Physical / ${hw.logical_cores} Logical Cores`;
        }
        if (hw.total_ram_mb) {
            document.getElementById('set-ram-info').innerText = `${(hw.total_ram_mb / 1024).toFixed(1)} GB Total (${(hw.available_ram_mb / 1024).toFixed(1)} GB Available)`;
        }
        if (hw.os_name) document.getElementById('set-os-name').innerText = hw.os_name;
        if (tc.compiler) document.getElementById('set-compiler').innerText = tc.compiler;
        if (srv.launch_latency_ms !== undefined) {
            document.getElementById('set-launch-latency').innerText = `${srv.launch_latency_ms.toFixed(1)} ms`;
        }
        if (srv.base_url) document.getElementById('set-server-url').innerText = srv.base_url;
        if (srv.static_dir) document.getElementById('set-static-dir').innerText = srv.static_dir;
        if (srv.uptime_seconds !== undefined) {
            document.getElementById('set-uptime').innerText = `${srv.uptime_seconds}s`;
        }
    }

    async pingServer() {
        const latEl = document.getElementById('set-ipc-latency');
        const pill = document.getElementById('settings-status-pill');
        if (latEl) latEl.innerText = 'Pinging...';

        try {
            const res = await window.api.getHealth();
            if (res.success) {
                if (latEl) latEl.innerText = `${res.latency} ms (Round-Trip)`;
                if (pill) {
                    pill.className = 'metric-pill pill-success';
                    pill.innerText = `System Operational • Online (${res.latency} ms)`;
                }
                window.showToast(`Server Ping OK: Round-trip IPC latency is ${res.latency} ms`, 'success');
            } else {
                if (latEl) latEl.innerText = 'Unreachable';
                if (pill) {
                    pill.className = 'metric-pill pill-error';
                    pill.innerText = 'Server Unreachable • Offline';
                }
                window.showToast('Server ping timed out or offline.', 'error');
            }
        } catch (err) {
            if (latEl) latEl.innerText = 'Error';
            window.showToast(`Ping error: ${err.message}`, 'error');
        }
    }

    copyDiagnostics() {
        if (!this.diagnosticsData) {
            window.showToast('Diagnostics metadata not loaded yet.', 'error');
            return;
        }

        const jsonStr = JSON.stringify(this.diagnosticsData, null, 2);
        navigator.clipboard.writeText(jsonStr).then(() => {
            window.showToast('System diagnostics JSON copied to clipboard.', 'success');
        }).catch(() => {
            window.showToast('Failed to copy to clipboard.', 'error');
        });
    }

    saveComputePresets() {
        const elements = document.getElementById('set-def-elements')?.value || '40';
        const seed = document.getElementById('set-def-seed')?.value || '42';
        const cases = document.getElementById('set-def-cases')?.value || '500';

        // Apply to thermal inputs if present
        const thermalElementsInput = document.getElementById('thermal-mesh-select');
        if (thermalElementsInput) thermalElementsInput.value = elements;

        // Apply to performance inputs if present
        const perfCasesInput = document.getElementById('perf-input-cases');
        if (perfCasesInput) perfCasesInput.value = cases;
        const perfSeedInput = document.getElementById('perf-input-seed');
        if (perfSeedInput) perfSeedInput.value = seed;

        window.showToast(`Saved compute presets (Elements: ${elements}, PRNG Seed: ${seed}, Cases: ${cases}).`, 'success');
    }

    resetComputeDefaults() {
        document.getElementById('set-def-elements').value = '40';
        document.getElementById('set-def-seed').value = '42';
        document.getElementById('set-def-cases').value = '500';
        document.getElementById('set-def-eps').value = '1.0e-6';
        document.getElementById('set-def-tol').value = '1.0e-5';

        this.saveComputePresets();
        window.showToast('Restored all numerical compute presets to assignment defaults.', 'info');
    }

    applyViewportPresets() {
        const opacity = parseFloat(document.getElementById('set-def-cad-opacity')?.value || '85');
        const pointSize = parseFloat(document.getElementById('set-def-point-size')?.value || '10');
        const colormap = document.getElementById('set-def-colormap')?.value || 'turbo';

        // Apply to Geometry Viewer if instantiated
        if (window.geometryViewer) {
            if (window.geometryViewer.setMeshOpacity) {
                window.geometryViewer.setMeshOpacity(opacity / 100.0);
            }
            if (window.geometryViewer.setPointSize) {
                window.geometryViewer.setPointSize(pointSize);
            }
            if (window.geometryViewer.setColormap) {
                window.geometryViewer.setColormap(colormap);
            }
        }

        // Sync viewport sliders
        const sliderOpacity = document.getElementById('slider-mesh-opacity');
        if (sliderOpacity) sliderOpacity.value = opacity;
        const sliderPoint = document.getElementById('slider-point-size');
        if (sliderPoint) sliderPoint.value = pointSize;
        const selectColormap = document.getElementById('select-colormap');
        if (selectColormap) selectColormap.value = colormap;

        window.showToast(`Applied 3D Viewport presets (Opacity: ${opacity}%, Point Size: ${pointSize}px, Colormap: ${colormap}).`, 'success');
    }

    async clearLogs() {
        try {
            const res = await window.api.clearLogs();
            if (res.success) {
                window.showToast('In-memory log ring buffer cleared successfully.', 'info');
                // If reports logs viewer is present, refresh it
                if (window.reportsExports) {
                    window.reportsExports.fetchLogs();
                }
            }
        } catch (err) {
            window.showToast(`Failed to clear logs: ${err.message}`, 'error');
        }
    }

    exportLogs() {
        const link = document.createElement('a');
        link.href = window.api.getLogsExportUrl();
        link.download = 'workbench_system_logs.log';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        window.showToast('Downloading system diagnostics log...', 'info');
    }

    async shutdownServer() {
        if (!confirm('Are you sure you want to shut down the Unified Engineering Workbench server?\n\nTo restart afterward, double-click Launch.exe in the project root directory.')) {
            return;
        }

        const pill = document.getElementById('settings-status-pill');
        if (pill) {
            pill.className = 'metric-pill pill-error';
            pill.innerText = 'Shutting Down...';
        }

        try {
            await window.api.shutdown();
            window.showToast('Server termination initiated. Double-click Launch.exe to restart.', 'warning');
            setTimeout(() => {
                if (pill) pill.innerText = 'Server Terminated / Offline';
            }, 1000);
        } catch (err) {
            window.showToast(`Shutdown request sent: ${err.message}`, 'info');
        }
    }
}

// Instantiate on DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.settings = new SettingsController();
});
