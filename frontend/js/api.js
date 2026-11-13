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
}

window.api = new WorkbenchApi();

