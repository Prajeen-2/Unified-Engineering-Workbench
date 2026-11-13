/**
 * Unified Engineering Workbench - Application Controller
 * Handles view switching, telemetry polling, logging, and notifications
 */

document.addEventListener('DOMContentLoaded', () => {
    // Navigation elements
    const navItems = document.querySelectorAll('.nav-item');
    const viewPanels = document.querySelectorAll('.view-panel');
    const moduleTitle = document.getElementById('current-module-title');
    const statusDot = document.getElementById('status-dot');
    const statusText = document.getElementById('status-text');
    const pingText = document.getElementById('status-ping');
    const btnShutdown = document.getElementById('btn-shutdown');
    const logConsole = document.getElementById('log-terminal-output');

    let currentModule = 'dashboard';
    let pollTimer = null;

    // View Navigation Controller
    window.switchModule = (targetModule) => {
        const item = document.querySelector(`.nav-item[data-module="${targetModule}"]`);
        if (!item) return;

        // Update sidebar state
        navItems.forEach(n => n.classList.remove('active'));
        item.classList.add('active');

        // Update views
        viewPanels.forEach(panel => {
            if (panel.id === `view-${targetModule}`) {
                panel.classList.add('active');
            } else {
                panel.classList.remove('active');
            }
        });

        // Update top bar title
        const label = item.querySelector('.nav-label')?.innerText || targetModule;
        moduleTitle.innerText = label;
        currentModule = targetModule;

        if (targetModule === 'geometry' && window.geometryViewer) {
            window.geometryViewer.onWindowResize();
        }
        if (targetModule === 'thermal' && window.thermalAnalysis) {
            window.thermalAnalysis.resizeCharts();
        }
        if (targetModule === 'performance' && window.performanceLab) {
            window.performanceLab.resizeCharts();
        }
        if (targetModule === 'hardware' && window.hardwareIntegration) {
            window.hardwareIntegration.resizeCharts();
        }
    };

    navItems.forEach(item => {
        item.addEventListener('click', (e) => {
            e.preventDefault();
            const targetModule = item.dataset.module;
            if (!targetModule) return;
            window.switchModule(targetModule);
            window.location.hash = targetModule;
        });
    });

    if (window.location.hash) {
        const initial = window.location.hash.replace('#', '');
        window.switchModule(initial);
    }

    // Toast Notification System
    window.showToast = (message, type = 'info') => {
        const container = document.getElementById('toast-container');
        if (!container) return;

        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.innerText = message;
        container.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transition = 'opacity 0.3s ease';
            setTimeout(() => toast.remove(), 300);
        }, 3500);
    };

    // Telemetry and Health Polling
    async function updateTelemetry() {
        const healthRes = await window.api.getHealth();
        if (healthRes.success) {
            statusDot.classList.remove('disconnected');
            statusText.innerText = 'ONLINE';
            pingText.innerText = `${healthRes.latency} ms`;
            document.getElementById('dash-server-status').innerText = 'Online / Ready';
            document.getElementById('dash-server-status').className = 'metric-pill pill-success';
        } else {
            statusDot.classList.add('disconnected');
            statusText.innerText = 'OFFLINE';
            pingText.innerText = '-- ms';
            document.getElementById('dash-server-status').innerText = 'Disconnected';
            document.getElementById('dash-server-status').className = 'metric-pill pill-pending';
            return;
        }

        const infoRes = await window.api.getSystemInfo();
        if (infoRes.success && infoRes.data) {
            const data = infoRes.data;
            document.getElementById('dash-cpu-model').innerText = data.cpu_model || 'N/A';
            document.getElementById('dash-core-counts').innerText = `${data.physical_cores} Physical / ${data.logical_cores} Logical`;
            document.getElementById('dash-ram-metrics').innerText = `${data.available_ram_gb} GB / ${data.total_ram_gb} GB`;
            document.getElementById('dash-compiler').innerText = data.compiler || 'MSVC x64';
            document.getElementById('dash-build-type').innerText = data.build_type || 'Release';
            document.getElementById('dash-uptime').innerText = formatUptime(data.uptime_seconds);
            document.getElementById('sidebar-host-info').innerText = `${data.architecture} | ${data.os_name}`;
        }

        // Update logs if on dashboard
        if (currentModule === 'dashboard') {
            const logsRes = await window.api.getLogs(60);
            if (logsRes.success && logsRes.data && logsRes.data.logs) {
                renderLogs(logsRes.data.logs);
            }
        }
    }

    function formatUptime(seconds) {
        if (!seconds) return '0s';
        const mins = Math.floor(seconds / 60);
        const secs = seconds % 60;
        const hrs = Math.floor(mins / 60);
        if (hrs > 0) return `${hrs}h ${mins % 60}m ${secs}s`;
        if (mins > 0) return `${mins}m ${secs}s`;
        return `${secs}s`;
    }

    function renderLogs(logs) {
        if (!logConsole) return;
        const isScrolledToBottom = logConsole.scrollHeight - logConsole.clientHeight <= logConsole.scrollTop + 20;

        logConsole.innerHTML = '';
        logs.forEach(entry => {
            const row = document.createElement('div');
            row.className = 'log-line';

            const ts = document.createElement('span');
            ts.className = 'log-ts';
            ts.innerText = `[${entry.timestamp.split(' ')[1] || entry.timestamp}]`;

            const lvl = document.createElement('span');
            lvl.className = `log-level log-level-${entry.level}`;
            lvl.innerText = `[${entry.level}]`;

            const msg = document.createElement('span');
            msg.className = 'log-msg';
            msg.innerText = entry.message;

            row.appendChild(ts);
            row.appendChild(lvl);
            row.appendChild(msg);
            logConsole.appendChild(row);
        });

        if (isScrolledToBottom) {
            logConsole.scrollTop = logConsole.scrollHeight;
        }
    }

    // Safe Shutdown Trigger
    btnShutdown.addEventListener('click', async () => {
        const confirmed = confirm('Are you sure you want to shut down the Unified Engineering Workbench C++ backend?');
        if (!confirmed) return;

        showToast('Initiating safe server termination...', 'info');
        const res = await window.api.shutdown();
        if (res.success) {
            showToast('Backend server shut down successfully.', 'success');
            statusDot.classList.add('disconnected');
            statusText.innerText = 'STOPPED';
            clearInterval(pollTimer);
        } else {
            showToast('Shutdown request failed or already stopped.', 'error');
        }
    });

    // Start background polling
    updateTelemetry();
    pollTimer = setInterval(updateTelemetry, 2000);
});

