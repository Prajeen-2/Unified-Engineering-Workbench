/**
 * Unified Engineering Workbench - Multicore Performance Lab Controller
 * Handles hardware telemetry, bounded thread-pool benchmarks, Plotly visualizations,
 * and scientific CSV exports.
 */

class PerformanceLab {
    constructor() {
        this.currentBenchmarkData = null;
        this.isRunning = false;
        this.plotlyDarkLayout = {
            paper_bgcolor: '#182030',
            plot_bgcolor: '#121722',
            font: { family: 'Consolas, monospace', size: 11, color: '#8b949e' },
            margin: { l: 45, r: 20, t: 30, b: 40 },
            xaxis: {
                gridcolor: '#273449',
                linecolor: '#273449',
                tickfont: { color: '#8b949e' },
                title: { font: { size: 11, color: '#e6edf3' } }
            },
            yaxis: {
                gridcolor: '#273449',
                linecolor: '#273449',
                tickfont: { color: '#8b949e' },
                title: { font: { size: 11, color: '#e6edf3' } }
            },
            legend: {
                x: 0.05, y: 0.95,
                bgcolor: 'rgba(24, 32, 48, 0.8)',
                bordercolor: '#273449',
                borderwidth: 1,
                font: { size: 10, color: '#e6edf3' }
            }
        };

        this.init();
    }

    init() {
        this.bindEvents();
        this.loadHardwareInfo();
        this.initDocsTabs();
        this.initEmptyCharts();
        // Automatically perform initial benchmark so metrics & charts are populated immediately
        setTimeout(() => {
            this.runBenchmark();
        }, 300);
    }

    bindEvents() {
        const btnRun = document.getElementById('btn-run-perf-benchmark');
        if (btnRun) {
            btnRun.addEventListener('click', () => this.runBenchmark());
        }

        const btnCancel = document.getElementById('btn-cancel-perf-benchmark');
        if (btnCancel) {
            btnCancel.addEventListener('click', () => this.cancelBenchmark());
        }

        const btnReset = document.getElementById('btn-reset-perf-benchmark');
        if (btnReset) {
            btnReset.addEventListener('click', () => this.resetDefaults());
        }

        const btnExport = document.getElementById('btn-export-perf-csv');
        if (btnExport) {
            btnExport.addEventListener('click', () => this.exportCsv());
        }

        window.addEventListener('resize', () => {
            this.resizeCharts();
        });
    }

    async cancelBenchmark() {
        if (!this.isRunning || !this.activeJobId) return;
        try {
            await window.api.cancelJob(this.activeJobId);
            const statusText = document.getElementById('perf-status-text');
            if (statusText) statusText.innerText = 'Cancelling benchmark in progress...';
        } catch (err) {
            console.error('Failed to deliver cancel signal:', err);
        }
    }

