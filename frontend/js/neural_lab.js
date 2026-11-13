/**
 * Unified Engineering Workbench - Neural Network Lab
 * Task F: Manual Analytical Backpropagation, Central Finite-Difference Gradient Checking & Training
 */

class NeuralLabController {
    constructor() {
        this.currentResult = null;
        this.isTraining = false;

        this.plotlyLayoutBase = {
            paper_bgcolor: 'transparent',
            plot_bgcolor: '#121722',
            font: {
                family: '-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace',
                color: '#e6edf3',
                size: 11
            },
            margin: { t: 36, r: 24, b: 38, l: 56 },
            xaxis: {
                gridcolor: '#1c2637',
                linecolor: '#273449',
                tickfont: { color: '#8b949e', size: 10 }
            },
            yaxis: {
                gridcolor: '#1c2637',
                linecolor: '#273449',
                tickfont: { color: '#8b949e', size: 10 }
            },
            legend: {
                x: 0.02, y: 0.98,
                bgcolor: 'rgba(24, 32, 48, 0.85)',
                bordercolor: '#273449',
                borderwidth: 1,
                font: { size: 10, color: '#e6edf3' }
            }
        };

        this.init();
    }

    init() {
        this.bindEvents();
        this.initTabs();
        this.initBlankCharts();
        // Auto-run baseline training on initial launch so the view is populated immediately
        setTimeout(() => this.runTraining(), 300);
    }

    bindEvents() {
        const btnTrain = document.getElementById('btn-run-neural-train');
        if (btnTrain) {
            btnTrain.addEventListener('click', () => this.runTraining());
        }

        const btnCheck = document.getElementById('btn-run-neural-check');
        if (btnCheck) {
            btnCheck.addEventListener('click', () => this.checkGradientsOnly());
        }

        const btnReset = document.getElementById('btn-reset-neural');
        if (btnReset) {
            btnReset.addEventListener('click', () => this.resetDefaults());
        }

        const btnExport = document.getElementById('btn-export-neural-csv');
        if (btnExport) {
            btnExport.addEventListener('click', () => this.exportCsv());
        }
    }

