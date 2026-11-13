/**
 * Unified Engineering Workbench - REST API Client
 * Manages frontend <-> C++ backend IPC communication
 */

class WorkbenchApi {
    constructor() {
        this.baseUrl = window.location.origin.startsWith('http') 
            ? window.location.origin 
            : 'http://127.0.0.1:8080';
        this.timeoutMs = 5000;
    }

    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), this.timeoutMs);

        const startTime = performance.now();
        try {
            const response = await fetch(url, {
                ...options,
                signal: controller.signal,
                headers: {
                    'Content-Type': 'application/json',
                    ...(options.headers || {})
                }
            });
            clearTimeout(timeoutId);
            const latency = Math.round(performance.now() - startTime);

            if (!response.ok) {
                throw new Error(`HTTP Error ${response.status}: ${response.statusText}`);
            }

            const data = await response.json();
            return { success: true, data, latency };
        } catch (err) {
            clearTimeout(timeoutId);
            const latency = Math.round(performance.now() - startTime);
            return {
                success: false,
                error: err.name === 'AbortError' ? 'Request timed out' : err.message,
                latency
            };
        }
    }

    async getHealth() {
        return this.request('/api/health');
    }

    async getSystemInfo() {
        return this.request('/api/system/info');
    }

    async getLogs(limit = 100) {
        return this.request(`/api/logs?limit=${limit}`);
    }

    async shutdown() {
        return this.request('/api/system/shutdown', { method: 'POST' });
    }

    async runPerformanceBenchmark(config = {}) {
        return this.request('/api/performance/benchmark', {
            method: 'POST',
            body: JSON.stringify(config)
        });
    }

    getPerformanceExportCsvUrl(config = {}) {
        const params = new URLSearchParams();
        if (config.num_cases) params.set('cases', config.num_cases);
        if (config.num_elements) params.set('elements', config.num_elements);
        if (config.random_seed) params.set('seed', config.random_seed);
        if (config.repetitions) params.set('repetitions', config.repetitions);
        return `${this.baseUrl}/api/performance/export-csv?${params.toString()}`;
    }

    // Hardware Integration API Endpoints (Task E)
    async getHardwareDevices() {
        return this.request('/api/hardware/devices');
    }

    async getHardwareTelemetry() {
        return this.request('/api/hardware/telemetry');
    }

    async getHardwareArchitecture() {
        return this.request('/api/hardware/architecture');
    }

    async connectHardwareDevice(deviceId = 'all') {
        return this.request('/api/hardware/connect', {
            method: 'POST',
            body: JSON.stringify({ device_id: deviceId })
        });
    }

    async disconnectHardwareDevice(deviceId = 'all') {
        return this.request('/api/hardware/disconnect', {
            method: 'POST',
            body: JSON.stringify({ device_id: deviceId })
        });
    }

    async injectHardwareFault(deviceId, faultType = 'timeout') {
        return this.request('/api/hardware/fault-inject', {
            method: 'POST',
            body: JSON.stringify({ device_id: deviceId, fault_type: faultType })
        });
    }

    async recoverHardwareFault(deviceId) {
        return this.request('/api/hardware/fault-recover', {
            method: 'POST',
            body: JSON.stringify({ device_id: deviceId })
        });
    }

    // Neural Network Lab API Endpoints (Task F)
    async trainNeuralNetwork(config = {}) {
        return this.request('/api/neural/train', {
            method: 'POST',
            body: JSON.stringify(config)
        });
    }

    async checkNeuralGradients(config = {}) {
        return this.request('/api/neural/gradient-check', {
            method: 'POST',
            body: JSON.stringify(config)
        });
    }

    getNeuralExportCsvUrl(config = {}) {
        const params = new URLSearchParams();
        if (config.x !== undefined) params.set('x', config.x);
        if (config.y !== undefined) params.set('y', config.y);
        if (config.initial_parameters?.w !== undefined) params.set('w', config.initial_parameters.w);
        if (config.initial_parameters?.b !== undefined) params.set('b', config.initial_parameters.b);
        if (config.initial_parameters?.v !== undefined) params.set('v', config.initial_parameters.v);
        if (config.initial_parameters?.c !== undefined) params.set('c', config.initial_parameters.c);
        if (config.learning_rate !== undefined) params.set('lr', config.learning_rate);
        if (config.num_steps !== undefined) params.set('steps', config.num_steps);
        if (config.epsilon !== undefined) params.set('eps', config.epsilon);
        if (config.max_tolerance !== undefined) params.set('tol', config.max_tolerance);
        return `${this.baseUrl}/api/neural/export-csv?${params.toString()}`;
    }

    // Reports & Exports API Endpoints (Task G)
    getHardwareExportCsvUrl() {
        return `${this.baseUrl}/api/hardware/export-csv`;
    }

    getLogsExportUrl() {
        return `${this.baseUrl}/api/logs/export`;
    }

    getReportDocumentUrl(filename) {
        return `${this.baseUrl}/api/reports/download?file=${encodeURIComponent(filename)}`;
    }

    async getReportsSummary() {
        return this.request('/api/reports/summary');
    }
}

window.api = new WorkbenchApi();

