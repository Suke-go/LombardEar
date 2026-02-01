// Simple Rolling Chart Class
class RollingChart {
    constructor(canvasId, maxPoints, colors, min, max) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.data = []; // Array of arrays (one for each line)
        this.colors = colors;
        this.maxPoints = maxPoints;
        this.min = min;
        this.max = max;
        this.width = this.canvas.width = this.canvas.clientWidth;
        this.height = this.canvas.height = this.canvas.clientHeight;

        // Init empty data
        for (let i = 0; i < colors.length; i++) this.data.push(new Array(maxPoints).fill(min));
    }

    add(values) { // values is array [v1, v2...]
        for (let i = 0; i < this.data.length; i++) {
            this.data[i].shift();
            this.data[i].push(values[i]);
        }
        this.draw();
    }

    draw() {
        this.ctx.clearRect(0, 0, this.width, this.height);

        // Grid
        this.ctx.strokeStyle = '#444';
        this.ctx.lineWidth = 1;
        this.ctx.beginPath();
        this.ctx.moveTo(0, this.height / 2); this.ctx.lineTo(this.width, this.height / 2);
        this.ctx.stroke();

        for (let line = 0; line < this.data.length; line++) {
            this.ctx.strokeStyle = this.colors[line];
            this.ctx.lineWidth = 2;
            this.ctx.beginPath();

            let range = this.max - this.min;
            for (let i = 0; i < this.maxPoints; i++) {
                let x = (i / (this.maxPoints - 1)) * this.width;
                let val = this.data[line][i];
                // Normalize
                let norm = (val - this.min) / range;
                // Flip Y
                let y = this.height - (norm * this.height);

                if (i === 0) this.ctx.moveTo(x, y);
                else this.ctx.lineTo(x, y);
            }
            this.ctx.stroke();
        }
    }
}

const chartLevels = new RollingChart('chart-levels', 100, ['#00E676', '#2979FF', '#FF1744'], 0, 1.0); // L, R, B
const chartBeta = new RollingChart('chart-beta', 100, ['#FFD600'], -2.0, 2.0);
const chartMu = new RollingChart('chart-mu', 100, ['#d500f9'], 0, 0.05);
const chartError = new RollingChart('chart-error', 100, ['#00E5FF'], 0, 1.0);

const wsUrl = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws';
let socket;

function connect() {
    socket = new WebSocket(wsUrl);
    const statusEl = document.getElementById('status');

    socket.onopen = () => {
        statusEl.textContent = "Connected";
        statusEl.className = "status connected";
    };

    socket.onclose = () => {
        statusEl.textContent = "Disconnected (Retrying...)";
        statusEl.className = "status disconnected";
        setTimeout(connect, 2000);
    };

    socket.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);

            // Handle device list message
            if (msg.type === 'devices' && msg.list) {
                const sel = document.getElementById('sel-output-device');
                sel.innerHTML = '';
                msg.list.forEach(dev => {
                    const opt = document.createElement('option');
                    opt.value = dev.id;
                    opt.textContent = dev.name;
                    sel.appendChild(opt);
                });
                return;
            }

            chartLevels.add([msg.l, msg.r, msg.b]);
            chartError.add([msg.e]);
            chartBeta.add([msg.beta]);
            chartMu.add([msg.mu]);

            document.getElementById('val-beta').innerText = msg.beta.toFixed(3);
            document.getElementById('val-mu').innerText = msg.mu.toFixed(5);
        } catch (e) { console.error(e); }
    };
}

connect();

// Controls
function sendParams() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        const alpha = parseFloat(document.getElementById('sld-alpha').value);
        const leak = parseFloat(document.getElementById('sld-leak').value);
        const muMax = parseFloat(document.getElementById('sld-mumax').value);

        // DSP Params
        const aecOn = document.getElementById('chk-aec').checked ? 1 : 0;
        const agcOn = document.getElementById('chk-agc').checked ? 1 : 0;
        const ngOn = document.getElementById('chk-ng').checked ? 1 : 0;
        const agcTarget = parseFloat(document.getElementById('sld-agc-target').value);
        const ngThresh = parseFloat(document.getElementById('sld-ng-thresh').value);

        document.getElementById('lbl-alpha').innerText = alpha;
        document.getElementById('lbl-leak').innerText = leak;
        document.getElementById('lbl-mumax').innerText = muMax;
        document.getElementById('lbl-agc-target').innerText = agcTarget;
        document.getElementById('lbl-ng-thresh').innerText = ngThresh;

        socket.send(JSON.stringify({
            alpha: alpha,
            leak: leak,
            mu_max: muMax,
            aec_on: aecOn,
            agc_on: agcOn,
            ng_on: ngOn,
            agc_target: agcTarget,
            ng_thresh: ngThresh
        }));
    }
}

document.getElementById('sld-alpha').addEventListener('input', sendParams);
document.getElementById('sld-leak').addEventListener('input', sendParams);
document.getElementById('sld-mumax').addEventListener('input', sendParams);

// DSP Listeners
document.getElementById('chk-aec').addEventListener('change', sendParams);
document.getElementById('chk-agc').addEventListener('change', sendParams);
document.getElementById('chk-ng').addEventListener('change', sendParams);
document.getElementById('sld-agc-target').addEventListener('input', sendParams);
document.getElementById('sld-ng-thresh').addEventListener('input', sendParams);

// Output device change handler
document.getElementById('sel-output-device').addEventListener('change', (e) => {
    if (socket && socket.readyState === WebSocket.OPEN) {
        const deviceId = parseInt(e.target.value, 10);
        socket.send(JSON.stringify({ set_output_device: deviceId }));
    }
});
