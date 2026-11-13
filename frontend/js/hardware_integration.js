/**
 * Unified Engineering Workbench - Embedded Hardware Integration Controller
 * Manages C++ mock hardware devices (MCU, FPGA, JTAG), real-time telemetry streaming,
 * fault injection & recovery, and electrical systems architecture documentation.
 */

class HardwareIntegration {
    constructor() {
        this.pollTimer = null;
        this.isPollingActive = true;
        this.pollIntervalMs = 1000;
        this.devicesData = {};

        // Rolling history for Plotly voltage strip chart (last 25 samples)
        this.maxHistoryLength = 25;
        this.historyTimestamps = [];
        this.historyV33 = [];
        this.historyV18 = [];
        this.historyV10 = [];

        this.plotlyDarkLayout = {
            paper_bgcolor: '#182030',
            plot_bgcolor: '#121722',
            font: { family: 'Consolas, monospace', size: 11, color: '#8b949e' },
            margin: { l: 45, r: 20, t: 30, b: 35 },
            xaxis: {
                gridcolor: '#273449',
                linecolor: '#273449',
                tickfont: { color: '#8b949e' },
                title: { text: 'Time (s)', font: { size: 10, color: '#e6edf3' } }
            },
            yaxis: {
                gridcolor: '#273449',
                linecolor: '#273449',
                tickfont: { color: '#8b949e' },
                title: { text: 'Voltage (V)', font: { size: 10, color: '#e6edf3' } },
                range: [0.5, 3.8]
            },
            legend: {
                x: 0.02, y: 0.98,
                bgcolor: 'rgba(24, 32, 48, 0.85)',
                bordercolor: '#273449',
                borderwidth: 1,
                font: { size: 10, color: '#e6edf3' },
                orientation: 'h'
            }
        };

        this.init();
    }

    init() {
        this.bindEvents();
        this.initTabs();
        this.loadArchitectureData();
        this.initVoltageChart();
        this.pollTelemetry();
        this.startAutoPolling();
    }

    bindEvents() {
        // Master Bus Buttons
        document.getElementById('btn-hw-poll-now')?.addEventListener('click', () => {
            this.pollTelemetry();
            if (window.showToast) window.showToast('Telemetry frame polled.', 'info');
        });

        document.getElementById('btn-hw-connect-all')?.addEventListener('click', async () => {
            const res = await window.api.connectHardwareDevice('all');
            if (res.success) {
                if (window.showToast) window.showToast('All 3 hardware devices connected.', 'success');
                this.pollTelemetry();
            }
        });

        document.getElementById('btn-hw-disconnect-all')?.addEventListener('click', async () => {
            const res = await window.api.disconnectHardwareDevice('all');
            if (res.success) {
                if (window.showToast) window.showToast('All hardware devices disconnected.', 'info');
                this.pollTelemetry();
            }
        });

        // MCU Controls
        document.getElementById('btn-mcu-toggle')?.addEventListener('click', () => this.toggleDevice('mcu_stm32h7'));
        document.getElementById('btn-mcu-fault')?.addEventListener('click', () => this.injectFault('mcu_stm32h7', 'timeout'));
        document.getElementById('btn-mcu-recover')?.addEventListener('click', () => this.recoverFault('mcu_stm32h7'));

        // FPGA Controls
        document.getElementById('btn-fpga-toggle')?.addEventListener('click', () => this.toggleDevice('fpga_artix7'));
        document.getElementById('btn-fpga-fault')?.addEventListener('click', () => this.injectFault('fpga_artix7', 'timeout'));
        document.getElementById('btn-fpga-recover')?.addEventListener('click', () => this.recoverFault('fpga_artix7'));

        // JTAG Controls
        document.getElementById('btn-jtag-toggle')?.addEventListener('click', () => this.toggleDevice('jtag_ft2232h'));
        document.getElementById('btn-jtag-fault')?.addEventListener('click', () => this.injectFault('jtag_ft2232h', 'scan_chain_break'));
        document.getElementById('btn-jtag-recover')?.addEventListener('click', () => this.recoverFault('jtag_ft2232h'));
    }

