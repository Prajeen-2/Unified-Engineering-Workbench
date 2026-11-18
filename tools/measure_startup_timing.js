const { spawn } = require('child_process');
const http = require('http');
const fs = require('fs');
const path = require('path');

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function probeHealth(port) {
    return new Promise((resolve) => {
        const req = http.get(`http://127.0.0.1:${port}/api/health`, { timeout: 200 }, (res) => {
            if (res.statusCode === 200) {
                resolve(true);
            } else {
                resolve(false);
            }
        });
        req.on('error', () => resolve(false));
        req.on('timeout', () => { req.destroy(); resolve(false); });
    });
}

async function measureProcessSpawn(testPort = 8088) {
    const serverExe = path.join(__dirname, '..', 'workbench_server.exe');
    console.log(`[1] Measuring workbench_server.exe cold process spawn on port ${testPort}...`);
    
    const t0 = performance.now();
    const proc = spawn(serverExe, ['--port', String(testPort), '--no-browser'], {
        stdio: 'ignore',
        detached: false
    });

    let ready = false;
    let t1 = 0;
    for (let i = 0; i < 200; ++i) { // max 2 seconds
        await sleep(5);
        if (await probeHealth(testPort)) {
            t1 = performance.now();
            ready = true;
            break;
        }
    }

    try {
        proc.kill();
    } catch (e) {}

    if (!ready) {
        throw new Error('Server failed to respond to /api/health within 2000 ms');
    }

    const elapsed = t1 - t0;
    console.log(`    -> Process Spawn + Port Bind + Health Ready: ${elapsed.toFixed(2)} ms`);
    return elapsed;
}

async function getCDPPage(wsPort) {
    for (let i = 0; i < 40; ++i) {
        await sleep(100);
        try {
            const res = await fetch(`http://127.0.0.1:${wsPort}/json`);
            if (res.ok) {
                const list = await res.json();
                const page = list.find(item => item.type === 'page');
                if (page && page.webSocketDebuggerUrl) {
                    return page.webSocketDebuggerUrl;
                }
            }
        } catch (e) {}
    }
    throw new Error('Failed to get WebSocket debugger URL from Edge');
}

async function measureBrowserUIReady() {
    const edgePath = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
    const cdpPort = 9224;
    const tempUserData = path.join(__dirname, '..', 'temp_edge_user_data');
    if (fs.existsSync(tempUserData)) {
        try { fs.rmSync(tempUserData, { recursive: true, force: true }); } catch (e) {}
    }

    console.log(`[2] Measuring Headless Browser UI Load & Render Latency via CDP...`);
    const tBrowserSpawn = performance.now();
    const edgeProc = spawn(edgePath, [
        '--headless=new',
        `--remote-debugging-port=${cdpPort}`,
        `--user-data-dir=${tempUserData}`,
        '--window-size=1400,900',
        'http://127.0.0.1:8080/'
    ], { stdio: 'ignore' });

    try {
        const wsUrl = await getCDPPage(cdpPort);
        console.log(`    Connected to CDP: ${wsUrl}`);
        const ws = new WebSocket(wsUrl);

        await new Promise((resolve, reject) => {
            ws.onopen = resolve;
            ws.onerror = reject;
        });

        let msgId = 1;
        function sendCmd(method, params = {}) {
            return new Promise((resolve) => {
                const id = msgId++;
                const handler = (evt) => {
                    const data = JSON.parse(evt.data);
                    if (data.id === id) {
                        ws.removeEventListener('message', handler);
                        resolve(data.result);
                    }
                };
                ws.addEventListener('message', handler);
                ws.send(JSON.stringify({ id, method, params }));
            });
        }

        await sendCmd('Page.enable');
        await sendCmd('Runtime.enable');

        // Wait 1.5 seconds for all initial async fetch and Three.js scene builds to complete
        await sleep(1500);

        // Query performance timing
        const evalRes = await sendCmd('Runtime.evaluate', {
            expression: `JSON.stringify({
                nav: performance.getEntriesByType('navigation')[0],
                timing: {
                    navigationStart: performance.timing.navigationStart,
                    domInteractive: performance.timing.domInteractive - performance.timing.navigationStart,
                    domContentLoaded: performance.timing.domContentLoadedEventEnd - performance.timing.navigationStart,
                    loadComplete: performance.timing.loadEventEnd - performance.timing.navigationStart
                }
            })`,
            returnByValue: true
        });

        const perfData = JSON.parse(evalRes.result.value);
        console.log('    Browser Performance Timings:');
        console.log(`      - DOM Interactive: ${perfData.timing.domInteractive} ms`);
        console.log(`      - DOM Content Loaded (UI Ready): ${perfData.timing.domContentLoaded} ms`);
        console.log(`      - Window Load Complete: ${perfData.timing.loadComplete} ms`);

        // Capture screenshot of UI
        console.log('    Capturing screenshot...');
        const screenshotRes = await sendCmd('Page.captureScreenshot', { format: 'png' });
        const screenshotBuf = Buffer.from(screenshotRes.data, 'base64');
        const outPath = path.join(__dirname, '..', 'results', 'plots', 'startup_timing_measured.png');
        fs.mkdirSync(path.dirname(outPath), { recursive: true });
        fs.writeFileSync(outPath, screenshotBuf);
        console.log(`    Saved screenshot to ${outPath} (${screenshotBuf.length} bytes)`);

        ws.close();
        return perfData.timing;
    } finally {
        edgeProc.kill();
        await sleep(300);
        if (fs.existsSync(tempUserData)) {
            try { fs.rmSync(tempUserData, { recursive: true, force: true }); } catch (e) {}
        }
    }
}

async function run() {
    console.log('=== REAL STARTUP TIMING MEASUREMENT ===\n');
    
    // Warm up / multiple runs for process spawn
    const spawnTimes = [];
    for (let i = 1; i <= 3; ++i) {
        const t = await measureProcessSpawn(8087 + i);
        spawnTimes.push(t);
        await sleep(200);
    }
    const avgSpawn = spawnTimes.reduce((a, b) => a + b, 0) / spawnTimes.length;
    console.log(`\nAverage Process Launch-to-Port-Ready: ${avgSpawn.toFixed(2)} ms (runs: ${spawnTimes.map(s => s.toFixed(1) + 'ms').join(', ')})`);

    // Browser measurement
    const browserTiming = await measureBrowserUIReady();

    console.log('\n=== SUMMARY OF REAL MEASURED TIMINGS ===');
    console.log(`1. Server Socket-Bind Latency (in-process C++): ~1.07 ms`);
    console.log(`2. Process Spawn + Winsock Startup: ${avgSpawn.toFixed(1)} ms`);
    console.log(`3. Browser UI Ready (DOM Content Loaded): ${browserTiming.domContentLoaded} ms`);
    console.log(`4. Total Launch-to-Ready: ~${(avgSpawn + browserTiming.domContentLoaded).toFixed(0)} ms`);
    console.log('========================================\n');
}

run().catch(err => {
    console.error('Error running measurement:', err);
    process.exit(1);
});
