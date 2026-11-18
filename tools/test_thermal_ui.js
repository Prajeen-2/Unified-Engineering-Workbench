const { spawn } = require('child_process');

async function testThermalUI() {
    const edgePath = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
    const cdpPort = 9226;
    const edge = spawn(edgePath, [
        '--headless=new',
        '--remote-debugging-port=' + cdpPort,
        '--window-size=1400,900',
        'http://127.0.0.1:8080/#thermal'
    ], { stdio: 'ignore' });

    await new Promise(r => setTimeout(r, 1500));

    try {
        const res = await fetch('http://127.0.0.1:' + cdpPort + '/json');
        const list = await res.json();
        const page = list.find(p => p.type === 'page');
        const ws = new WebSocket(page.webSocketDebuggerUrl);

        await new Promise(r => ws.onopen = r);

        let id = 1;
        function send(method, params = {}) {
            return new Promise(resolve => {
                const msgId = id++;
                const handler = (evt) => {
                    const data = JSON.parse(evt.data);
                    if (data.id === msgId) {
                        ws.removeEventListener('message', handler);
                        resolve(data.result);
                    }
                };
                ws.addEventListener('message', handler);
                ws.send(JSON.stringify({ id: msgId, method, params }));
            });
        }

        await send('Runtime.enable');

        // Check initial state
        let evalRes = await send('Runtime.evaluate', {
            expression: `({
                badge: document.getElementById('thermal-status-badge').innerText,
                maxTemp: document.getElementById('res-thermal-max-temp').innerText,
                elements: document.getElementById('res-thermal-elements').innerText,
                loc: document.getElementById('res-thermal-max-temp-loc').innerText
            })`,
            returnByValue: true
        });
        console.log('1. INITIAL STATE (BEFORE USER RUNS SIMULATION):');
        console.log(evalRes.result.value);

        // Click Run Simulation
        console.log('\n2. USER CLICKS "RUN SIMULATION"...');
        await send('Runtime.evaluate', {
            expression: `document.getElementById('btn-run-thermal').click()`
        });

        await new Promise(r => setTimeout(r, 600));

        evalRes = await send('Runtime.evaluate', {
            expression: `({
                badge: document.getElementById('thermal-status-badge').innerText,
                maxTemp: document.getElementById('res-thermal-max-temp').innerText,
                elements: document.getElementById('res-thermal-elements').innerText,
                loc: document.getElementById('res-thermal-max-temp-loc').innerText,
                maxError: document.getElementById('res-thermal-max-error').innerText
            })`,
            returnByValue: true
        });
        console.log('AFTER RUNNING SIMULATION WITH DEFAULT PARAMETERS (T0=100, TL=20, q=10000, k=10):');
        console.log(evalRes.result.value);

        // Modify a parameter
        console.log('\n3. USER EDITS PARAMETERS: T0 = 150 °C, q = 20000 W/m³...');
        await send('Runtime.evaluate', {
            expression: `
                document.getElementById('thermal-param-t0').value = '150.0';
                document.getElementById('thermal-param-t0').dispatchEvent(new Event('input'));
                document.getElementById('thermal-param-q').value = '20000.0';
                document.getElementById('thermal-param-q').dispatchEvent(new Event('input'));
            `
        });

        await new Promise(r => setTimeout(r, 200));

        evalRes = await send('Runtime.evaluate', {
            expression: `({
                badge: document.getElementById('thermal-status-badge').innerText,
                loc: document.getElementById('res-thermal-max-temp-loc').innerText
            })`,
            returnByValue: true
        });
        console.log('STATUS DETECTS CHANGE:');
        console.log(evalRes.result.value);

        // Click Run Simulation with new parameters
        console.log('\n4. USER CLICKS "RUN SIMULATION" WITH CUSTOM PARAMETERS...');
        await send('Runtime.evaluate', {
            expression: `document.getElementById('btn-run-thermal').click()`
        });

        await new Promise(r => setTimeout(r, 600));

        evalRes = await send('Runtime.evaluate', {
            expression: `({
                badge: document.getElementById('thermal-status-badge').innerText,
                maxTemp: document.getElementById('res-thermal-max-temp').innerText,
                elements: document.getElementById('res-thermal-elements').innerText,
                loc: document.getElementById('res-thermal-max-temp-loc').innerText,
                maxError: document.getElementById('res-thermal-max-error').innerText
            })`,
            returnByValue: true
        });
        console.log('COMPUTED NEW RESULTS DYNAMICALLY:');
        console.log(evalRes.result.value);

        ws.close();
    } finally {
        edge.kill();
    }
}

testThermalUI().catch(console.error);

