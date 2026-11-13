/**
 * Unified Engineering Workbench - Reports & Exports Controller
 * Task G: Deliverables Download Hub, System Logging & Executive Documentation
 */

class ReportsExportsController {
    constructor() {
        this.init();
    }

    init() {
        this.bindDownloadButtons();
        this.initTabs();
        this.bindLogControls();
        this.fetchLogs();
    }

    triggerDownload(url, filename) {
        const link = document.createElement('a');
        link.href = url;
        if (filename) link.download = filename;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        if (window.showToast) {
            window.showToast(`Downloading deliverable: ${filename || 'file'}...`, 'info');
        }
    }

    bindDownloadButtons() {
        // 1. Thermal Nodal Results CSV
        document.getElementById('btn-rep-thermal-results')?.addEventListener('click', () => {
            this.triggerDownload(`${window.api.baseUrl}/api/thermal/export-csv?type=temperature`, 'thermal_temperature_results.csv');
        });

        // 2. Thermal Convergence CSV
        document.getElementById('btn-rep-thermal-conv')?.addEventListener('click', () => {
            this.triggerDownload(`${window.api.baseUrl}/api/thermal/export-csv?type=convergence`, 'thermal_mesh_convergence.csv');
        });

        // 3. Multicore Performance Benchmark CSV
        document.getElementById('btn-rep-perf-bench')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getPerformanceExportCsvUrl(), 'multicore_benchmark_results.csv');
        });

        // 4. Hardware Pin Architecture CSV
        document.getElementById('btn-rep-hw-pins')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getHardwareExportCsvUrl(), 'hardware_pin_architecture.csv');
        });

        // 5. Neural Backprop Training Log CSV
        document.getElementById('btn-rep-neural-log')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getNeuralExportCsvUrl(), 'neural_backprop_training_log.csv');
        });

        // 6. Manual Backpropagation Markdown Doc
        document.getElementById('btn-rep-doc-backprop')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getReportDocumentUrl('MANUAL_BACKPROP_DERIVATION.md'), 'MANUAL_BACKPROP_DERIVATION.md');
        });

        // 7. Hardware Architecture Markdown Doc
        document.getElementById('btn-rep-doc-hw')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getReportDocumentUrl('HARDWARE_INTEGRATION.md'), 'HARDWARE_INTEGRATION.md');
        });

        // 8. Performance Lab Markdown Doc
        document.getElementById('btn-rep-doc-perf')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getReportDocumentUrl('PERFORMANCE_LAB.md'), 'PERFORMANCE_LAB.md');
        });
    }

    initTabs() {
        const tabBtns = document.querySelectorAll('#view-reports .tabs-header .tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                tabBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');

                const targetTabId = btn.dataset.tab;
                const contents = document.querySelectorAll('#view-reports .tab-content');
                contents.forEach(content => {
                    content.classList.remove('active');
                    if (content.id === targetTabId) {
                        content.classList.add('active');
                    }
                });
            });
        });
    }

    bindLogControls() {
        document.getElementById('btn-refresh-logs')?.addEventListener('click', () => {
            this.fetchLogs();
            if (window.showToast) window.showToast('Server diagnostics log refreshed.', 'info');
        });

        document.getElementById('btn-export-logs')?.addEventListener('click', () => {
            this.triggerDownload(window.api.getLogsExportUrl(), 'workbench_system_logs.log');
        });
    }

    async fetchLogs() {
        const container = document.getElementById('rep-log-container');
        if (!container) return;

        try {
            const res = await window.api.getLogs(100);
            if (res.success && res.data && res.data.logs) {
                let html = '';
                res.data.logs.forEach(l => {
                    let lvlColor = 'var(--accent-blue)';
                    if (l.level === 'WARN') lvlColor = 'var(--accent-amber)';
                    if (l.level === 'ERROR') lvlColor = 'var(--accent-red)';
                    if (l.level === 'DEBUG') lvlColor = 'var(--text-muted)';

                    html += `
                        <div style="margin-bottom: 3px; line-height: 1.4;">
                            <span style="color: var(--text-muted); font-size: 10px;">[${l.timestamp}]</span>
                            <span style="color: ${lvlColor}; font-weight: 700; margin: 0 4px;">[${l.level}]</span>
                            <span style="color: var(--text-primary);">${this.escapeHtml(l.message)}</span>
                        </div>
                    `;
                });
                container.innerHTML = html || '<div style="color: var(--text-muted);">No log entries recorded.</div>';
                container.scrollTop = container.scrollHeight;
            } else {
                container.innerHTML = '<div style="color: var(--accent-amber);">Could not retrieve backend logs from REST server.</div>';
            }
        } catch (err) {
            container.innerHTML = `<div style="color: var(--accent-red);">Logging IPC Error: ${this.escapeHtml(err.message)}</div>`;
        }
    }

    escapeHtml(text) {
        if (!text) return '';
        return text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
    }
}

// Instantiate on DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.reportsExports = new ReportsExportsController();
});