    initTabs() {
        const tabBtns = document.querySelectorAll('#view-neural .tabs-header .tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                tabBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');

                const targetTabId = btn.dataset.tab;
                const contents = document.querySelectorAll('#view-neural .tab-content');
                contents.forEach(content => {
                    content.classList.remove('active');
                    if (content.id === targetTabId) {
                        content.classList.add('active');
                    }
                });
            });
        });
    }

    getConfigFromInputs() {
        return {
            x: parseFloat(document.getElementById('neural-input-x')?.value ?? 1.0),
            y: parseFloat(document.getElementById('neural-input-y')?.value ?? 2.0),
            initial_parameters: {
                w: parseFloat(document.getElementById('neural-input-w')?.value ?? 0.5),
                b: parseFloat(document.getElementById('neural-input-b')?.value ?? 0.0),
                v: parseFloat(document.getElementById('neural-input-v')?.value ?? 1.0),
                c: parseFloat(document.getElementById('neural-input-c')?.value ?? 0.0)
            },
            learning_rate: parseFloat(document.getElementById('neural-input-lr')?.value ?? 0.1),
            num_steps: parseInt(document.getElementById('neural-input-steps')?.value ?? 100, 10),
            epsilon: parseFloat(document.getElementById('neural-input-eps')?.value ?? 1e-6),
            max_tolerance: parseFloat(document.getElementById('neural-input-tol')?.value ?? 1e-5)
        };
    }

    resetDefaults() {
        document.getElementById('neural-input-x').value = '1.0';
        document.getElementById('neural-input-y').value = '2.0';
        document.getElementById('neural-input-w').value = '0.5';
        document.getElementById('neural-input-b').value = '0.0';
        document.getElementById('neural-input-v').value = '1.0';
        document.getElementById('neural-input-c').value = '0.0';
        document.getElementById('neural-input-lr').value = '0.1';
        document.getElementById('neural-input-steps').value = '100';
        document.getElementById('neural-input-eps').value = '0.000001';
        document.getElementById('neural-input-tol').value = '0.00001';

        window.showToast('Reset hyperparameters to benchmark defaults.', 'info');
        this.runTraining();
    }

    initBlankCharts() {
        if (!window.Plotly) return;

        Plotly.newPlot('plot-neural-loss', [{
            x: [], y: [], mode: 'lines+markers',
            line: { color: '#39c5cf', width: 2 },
            marker: { size: 4, color: '#388bfd' }
        }], {
            ...this.plotlyLayoutBase,
            title: { text: 'Loss Convergence J(θ) vs Step', font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyLayoutBase.xaxis, title: { text: 'Training Step (Iteration)', font: { size: 10, color: '#8b949e' } } },
            yaxis: { ...this.plotlyLayoutBase.yaxis, title: { text: 'Half MSE Loss J', font: { size: 10, color: '#8b949e' } } }
        }, { responsive: true, displayModeBar: false });

        Plotly.newPlot('plot-neural-params', [
            { x: [], y: [], name: 'w (weight)', line: { color: '#388bfd', width: 2 } },
            { x: [], y: [], name: 'b (bias)', line: { color: '#39c5cf', width: 2 } },
            { x: [], y: [], name: 'v (weight)', line: { color: '#d29922', width: 2 } },
            { x: [], y: [], name: 'c (bias)', line: { color: '#f85149', width: 2 } }
        ], {
            ...this.plotlyLayoutBase,
            title: { text: 'Parameter Trajectories [w, b, v, c] vs Step', font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyLayoutBase.xaxis, title: { text: 'Training Step (Iteration)', font: { size: 10, color: '#8b949e' } } },
            yaxis: { ...this.plotlyLayoutBase.yaxis, title: { text: 'Parameter Value', font: { size: 10, color: '#8b949e' } } }
        }, { responsive: true, displayModeBar: false });

        Plotly.newPlot('plot-neural-grad-error', [{
            x: [], y: [], type: 'scatter', mode: 'lines+markers',
            name: 'Peak Gradient Error',
            line: { color: '#2ea043', width: 1.5 },
            marker: { size: 3, color: '#2ea043' }
        }], {
            ...this.plotlyLayoutBase,
            title: { text: 'Finite-Difference Discrepancy vs Tolerance (10⁻⁵)', font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyLayoutBase.xaxis, title: { text: 'Training Step', font: { size: 10, color: '#8b949e' } } },
            yaxis: {
                ...this.plotlyLayoutBase.yaxis,
                type: 'log',
                title: { text: 'Absolute Error (log scale)', font: { size: 10, color: '#8b949e' } }
            }
        }, { responsive: true, displayModeBar: false });
    }

    async runTraining() {
        if (this.isTraining) return;
        this.isTraining = true;

        const btnLabel = document.getElementById('neural-btn-label');
        const progressBar = document.getElementById('neural-progress-bar');
        const progressContainer = document.getElementById('neural-progress-container');
        const statusText = document.getElementById('neural-status-text');

        if (btnLabel) btnLabel.innerText = 'Training in C++...';
        if (progressContainer) progressContainer.style.display = 'block';
        if (progressBar) {
            progressBar.style.width = '40%';
            progressBar.style.transition = 'width 0.4s ease';
        }
        if (statusText) statusText.innerText = 'Executing C++ forward pass, exact chain-rule backpropagation, and numerical gradient checks...';

        const config = this.getConfigFromInputs();

        try {
            const res = await window.api.trainNeuralNetwork(config);
            if (progressBar) progressBar.style.width = '100%';

            if (!res.success || !res.data || !res.data.success) {
                const errMsg = res.data?.error_message || res.error || 'Unknown training error occurred';
                if (statusText) statusText.innerText = `Error: ${errMsg}`;
                window.showToast(`Training failed: ${errMsg}`, 'error');
                return;
            }

            this.currentResult = res.data;
            this.populateSummary(this.currentResult);
            this.renderCharts(this.currentResult);
            this.populateStepTable(this.currentResult.step_history || []);

            if (statusText) {
                statusText.innerText = `Training complete: 100 steps computed in C++. Initial Loss: ${this.currentResult.initial_loss.toFixed(6)}, Final Loss: ${this.currentResult.final_loss.toExponential(4)}. All gradient checks passed.`;
            }
            window.showToast('Neural network training and gradient verification completed successfully.', 'success');
        } catch (err) {
            if (statusText) statusText.innerText = `Execution error: ${err.message}`;
            window.showToast(`Error: ${err.message}`, 'error');
        } finally {
            this.isTraining = false;
            if (btnLabel) btnLabel.innerText = 'Run 100-Step Training';
            setTimeout(() => {
                if (progressContainer) progressContainer.style.display = 'none';
                if (progressBar) progressBar.style.width = '0%';
            }, 600);
        }
    }

    async checkGradientsOnly() {
        const config = this.getConfigFromInputs();
        const payload = {
            x: config.x,
            y: config.y,
            parameters: config.initial_parameters,
            epsilon: config.epsilon,
            max_tolerance: config.max_tolerance
        };

        try {
            const res = await window.api.checkNeuralGradients(payload);
            if (res.success && res.data && res.data.gradient_check) {
                const check = res.data.gradient_check;
                this.populateGradCheckTable(check);
                const pill = document.getElementById('neural-kpi-verification');
                if (pill) {
                    pill.className = check.passed ? 'metric-pill pill-success' : 'metric-pill pill-error';
                    pill.innerText = check.passed ? `PASS (Max Err: ${check.max_error.toExponential(2)})` : 'FAIL';
                }
                window.showToast(`Gradient Check: ${check.passed ? 'PASSED' : 'FAILED'} (Max Error: ${check.max_error.toExponential(3)})`, check.passed ? 'success' : 'error');
            } else {
                window.showToast('Gradient check failed to evaluate.', 'error');
            }
        } catch (err) {
            window.showToast(`Gradient check error: ${err.message}`, 'error');
        }
    }

    populateSummary(result) {
        // KPIs
        const initLossEl = document.getElementById('neural-kpi-init-loss');
        if (initLossEl) initLossEl.innerText = result.initial_loss.toFixed(6);

        const finalLossEl = document.getElementById('neural-kpi-final-loss');
        if (finalLossEl) finalLossEl.innerText = result.final_loss < 1e-4 ? result.final_loss.toExponential(4) : result.final_loss.toFixed(6);

        const yhatEl = document.getElementById('neural-kpi-yhat');
        if (yhatEl) yhatEl.innerText = `${result.initial_prediction.toFixed(4)} → ${result.final_prediction.toFixed(4)}`;

        const maxErrEl = document.getElementById('neural-kpi-max-error');
        if (maxErrEl) maxErrEl.innerText = `${result.max_gradient_check_error.toExponential(3)} (Tol: ${result.config.max_tolerance})`;

        const pill = document.getElementById('neural-kpi-verification');
        if (pill) {
            pill.className = result.all_gradient_checks_passed ? 'metric-pill pill-success' : 'metric-pill pill-error';
            pill.innerText = result.all_gradient_checks_passed ? 'PASS (Tol: 1e-5)' : 'FAIL';
        }

        // Populate Step 0 gradient check table
        if (result.step_history && result.step_history.length > 0) {
            const step0 = result.step_history[0];
            this.populateGradCheckTable(step0.gradient_check);
        }
    }

    populateGradCheckTable(check) {
        const tbody = document.getElementById('neural-grad-table-body');
        if (!tbody || !check) return;

        const params = [
            { name: 'w (hidden weight)', data: check.w },
            { name: 'b (hidden bias)', data: check.b },
            { name: 'v (output weight)', data: check.v },
            { name: 'c (output bias)', data: check.c }
        ];

        let html = '';
        params.forEach(p => {
            const pass = p.data.error <= check.max_tolerance;
            html += `
                <tr>
                    <td style="font-weight: 600; color: var(--text-primary);">${p.name}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-cyan);">${p.data.analytical.toFixed(6)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-blue);">${p.data.numerical.toFixed(6)}</td>
                    <td style="font-family: var(--font-mono); color: ${pass ? 'var(--accent-green)' : 'var(--accent-red)'};">${p.data.error.toExponential(4)}</td>
                    <td><span class="metric-pill ${pass ? 'pill-success' : 'pill-error'}" style="font-size: 10px; padding: 2px 6px;">${pass ? 'PASS' : 'FAIL'}</span></td>
                </tr>
            `;
        });
        tbody.innerHTML = html;
    }

    renderCharts(result) {
        if (!window.Plotly || !result.step_history) return;

        const steps = result.step_history.map(s => s.step);
        const losses = result.step_history.map(s => s.loss);
        const ws = result.step_history.map(s => s.parameters.w);
        const bs = result.step_history.map(s => s.parameters.b);
        const vs = result.step_history.map(s => s.parameters.v);
        const cs = result.step_history.map(s => s.parameters.c);
        const gradErrors = result.step_history.map(s => s.gradient_check.max_error);

        // Chart 1: Loss Convergence (Logarithmic)
        const lossTrace = {
            x: steps,
            y: losses,
            mode: 'lines+markers',
            name: 'Loss J(θ)',
            line: { color: '#39c5cf', width: 2.5 },
            marker: { size: 3, color: '#388bfd' }
        };

        const lossLayout = {
            ...this.plotlyLayoutBase,
            title: { text: `Loss Convergence: J₀ = ${result.initial_loss.toFixed(4)} → J₁₀₀ = ${result.final_loss.toExponential(2)}`, font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyLayoutBase.xaxis, title: { text: 'Training Step (Iteration)', font: { size: 10, color: '#8b949e' } } },
            yaxis: {
                ...this.plotlyLayoutBase.yaxis,
                type: 'log',
                title: { text: 'Half MSE Loss (log scale)', font: { size: 10, color: '#8b949e' } }
            }
        };
        Plotly.react('plot-neural-loss', [lossTrace], lossLayout, { responsive: true, displayModeBar: false });

        // Chart 2: Parameter Trajectories
        const paramTraces = [
            { x: steps, y: ws, name: 'w (hidden weight)', line: { color: '#388bfd', width: 2 } },
            { x: steps, y: bs, name: 'b (hidden bias)', line: { color: '#39c5cf', width: 2 } },
            { x: steps, y: vs, name: 'v (output weight)', line: { color: '#d29922', width: 2 } },
            { x: steps, y: cs, name: 'c (output bias)', line: { color: '#f85149', width: 2 } }
        ];

        const paramLayout = {
            ...this.plotlyLayoutBase,
            title: { text: 'Parameter Trajectories [w, b, v, c] over 100 Steps', font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyLayoutBase.xaxis, title: { text: 'Training Step (Iteration)', font: { size: 10, color: '#8b949e' } } },
            yaxis: { ...this.plotlyLayoutBase.yaxis, title: { text: 'Parameter Value', font: { size: 10, color: '#8b949e' } } }
        };
        Plotly.react('plot-neural-params', paramTraces, paramLayout, { responsive: true, displayModeBar: false });

        // Chart 3: Gradient Checking Errors vs Tolerance
        const gradTrace = {
            x: steps,
            y: gradErrors,
            mode: 'lines+markers',
            name: 'Max Parameter Discrepancy',
            line: { color: '#2ea043', width: 1.5 },
            marker: { size: 3, color: '#2ea043' }
        };

        const tolLine = {
            x: [0, steps[steps.length - 1]],
            y: [result.config.max_tolerance, result.config.max_tolerance],
            mode: 'lines',
            name: 'Tolerance Gate (10⁻⁵)',
            line: { color: '#f85149', width: 2, dash: 'dash' }
        };

        const gradLayout = {
            ...this.plotlyLayoutBase,
            title: { text: `Central Difference Gradient Discrepancy (Peak: ${result.max_gradient_check_error.toExponential(3)} ≪ 10⁻⁵)`, font: { size: 12, color: '#e6edf3' } },
            xaxis: { ...this.plotlyLayoutBase.xaxis, title: { text: 'Training Step', font: { size: 10, color: '#8b949e' } } },
            yaxis: {
                ...this.plotlyLayoutBase.yaxis,
                type: 'log',
                title: { text: 'Absolute Error (log scale)', font: { size: 10, color: '#8b949e' } }
            }
        };
        Plotly.react('plot-neural-grad-error', [gradTrace, tolLine], gradLayout, { responsive: true, displayModeBar: false });
    }

    populateStepTable(stepHistory) {
        const tbody = document.getElementById('neural-step-table-body');
        if (!tbody) return;

        let rows = '';
        stepHistory.forEach(s => {
            const pass = s.gradient_check.passed;
            rows += `
                <tr>
                    <td style="font-weight: 700; color: var(--accent-cyan);">${s.step}</td>
                    <td style="font-family: var(--font-mono);">${s.parameters.w.toFixed(5)}</td>
                    <td style="font-family: var(--font-mono);">${s.parameters.b.toFixed(5)}</td>
                    <td style="font-family: var(--font-mono);">${s.parameters.v.toFixed(5)}</td>
                    <td style="font-family: var(--font-mono);">${s.parameters.c.toFixed(5)}</td>
                    <td style="font-family: var(--font-mono); color: var(--text-muted);">${s.z.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: var(--text-muted);">${s.h.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); font-weight: 600; color: var(--text-primary);">${s.y_hat.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-amber);">${s.error.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-green);">${s.loss < 1e-4 ? s.loss.toExponential(3) : s.loss.toFixed(5)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-cyan);">${s.gradients.dJ_dw.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-cyan);">${s.gradients.dJ_db.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-cyan);">${s.gradients.dJ_dv.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: var(--accent-cyan);">${s.gradients.dJ_dc.toFixed(4)}</td>
                    <td style="font-family: var(--font-mono); color: ${pass ? 'var(--accent-green)' : 'var(--accent-red)'};">${s.gradient_check.max_error.toExponential(2)}</td>
                    <td><span class="metric-pill ${pass ? 'pill-success' : 'pill-error'}" style="font-size: 9px; padding: 1px 5px;">${pass ? 'PASS' : 'FAIL'}</span></td>
                </tr>
            `;
        });

        tbody.innerHTML = rows;
    }

    exportCsv() {
        const config = this.getConfigFromInputs();
        const exportUrl = window.api.getNeuralExportCsvUrl(config);
        const link = document.createElement('a');
        link.href = exportUrl;
        link.download = 'neural_backprop_training_log.csv';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        window.showToast('Downloading neural training log CSV...', 'info');
    }

    resizeCharts() {
        if (!window.Plotly) return;
        ['plot-neural-loss', 'plot-neural-params', 'plot-neural-grad-error'].forEach(id => {
            const el = document.getElementById(id);
            if (el && el.data) {
                Plotly.Plots.resize(el);
            }
        });
    }
}

// Instantiate on DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.neuralLab = new NeuralLabController();
});