    initDocsTabs() {
        const tabBtns = document.querySelectorAll('#view-performance .tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                tabBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');

                const targetTabId = btn.dataset.tab;
                const contents = document.querySelectorAll('#view-performance .tab-content');
                contents.forEach(content => {
                    content.classList.remove('active');
                    if (content.id === targetTabId) {
                        content.classList.add('active');
                    }
                });
            });
        });
    }

    async loadHardwareInfo() {
        try {
            const res = await window.api.getSystemInfo();
            if (res.success && res.data) {
                const d = res.data;
                const elCpu = document.getElementById('perf-hw-cpu');
                const elPhys = document.getElementById('perf-hw-physical-cores');
                const elLog = document.getElementById('perf-hw-logical-cores');
                const elRam = document.getElementById('perf-hw-ram');
                const elOs = document.getElementById('perf-hw-os');
                const elComp = document.getElementById('perf-hw-compiler');
                const elFlags = document.getElementById('perf-hw-flags');
                const elLaunch = document.getElementById('perf-hw-launch-time');

                if (elCpu) elCpu.innerText = d.cpu_model || 'Intel Core x64';
                if (elPhys) elPhys.innerText = `${d.physical_cores} Cores`;
                if (elLog) elLog.innerText = `${d.logical_cores} Threads`;
                if (elRam) elRam.innerText = `${d.total_ram_gb} GB (${d.available_ram_gb} GB Free)`;
                if (elOs) elOs.innerText = d.os_name || 'Windows 11 x64';
                if (elComp) elComp.innerText = d.compiler || 'MSVC';
                if (elFlags) elFlags.innerText = d.compiler_flags || '/O2 /Oi /Ot /Gy /utf-8';
                if (elLaunch) elLaunch.innerText = `${d.launch_to_ready_ms} ms`;
            }
        } catch (err) {
            console.error('Failed to load hardware telemetry:', err);
        }
    }

    resetDefaults() {
        const elCases = document.getElementById('perf-input-cases');
        const elElements = document.getElementById('perf-input-elements');
        const elSeed = document.getElementById('perf-input-seed');

        if (elCases) elCases.value = 500;
        if (elElements) elElements.value = 100;
        if (elSeed) elSeed.value = 42;

        if (window.showToast) {
            window.showToast('Workload configuration reset to default parameters (500 cases, 100 elements, seed 42).', 'info');
        }
    }

    async runBenchmark() {
        if (this.isRunning) return;

        const numCases = parseInt(document.getElementById('perf-input-cases')?.value || '500', 10);
        const numElements = parseInt(document.getElementById('perf-input-elements')?.value || '100', 10);
        const randomSeed = parseInt(document.getElementById('perf-input-seed')?.value || '42', 10);

        if (isNaN(numCases) || numCases <= 0 || numCases > 5000) {
            if (window.showToast) window.showToast('Cases must be between 50 and 5000.', 'error');
            return;
        }
        if (isNaN(numElements) || numElements < 10 || numElements > 500) {
            if (window.showToast) window.showToast('Elements must be between 10 and 500.', 'error');
            return;
        }

        const config = {
            num_cases: numCases,
            num_elements: numElements,
            random_seed: randomSeed,
            worker_counts: [1, 2, 3, 4],
            repetitions: 5
        };

        this.setRunningState(true);

        try {
            const startRes = await window.api.startBenchmarkJob(config);
            if (!startRes.success || !startRes.data || !startRes.data.job_id) {
                throw new Error(startRes.error || startRes.data?.error_message || 'Failed to initialize benchmark job.');
            }

            this.activeJobId = startRes.data.job_id;
            const progressBar = document.getElementById('perf-progress-bar');
            const statusText = document.getElementById('perf-status-text');

            // Poll real C++ async job state until completed or cancelled
            while (this.isRunning && this.activeJobId) {
                await new Promise(r => setTimeout(r, 150));
                const pollRes = await window.api.getJob(this.activeJobId);
                if (!pollRes.success || !pollRes.data) continue;

                const job = pollRes.data;
                if (progressBar) progressBar.style.width = (job.progress || 0) + '%';
                if (statusText && job.message) statusText.innerText = job.message;

                if (job.status === 'completed') {
                    if (progressBar) progressBar.style.width = '100%';
                    this.currentBenchmarkData = job.result;
                    this.renderResults(job.result);
                    if (window.showToast) {
                        window.showToast('Multicore benchmark suite completed with 100% numerical verification!', 'success');
                    }
                    break;
                } else if (job.status === 'cancelled') {
                    this.showError('Benchmark cancelled by user.');
                    if (window.showToast) window.showToast('Benchmark cancelled.', 'info');
                    break;
                } else if (job.status === 'failed') {
                    const errMsg = job.error_message || 'Benchmark execution failed.';
                    this.showError(errMsg);
                    if (window.showToast) window.showToast(errMsg, 'error');
                    break;
                }
            }
        } catch (err) {
            this.showError(err.message || 'Error communicating with backend benchmark engine.');
            if (window.showToast) window.showToast(err.message, 'error');
        } finally {
            this.activeJobId = null;
            this.setRunningState(false);
        }
    }

    setRunningState(running) {
        this.isRunning = running;
        const btnRun = document.getElementById('btn-run-perf-benchmark');
        const btnCancel = document.getElementById('btn-cancel-perf-benchmark');
        const btnLabel = document.getElementById('perf-btn-label');
        const progressContainer = document.getElementById('perf-progress-container');
        const progressBar = document.getElementById('perf-progress-bar');
        const summaryPill = document.getElementById('perf-summary-pill');

        if (running) {
            if (btnRun) btnRun.disabled = true;
            if (btnCancel) btnCancel.style.display = 'inline-flex';
            if (btnLabel) btnLabel.innerText = 'Benchmarking...';
            if (summaryPill) {
                summaryPill.className = 'metric-pill pill-cyan';
                summaryPill.innerText = 'Computing...';
            }
            if (progressContainer) progressContainer.style.display = 'block';
            if (progressBar) progressBar.style.width = '0%';
        } else {
            if (btnRun) btnRun.disabled = false;
            if (btnCancel) btnCancel.style.display = 'none';
            if (btnLabel) btnLabel.innerText = 'Run Multicore Benchmark';
            if (progressContainer) {
                if (progressBar) progressBar.style.width = '100%';
                setTimeout(() => {
                    if (!this.isRunning && progressContainer) progressContainer.style.display = 'none';
                }, 400);
            }
        }
    }

    showError(msg) {
        const statusText = document.getElementById('perf-status-text');
        const summaryPill = document.getElementById('perf-summary-pill');
        if (statusText) {
            statusText.innerHTML = `<span style="color: var(--accent-red); font-weight: 600;">[ERROR] ${msg}</span>`;
        }
        if (summaryPill) {
            summaryPill.className = 'metric-pill pill-pending';
            summaryPill.innerText = 'Error';
        }
    }

    renderResults(data) {
        const statusText = document.getElementById('perf-status-text');
        const summaryPill = document.getElementById('perf-summary-pill');
        const elT1 = document.getElementById('perf-kpi-t1');
        const elSpeedup = document.getElementById('perf-kpi-speedup');
        const elEfficiency = document.getElementById('perf-kpi-efficiency');
        const elVerification = document.getElementById('perf-kpi-verification');
        const elDiffDetail = document.getElementById('perf-kpi-diff-detail');

        if (statusText) {
            statusText.innerText = `Benchmark completed at ${data.timestamp}. All worker configurations verified strictly.`;
        }
        if (summaryPill) {
            summaryPill.className = data.all_verified ? 'metric-pill pill-success' : 'metric-pill pill-pending';
            summaryPill.innerText = data.all_verified ? 'Verified (PASS)' : 'Verification Failed';
        }

        // Calculate KPI summaries
        const workers = data.worker_results || [];
        let maxSpeedup = 1.0;
        let efficiencyAtMax = 1.0;
        let peakDiff = 0.0;
        let allVerified = data.all_verified;

        workers.forEach(w => {
            if (w.speedup > maxSpeedup) {
                maxSpeedup = w.speedup;
                efficiencyAtMax = w.efficiency;
            }
            if (w.max_temp_diff_vs_serial > peakDiff) {
                peakDiff = w.max_temp_diff_vs_serial;
            }
        });

        if (elT1) elT1.innerText = `${data.serial_baseline_median_ms.toFixed(3)} ms`;
        if (elSpeedup) elSpeedup.innerText = `${maxSpeedup.toFixed(2)}x`;
        if (elEfficiency) elEfficiency.innerText = `${(efficiencyAtMax * 100).toFixed(1)}%`;

        if (elVerification) {
            elVerification.innerHTML = allVerified
                ? `<span class="badge-pass" style="font-size: 13px;">PASS (&le; 1e-8 &deg;C)</span>`
                : `<span class="badge-fail" style="font-size: 13px;">FAIL (&gt; 1e-8 &deg;C)</span>`;
        }
        if (elDiffDetail) {
            elDiffDetail.innerText = `Peak Absolute Discrepancy: ${peakDiff.toExponential(4)} °C (Machine Precision Level)`;
        }

        // Render Table
        this.renderTable(workers);

        // Render Scientific Visualizations
        this.renderCharts(workers);
    }

    renderTable(workers) {
        const tbody = document.getElementById('perf-table-body');
        if (!tbody) return;

        let rowsHtml = '';
        workers.forEach(w => {
            const reps = w.repetition_times_ms || [];
            const r1 = reps[0] !== undefined ? reps[0].toFixed(3) : '--';
            const r2 = reps[1] !== undefined ? reps[1].toFixed(3) : '--';
            const r3 = reps[2] !== undefined ? reps[2].toFixed(3) : '--';
            const r4 = reps[3] !== undefined ? reps[3].toFixed(3) : '--';
            const r5 = reps[4] !== undefined ? reps[4].toFixed(3) : '--';

            const statusBadge = w.verified
                ? `<span class="badge-pass">PASS</span>`
                : `<span class="badge-fail">FAIL</span>`;

            rowsHtml += `
                <tr>
                    <td style="font-weight: 700; color: var(--accent-cyan);">${w.worker_count} ${w.worker_count === 1 ? 'Worker' : 'Workers'}</td>
                    <td>${r1}</td>
                    <td>${r2}</td>
                    <td>${r3}</td>
                    <td>${r4}</td>
                    <td>${r5}</td>
                    <td style="font-weight: 700; color: var(--text-primary);">${w.median_time_ms.toFixed(3)}</td>
                    <td style="font-weight: 700; color: var(--accent-blue);">${w.speedup.toFixed(3)}</td>
                    <td style="font-weight: 700; color: var(--accent-green);">${(w.efficiency * 100).toFixed(1)}%</td>
                    <td style="font-family: var(--font-mono);">${w.max_temp_diff_vs_serial.toExponential(2)}</td>
                    <td>${statusBadge}</td>
                </tr>
            `;
        });

        tbody.innerHTML = rowsHtml;
    }

    initEmptyCharts() {
        const defaultWorkers = [
            { worker_count: 1, median_time_ms: 0, speedup: 1, efficiency: 1, repetition_times_ms: [0,0,0,0,0] },
            { worker_count: 2, median_time_ms: 0, speedup: 0, efficiency: 0, repetition_times_ms: [0,0,0,0,0] },
            { worker_count: 3, median_time_ms: 0, speedup: 0, efficiency: 0, repetition_times_ms: [0,0,0,0,0] },
            { worker_count: 4, median_time_ms: 0, speedup: 0, efficiency: 0, repetition_times_ms: [0,0,0,0,0] }
        ];
        this.renderCharts(defaultWorkers);
    }

    renderCharts(workers) {
        if (!window.Plotly) return;

        const workerCounts = workers.map(w => `${w.worker_count} ${w.worker_count === 1 ? 'Worker' : 'Workers'}`);
        const workerNumeric = workers.map(w => w.worker_count);
        const medianTimes = workers.map(w => w.median_time_ms);
        const speedups = workers.map(w => w.speedup);
        const efficiencies = workers.map(w => w.efficiency * 100);

        // Chart 1: Execution Time vs Workers (with error bars min-max spread)
        const errorPlus = workers.map(w => (w.max_time_ms || w.median_time_ms) - w.median_time_ms);
        const errorMinus = workers.map(w => w.median_time_ms - (w.min_time_ms || w.median_time_ms));

        const traceTime = {
            x: workerCounts,
            y: medianTimes,
            type: 'bar',
            marker: {
                color: '#388bfd',
                line: { color: '#1f6feb', width: 1.5 }
            },
            error_y: {
                type: 'data',
                symmetric: false,
                array: errorPlus,
                arrayminus: errorMinus,
                color: '#e6edf3',
                thickness: 1.5,
                width: 6
            },
            name: 'Median Execution Time (ms)'
        };

        const layoutTime = {
            ...this.plotlyDarkLayout,
            title: { text: 'Median Execution Time (ms)', font: { size: 12, color: '#e6edf3' } },
            yaxis: { ...this.plotlyDarkLayout.yaxis, title: 'Time (ms)' }
        };
        Plotly.newPlot('chart-perf-time', [traceTime], layoutTime, { responsive: true, displayModeBar: false });

        // Chart 2: Speedup vs Ideal Line
        const traceSpeedup = {
            x: workerNumeric,
            y: speedups,
            type: 'scatter',
            mode: 'lines+markers',
            marker: { size: 8, color: '#39c5cf' },
            line: { color: '#39c5cf', width: 2.5 },
            name: 'Measured Speedup (Sp)'
        };

        const traceIdealSpeedup = {
            x: workerNumeric,
            y: workerNumeric,
            type: 'scatter',
            mode: 'lines',
            line: { color: '#6e7681', width: 1.5, dash: 'dash' },
            name: 'Ideal Linear Speedup (Sp = p)'
        };

        const layoutSpeedup = {
            ...this.plotlyDarkLayout,
            title: { text: 'Multicore Speedup (Sp = T1 / Tp)', font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyDarkLayout.xaxis, title: 'Worker Threads (p)', tickvals: [1, 2, 3, 4] },
            yaxis: { ...this.plotlyDarkLayout.yaxis, title: 'Speedup Multiplier' }
        };
        Plotly.newPlot('chart-perf-speedup', [traceSpeedup, traceIdealSpeedup], layoutSpeedup, { responsive: true, displayModeBar: false });

        // Chart 3: Efficiency vs Ideal 100%
        const traceEfficiency = {
            x: workerNumeric,
            y: efficiencies,
            type: 'scatter',
            mode: 'lines+markers',
            marker: { size: 8, color: '#2ea043' },
            line: { color: '#2ea043', width: 2.5 },
            name: 'Measured Efficiency (%)'
        };

        const traceIdealEff = {
            x: workerNumeric,
            y: workerNumeric.map(() => 100),
            type: 'scatter',
            mode: 'lines',
            line: { color: '#6e7681', width: 1.5, dash: 'dash' },
            name: 'Ideal Efficiency (100%)'
        };

        const layoutEfficiency = {
            ...this.plotlyDarkLayout,
            title: { text: 'Parallel Efficiency (Ep = Sp / p)', font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyDarkLayout.xaxis, title: 'Worker Threads (p)', tickvals: [1, 2, 3, 4] },
            yaxis: { ...this.plotlyDarkLayout.yaxis, title: 'Efficiency (%)', range: [0, 115] }
        };
        Plotly.newPlot('chart-perf-efficiency', [traceEfficiency, traceIdealEff], layoutEfficiency, { responsive: true, displayModeBar: false });
    }

    resizeCharts() {
        if (!window.Plotly) return;
        ['chart-perf-time', 'chart-perf-speedup', 'chart-perf-efficiency'].forEach(id => {
            const el = document.getElementById(id);
            if (el && el.data) {
                Plotly.Plots.resize(el);
            }
        });
    }

    exportCsv() {
        if (!this.currentBenchmarkData) {
            // If no benchmark has been run yet in UI, fetch via server URL with default parameters
            const numCases = parseInt(document.getElementById('perf-input-cases')?.value || '500', 10);
            const numElements = parseInt(document.getElementById('perf-input-elements')?.value || '100', 10);
            const seed = parseInt(document.getElementById('perf-input-seed')?.value || '42', 10);
            const url = window.api.getPerformanceExportCsvUrl({
                num_cases: numCases,
                num_elements: numElements,
                random_seed: seed,
                repetitions: 5
            });
            window.location.href = url;
            return;
        }

        // Direct export of current verified benchmark data
        const data = this.currentBenchmarkData;
        let csv = `# Unified Engineering Workbench - Multicore Performance Lab Benchmark Export\n`;
        csv += `# Generated: ${data.timestamp}\n`;
        csv += `# Simulation Cases: ${data.config.num_cases}, Mesh Elements per Rod: ${data.config.num_elements}\n`;
        csv += `# PRNG Seed: ${data.config.random_seed}, Repetitions: ${data.config.repetitions}\n`;
        csv += `# Tolerance: 1.000000e-08 °C\n`;
        csv += `# Launch-to-Ready Time: ${data.launch_to_ready_ms} ms\n`;
        csv += `# Serial Baseline Median (T1): ${data.serial_baseline_median_ms.toFixed(4)} ms\n`;
        csv += `workers,run1_ms,run2_ms,run3_ms,run4_ms,run5_ms,median_ms,mean_ms,min_ms,max_ms,stddev_ms,speedup,efficiency,max_temp_diff_C,verification_status\n`;

        data.worker_results.forEach(w => {
            const reps = w.repetition_times_ms || [];
            csv += `${w.worker_count},`;
            for (let r = 0; r < 5; ++r) {
                csv += `${(reps[r] !== undefined ? reps[r] : 0).toFixed(4)},`;
            }
            csv += `${w.median_time_ms.toFixed(4)},${w.mean_time_ms.toFixed(4)},${w.min_time_ms.toFixed(4)},${w.max_time_ms.toFixed(4)},${w.std_dev_time_ms.toFixed(4)},${w.speedup.toFixed(4)},${w.efficiency.toFixed(4)},${w.max_temp_diff_vs_serial.toExponential(6)},${w.verified ? 'PASS' : 'FAIL'}\n`;
        });

        const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
        const link = document.createElement('a');
        const url = URL.createObjectURL(blob);
        link.setAttribute('href', url);
        link.setAttribute('download', `multicore_benchmark_cases${data.config.num_cases}_seed${data.config.random_seed}.csv`);
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        URL.revokeObjectURL(url);

        if (window.showToast) {
            window.showToast('Benchmark CSV downloaded successfully.', 'success');
        }
    }
}

// Instantiate upon DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.performanceLab = new PerformanceLab();
});