    initTabs() {
        const tabBtns = document.querySelectorAll('#view-hardware .tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                tabBtns.forEach(b => b.classList.remove('active'));
                btn.classList.add('active');

                const targetTabId = btn.dataset.tab;
                const contents = document.querySelectorAll('#view-hardware .tab-content');
                contents.forEach(content => {
                    content.classList.remove('active');
                    if (content.id === targetTabId) {
                        content.classList.add('active');
                    }
                });
            });
        });
    }

    async loadArchitectureData() {
        try {
            const res = await window.api.getHardwareArchitecture();
            if (res.success && res.data) {
                const pinTable = res.data.pin_voltage_table || [];
                const tbody = document.getElementById('hw-pin-table-body');
                if (tbody && pinTable.length > 0) {
                    let rows = '';
                    pinTable.forEach(p => {
                        rows += `
                            <tr>
                                <td style="font-weight: 700; color: var(--accent-cyan);">${p.pin}</td>
                                <td style="font-family: var(--font-mono); font-weight: 600; color: var(--text-primary);">${p.signal}</td>
                                <td>${p.source}</td>
                                <td>${p.destination}</td>
                                <td style="font-family: var(--font-mono); color: var(--accent-amber);">${p.voltage}</td>
                                <td>${p.protocol}</td>
                                <td style="color: var(--text-muted); font-size: 11px;">${p.pull}</td>
                                <td style="color: var(--text-secondary); font-size: 11px;">${p.ownership}</td>
                            </tr>
                        `;
                    });
                    tbody.innerHTML = rows;
                }
            }
        } catch (err) {
            console.error('Failed to load hardware architecture metadata:', err);
        }
    }

    startAutoPolling() {
        if (this.pollTimer) clearInterval(this.pollTimer);
        this.pollTimer = setInterval(() => {
            if (this.isPollingActive) {
                this.pollTelemetry();
            }
        }, this.pollIntervalMs);
    }

    async pollTelemetry() {
        try {
            const res = await window.api.getHardwareTelemetry();
            if (res.success && res.data && res.data.devices) {
                const devices = res.data.devices;
                let onlineCount = 0;
                let errorCount = 0;

                let latestV33 = 3.30;
                let latestV18 = 1.80;
                let latestV10 = 1.00;

                devices.forEach(d => {
                    const id = d.device_id;
                    this.devicesData[id] = d;

                    if (d.state === 'STREAMING' || d.state === 'CONNECTED') onlineCount++;
                    if (d.state === 'ERROR_TIMEOUT') errorCount++;

                    if (id === 'mcu_stm32h7') {
                        this.updateMcuUi(d);
                        if (d.rail_voltages && d.rail_voltages['3.3V_VDD']) {
                            latestV33 = d.rail_voltages['3.3V_VDD'];
                        }
                    } else if (id === 'fpga_artix7') {
                        this.updateFpgaUi(d);
                        if (d.rail_voltages) {
                            if (d.rail_voltages['1.0V_VCCINT']) latestV10 = d.rail_voltages['1.0V_VCCINT'];
                            if (d.rail_voltages['1.8V_VCCAUX']) latestV18 = d.rail_voltages['1.8V_VCCAUX'];
                        }
                    } else if (id === 'jtag_ft2232h') {
                        this.updateJtagUi(d);
                    }
                });

                // Update Master Status Pill
                const masterPill = document.getElementById('hw-master-status-pill');
                if (masterPill) {
                    if (errorCount > 0) {
                        masterPill.className = 'metric-pill pill-pending';
                        masterPill.innerText = `${errorCount} Fault Detected (Recoverable)`;
                    } else if (onlineCount === 3) {
                        masterPill.className = 'metric-pill pill-success';
                        masterPill.innerText = '3/3 Devices Streaming';
                    } else {
                        masterPill.className = 'metric-pill pill-blue';
                        masterPill.innerText = `${onlineCount}/3 Devices Online`;
                    }
                }

                // Update Plotly Voltage Strip Chart
                const nowSec = (this.historyTimestamps.length > 0)
                    ? this.historyTimestamps[this.historyTimestamps.length - 1] + 1
                    : 1;

                this.historyTimestamps.push(nowSec);
                this.historyV33.push(latestV33);
                this.historyV18.push(latestV18);
                this.historyV10.push(latestV10);

                if (this.historyTimestamps.length > this.maxHistoryLength) {
                    this.historyTimestamps.shift();
                    this.historyV33.shift();
                    this.historyV18.shift();
                    this.historyV10.shift();
                }

                this.updateVoltageChart();
            }
        } catch (err) {
            console.error('Error polling hardware telemetry:', err);
        }
    }

    updateMcuUi(d) {
        const card = document.getElementById('card-dev-mcu');
        const pill = document.getElementById('mcu-status-pill');
        const btnToggle = document.getElementById('btn-mcu-toggle');
        const logSnippet = document.getElementById('mcu-log-snippet');

        this.applyDeviceStatePill(card, pill, btnToggle, d.state);

        if (logSnippet) logSnippet.innerText = d.last_log_message || '--';

        if (d.state === 'STREAMING' || d.state === 'CONNECTED') {
            const vdd = d.rail_voltages?.['3.3V_VDD'] ? `${d.rail_voltages['3.3V_VDD'].toFixed(3)} V` : '--';
            const temp = `${d.core_temperature_c.toFixed(1)} °C`;
            const curr = `${d.current_draw_ma.toFixed(1)} mA`;
            const uptime = `${Math.round(d.uptime_ms / 1000)} s`;
            const pkts = `${d.packets_transmitted} / ${d.packets_received}`;

            const elVdd = document.getElementById('mcu-metric-vdd');
            const elTemp = document.getElementById('mcu-metric-temp');
            const elCurr = document.getElementById('mcu-metric-curr');
            const elUptime = document.getElementById('mcu-metric-uptime');
            const elPkts = document.getElementById('mcu-metric-pkts');
            const elAdc = document.getElementById('mcu-metric-adc');

            if (elVdd) elVdd.innerText = vdd;
            if (elTemp) elTemp.innerText = temp;
            if (elCurr) elCurr.innerText = curr;
            if (elUptime) elUptime.innerText = uptime;
            if (elPkts) elPkts.innerText = pkts;

            if (elAdc && d.device_specific) {
                const ch1 = d.device_specific.adc_channel_1_volts?.toFixed(3) || '1.650';
                const ch2 = d.device_specific.adc_channel_2_volts?.toFixed(3) || '2.480';
                elAdc.innerText = `CH1: ${ch1} V | CH2: ${ch2} V`;
            }
        }
    }

    updateFpgaUi(d) {
        const card = document.getElementById('card-dev-fpga');
        const pill = document.getElementById('fpga-status-pill');
        const btnToggle = document.getElementById('btn-fpga-toggle');
        const logSnippet = document.getElementById('fpga-log-snippet');

        this.applyDeviceStatePill(card, pill, btnToggle, d.state);

        if (logSnippet) logSnippet.innerText = d.last_log_message || '--';

        if (d.state === 'STREAMING' || d.state === 'CONNECTED') {
            const vccint = d.rail_voltages?.['1.0V_VCCINT'] ? `${d.rail_voltages['1.0V_VCCINT'].toFixed(3)} V` : '--';
            const vccaux = d.rail_voltages?.['1.8V_VCCAUX'] ? `${d.rail_voltages['1.8V_VCCAUX'].toFixed(3)} V` : '--';
            const vcco = d.rail_voltages?.['3.3V_VCCO_0'] ? `${d.rail_voltages['3.3V_VCCO_0'].toFixed(3)} V` : '--';
            const temp = `${d.core_temperature_c.toFixed(1)} °C`;
            const curr = `${d.current_draw_ma.toFixed(1)} mA`;

            const elVccint = document.getElementById('fpga-metric-vccint');
            const elVccaux = document.getElementById('fpga-metric-vccaux');
            const elVcco = document.getElementById('fpga-metric-vcco');
            const elTemp = document.getElementById('fpga-metric-temp');
            const elCurr = document.getElementById('fpga-metric-curr');

            if (elVccint) elVccint.innerText = vccint;
            if (elVccaux) elVccaux.innerText = vccaux;
            if (elVcco) elVcco.innerText = vcco;
            if (elTemp) elTemp.innerText = temp;
            if (elCurr) elCurr.innerText = curr;
        }
    }

    updateJtagUi(d) {
        const card = document.getElementById('card-dev-jtag');
        const pill = document.getElementById('jtag-status-pill');
        const btnToggle = document.getElementById('btn-jtag-toggle');
        const logSnippet = document.getElementById('jtag-log-snippet');

        this.applyDeviceStatePill(card, pill, btnToggle, d.state);

        if (logSnippet) logSnippet.innerText = d.last_log_message || '--';

        if (d.state === 'STREAMING' || d.state === 'CONNECTED') {
            const vtarget = d.rail_voltages?.['VTarget_Sense'] ? `${d.rail_voltages['VTarget_Sense'].toFixed(3)} V` : '--';
            const elVtarget = document.getElementById('jtag-metric-vtarget');
            const elTap = document.getElementById('jtag-metric-tap');

            if (elVtarget) elVtarget.innerText = vtarget;
            if (elTap && d.device_specific) {
                elTap.innerText = d.device_specific.tap_controller_state || 'Shift-DR / Run-Test/Idle';
            }
        }
    }

    applyDeviceStatePill(card, pill, btnToggle, state) {
        if (!pill) return;
        pill.innerText = state;

        if (card) {
            if (state === 'ERROR_TIMEOUT') {
                card.classList.add('error-state');
            } else {
                card.classList.remove('error-state');
            }
        }

        switch (state) {
            case 'STREAMING':
                pill.className = 'metric-pill pill-success';
                if (btnToggle) btnToggle.innerText = 'Disconnect';
                break;
            case 'CONNECTED':
                pill.className = 'metric-pill pill-cyan';
                if (btnToggle) btnToggle.innerText = 'Disconnect';
                break;
            case 'DISCONNECTED':
                pill.className = 'metric-pill pill-pending';
                if (btnToggle) btnToggle.innerText = 'Connect';
                break;
            case 'ERROR_TIMEOUT':
                pill.className = 'metric-pill pill-pending';
                pill.style.backgroundColor = 'rgba(248, 81, 73, 0.2)';
                pill.style.color = '#f85149';
                if (btnToggle) btnToggle.innerText = 'Reconnect';
                break;
            default:
                pill.className = 'metric-pill pill-blue';
                break;
        }
    }

    async toggleDevice(deviceId) {
        const dev = this.devicesData[deviceId];
        if (dev && (dev.state === 'STREAMING' || dev.state === 'CONNECTED')) {
            const res = await window.api.disconnectHardwareDevice(deviceId);
            if (res.success && window.showToast) {
                window.showToast(`${dev.device_id} disconnected.`, 'info');
            }
        } else {
            const res = await window.api.connectHardwareDevice(deviceId);
            if (res.success && window.showToast) {
                window.showToast(`${deviceId} connected.`, 'success');
            }
        }
        this.pollTelemetry();
    }

    async injectFault(deviceId, faultType = 'timeout') {
        const res = await window.api.injectHardwareFault(deviceId, faultType);
        if (res.success) {
            if (window.showToast) {
                window.showToast(`Simulated ${faultType} fault injected into ${deviceId}.`, 'error');
            }
            this.pollTelemetry();
        }
    }

    async recoverFault(deviceId) {
        const res = await window.api.recoverHardwareFault(deviceId);
        if (res.success) {
            if (window.showToast) {
                window.showToast(`Recovered ${deviceId}. Device re-synchronized successfully.`, 'success');
            }
            this.pollTelemetry();
        } else {
            if (window.showToast) {
                window.showToast(`Device ${deviceId} is not currently in a fault state.`, 'info');
            }
        }
    }

    initVoltageChart() {
        if (!window.Plotly) return;

        const trace33 = {
            x: [],
            y: [],
            mode: 'lines+markers',
            name: '3.3V VDD Rail (Nominal 3.30V)',
            line: { color: '#39c5cf', width: 2 },
            marker: { size: 5, color: '#39c5cf' }
        };

        const trace18 = {
            x: [],
            y: [],
            mode: 'lines+markers',
            name: '1.8V VCCAUX Rail (Nominal 1.80V)',
            line: { color: '#388bfd', width: 2 },
            marker: { size: 5, color: '#388bfd' }
        };

        const trace10 = {
            x: [],
            y: [],
            mode: 'lines+markers',
            name: '1.0V VCCINT Core Rail (Nominal 1.00V)',
            line: { color: '#d29922', width: 2 },
            marker: { size: 5, color: '#d29922' }
        };

        Plotly.newPlot('chart-hw-voltages', [trace33, trace18, trace10], this.plotlyDarkLayout, {
            responsive: true,
            displayModeBar: false
        });
    }

    updateVoltageChart() {
        if (!window.Plotly) return;
        const chartEl = document.getElementById('chart-hw-voltages');
        if (!chartEl || !chartEl.data) return;

        Plotly.update('chart-hw-voltages', {
            x: [this.historyTimestamps, this.historyTimestamps, this.historyTimestamps],
            y: [this.historyV33, this.historyV18, this.historyV10]
        });
    }

    resizeCharts() {
        if (!window.Plotly) return;
        const chartEl = document.getElementById('chart-hw-voltages');
        if (chartEl && chartEl.data) {
            Plotly.Plots.resize(chartEl);
        }
    }
}

// Instantiate on DOM load
document.addEventListener('DOMContentLoaded', () => {
    window.hardwareIntegration = new HardwareIntegration();
});
