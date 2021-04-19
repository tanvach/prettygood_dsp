(function () {
    'use strict';

    // init() once the page has finished loading.
    window.onload = init;

    var filter_count = 7;
    var filters = [];
    var live_update = false;

    // Default when there's no USB connection
    var config_dict = JSON.parse(`
        {
            "volume": 0,
            "filter_type": 1,
            "filter_count": 7,
            "filter_fc": [ 1000, 1000, 1000, 1000, 1000, 1000, 1000 ],
            "filter_db": [ 0, 0, 0, 0, 0, 0, 0 ],
            "filter_q": [ 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 ]
        }
    `);

    var canvas;
    var canvasContext;
    var canvasWidth = 0;
    var canvasHeight = 0;

    var curveColor = "rgb(224,27,106)";
    var gridColor = "rgb(100,100,100)";
    var textColor = "rgb(81,127,207)";

    var dbScale = 14;
    var noctaves = 9;
    var pixelsPerDb;
    var width;
    var height;

    // Start off by initializing a new context.
    var context = new (window.AudioContext || window.webkitAudioContext)();

    if (!context.createGain)
        context.createGain = context.createGainNode;
    if (!context.createDelay)
        context.createDelay = context.createDelayNode;
    if (!context.createScriptProcessor)
        context.createScriptProcessor = context.createJavaScriptNode;

    // shim layer with setTimeout fallback
    window.requestAnimFrame = (function () {
        return window.requestAnimationFrame ||
            window.webkitRequestAnimationFrame ||
            window.mozRequestAnimationFrame ||
            window.oRequestAnimationFrame ||
            window.msRequestAnimationFrame ||
            function (callback) {
                window.setTimeout(callback, 1000 / 60);
            };
    })();

    function sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
    
    function dbToY(db) {
        var y = (0.5 * height) - pixelsPerDb * db;
        return y;
    }

    function drawCurve() {

        // draw center
        width = canvas.width;
        height = canvas.height;

        canvasContext.clearRect(0, 0, width, height);

        canvasContext.strokeStyle = curveColor;
        canvasContext.lineWidth = 3;
        canvasContext.beginPath();
        canvasContext.moveTo(0, 0);

        pixelsPerDb = (0.5 * height) / dbScale;

        var frequencyHz = new Float32Array(width);
        var nyquist = 0.5 * context.sampleRate;
        // First get response.
        for (var i = 0; i < width; ++i) {
            var f = i / width;

            // Convert to log frequency scale (octaves).
            f = nyquist * Math.pow(2.0, noctaves * (f - 1.0));
            frequencyHz[i] = f;
        }

        var magResponse = new Float32Array(width);
        var phaseResponse = new Float32Array(width);

        for (var i = 0; i < filter_count; ++i) {
            var magResponse_i = new Float32Array(width);
            filters[i].getFrequencyResponse(frequencyHz, magResponse_i, phaseResponse);
            for (var j = 0; j < width; ++j) {
                if (i == 0) {
                    magResponse[j] = magResponse_i[j];
                } else {
                    magResponse[j] *= magResponse_i[j];
                }
            }
        }

        for (var i = 0; i < width; ++i) {
            var response = magResponse[i];
            var dbResponse = 20.0 * Math.log(response) / Math.LN10;

            var x = i;
            var y = dbToY(dbResponse);

            if (i == 0)
                canvasContext.moveTo(x, y);
            else
                canvasContext.lineTo(x, y);
        }
        canvasContext.stroke();
        canvasContext.beginPath();
        canvasContext.lineWidth = 1;
        canvasContext.strokeStyle = gridColor;

        // Draw frequency scale.
        for (var octave = 0; octave <= noctaves; octave++) {
            var x = octave * width / noctaves;

            canvasContext.strokeStyle = gridColor;
            canvasContext.moveTo(x, 30);
            canvasContext.lineTo(x, height);
            canvasContext.stroke();

            var f = nyquist * Math.pow(2.0, octave - noctaves);
            var value = f.toFixed(0);
            var unit = 'Hz';
            if (f > 1000) {
                unit = 'KHz';
                value = (f / 1000).toFixed(1);
            }
            canvasContext.textAlign = "center";
            canvasContext.strokeStyle = textColor;
            canvasContext.strokeText(value + unit, x, 20);
        }

        // Draw 0dB line.
        canvasContext.beginPath();
        canvasContext.moveTo(0, 0.5 * height);
        canvasContext.lineTo(width, 0.5 * height);
        canvasContext.stroke();

        // Draw decibel scale.

        for (var db = -dbScale; db < dbScale - 2; db += 2) {
            var y = dbToY(db);
            canvasContext.strokeStyle = textColor;
            canvasContext.strokeText(db.toFixed(0) + "dB", width - 40, y);
            canvasContext.strokeStyle = gridColor;
            canvasContext.beginPath();
            canvasContext.moveTo(0, y);
            canvasContext.lineTo(width, y);
            canvasContext.stroke();
        }
    }

    // Sliders
    function frequencyHandler(event, type, ui, i) {
        var value = ui.value;
        var nyquist = context.sampleRate * 0.5;
        var noctaves = Math.log(nyquist / 10.0) / Math.LN2;
        var v2 = Math.pow(2.0, noctaves * (value - 1.0));
        var cutoff = Math.floor(v2 * nyquist);
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = "fc = " + cutoff + " Hz";
        var filter = filters[i]
        filter.frequency.value = cutoff;
        config_dict.filter_fc[i] = parseFloat(cutoff);
        drawCurve();

    }

    function resonanceHandler(event, type, ui, i) {
        var value = Math.floor(ui.value * 100) / 100;
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = "Q = " + value + " dB";
        var filter = filters[i]
        filter.Q.value = value;
        config_dict.filter_q[i] = parseFloat(value);
        drawCurve();
    }

    function gainHandler(event, type, ui, i) {
        var value = Math.floor(ui.value * 100) / 100;
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = "gain = " + value;
        var filter = filters[i]
        filter.gain.value = value;
        config_dict.filter_db[i] = parseFloat(value);
        drawCurve();
    }

    function sliderHandler(event, type, ui, i) {
        var value = Math.floor(ui.value * 100) / 100;
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = value;
        config_dict[type] = parseFloat(value);
        drawCurve();
    }

    function createSlider(controls, name, width, text_width) {
        var sliderText = '<div> '
            + '<input id="' + name + 'Slider" '
            + 'type="range" min="0" max="1" step="0.001" value="0" style="height: 10px; width: ' + width + 'px;"> '
            + '<span id="' + name + 'Slider-value" style="display: inline-block; width: ' + text_width + 'px;">'
            + name
            + '</span> </div>';
        return sliderText;
    }

    function addFitlerSliders(n) {
        var controls = document.getElementById("controls");
        for (var i = 0; i < n; ++i) {
            controls.innerHTML = controls.innerHTML
                + '<div style="display: flex;"> Filter ' + i
                + createSlider(controls, 'frequency' + i, 200, 130)
                + createSlider(controls, 'gain' + i, 200, 90)
                + createSlider(controls, 'Q' + i, 200, 90)
                + '</div>';
        }
    }

    function configureFilterSliders(n) {

        var nyquist = context.sampleRate * 0.5;
        var noctaves = Math.log(nyquist / 10.0) / Math.LN2;

        for (var i = 0; i < n; ++i) {
            var fc_v = Math.log(config_dict.filter_fc[i] / nyquist) / Math.log(2) / noctaves + 1;
            configureSlider('frequency', i, fc_v || .6, 0.2, 0.98, frequencyHandler);
            configureSlider("gain", i, config_dict.filter_db[i] || 0.0, -10, 10, gainHandler);
            configureSlider("Q", i, config_dict.filter_q[i] || 1.0, 0, 7, resonanceHandler);
        }
    }

    function configureSlider(type, i, value, min, max, handler) {
        var slider = document.getElementById(type + i + "Slider");
        slider.min = min;
        slider.max = max;
        slider.value = value;
        handler(0, type, slider, i);
        slider.oninput = function () { handler(0, type, slider, i); };
    }

    // Check boxes
    function checkboxHandler(event, type, ui) {
        var value = ui.checked;
        config_dict[type] = value;
    }

    function configureCheckbox(type, value, handler) {
        var checkbox = document.getElementById(type + "Checkbox");
        checkbox.checked = value;
        handler(0, type, checkbox);
        checkbox.oninput = function () { handler(0, type, checkbox); };
    }

    // Selectors
    function selectorHandler(event, type, ui) {
        var value = ui.value;
        config_dict[type] = parseInt(value);
        drawCurve();
    }

    function configureSelector(type, value, handler) {
        var selector = document.getElementById(type + "Selector");
        selector.value = value;
        handler(0, type, selector);
        selector.oninput = function () { handler(0, type, selector); };
    }

    function configUIElements() {
        configureFilterSliders(filter_count);
        configureSlider("volume", 0, config_dict.volume, 0, 0.8, sliderHandler);

        configureCheckbox("enhance_bass", config_dict.enhance_bass, checkboxHandler);
        configureSelector("enhance_bass_cutoff", config_dict.enhance_bass_cutoff, selectorHandler);
        configureSelector("enhance_bass_high_pass", config_dict.enhance_bass_high_pass, selectorHandler);
        configureSlider("enhance_bass_lr_vol", 0, config_dict.enhance_bass_lr_vol, 0, 1, sliderHandler);
        configureSlider("enhance_bass_bass_vol", 0, config_dict.enhance_bass_bass_vol, 0, 1, sliderHandler);

        configureCheckbox("auto_volume", config_dict.auto_volume, checkboxHandler);
        configureSelector("auto_volume_max_gain", config_dict.auto_volume_max_gain, selectorHandler);
        configureSelector("auto_volume_lbi_response", config_dict.auto_volume_lbi_response, selectorHandler);
        configureSelector("auto_volume_hard_limit", config_dict.auto_volume_hard_limit, selectorHandler);
        configureSlider("auto_volume_threshold", 0, config_dict.auto_volume_threshold, -96, 0, sliderHandler);
        configureSlider("auto_volume_attack", 0, config_dict.auto_volume_attack, 0, 20, sliderHandler);
        configureSlider("auto_volume_decay", 0, config_dict.auto_volume_decay, 0, 20, sliderHandler);
        
        configureCheckbox("keep_usb_battery_on", config_dict.keep_usb_battery_on, checkboxHandler);
        configureCheckbox("keep_usb_battery_on_led", config_dict.keep_usb_battery_on_led, checkboxHandler);
        configureSelector("keep_usb_battery_on_pulse_period_sec", config_dict.keep_usb_battery_on_pulse_period_sec, selectorHandler);
        configureSelector("keep_usb_battery_on_pulse_duration_msec", config_dict.keep_usb_battery_on_pulse_duration_msec, selectorHandler);
    }

    function processReceivedText(text) {
        var receiver_lines = document.querySelector("#receiver_lines");
        if (receiver_lines) {
            receiver_lines.innerHTML = receiver_lines.innerHTML + text + '</br>';
        }
        try {
            var received_dict = JSON.parse(text);
            config_dict = received_dict;
            processConfigDict();
        } catch (error) {
            console.log(error);
        }
    }

    function processConfigDict() {
        if (config_dict) {
            console.log(config_dict);
            if (filter_count != config_dict.filter_count) {
                console.log('Number of filters must match with the DSP config!');
                return;
            }
            // Reload UI element states
            configUIElements();
        }
    }


    function initAudio() {
        for (var i = 0; i < filter_count; i++) {
            var filter = context.createBiquadFilter();
            filter.type = "peaking";
            filters.push(filter);
        }
    }

    function initWebUSB() {
        var applyButton = document.querySelector("#apply");
        var reloaddefaultsButton = document.querySelector("#reloaddefaults");
        var saveflashButton = document.querySelector("#saveflash");
        var connectButton = document.querySelector("#connect");
        var statusDisplay = document.querySelector('#status');
        var port;

        function connect() {
            port.connect().then(() => {
                statusDisplay.textContent = '';
                connectButton.textContent = 'Disconnect';

                // Override function
                sendJson = function (text) {
                    port.send(new TextEncoder('utf-8').encode(''));
                    port.send(new TextEncoder('utf-8').encode(text));
                }

                var text_buffer = '';

                port.onReceive = data => {
                    var textDecoder = new TextDecoder();
                    var text = textDecoder.decode(data);
                    console.log(text);
                    text_buffer = text_buffer + text;
                    if (text_buffer.includes('<<EOF>>')) {
                        var text_split = text_buffer.split('<<EOF>>');
                        console.log(text_split);
                        processReceivedText(text_split[0]);
                        text_buffer = text_split[1] || '';
                    }
                };
                port.onReceiveError = error => {
                    console.error(error);
                };
            }, error => {
                statusDisplay.textContent = error;
            });
        }

        connectButton.addEventListener('click', function () {
            if (port) {
                port.disconnect();
                connectButton.textContent = 'Connect';
                statusDisplay.textContent = '';
                port = null;
            } else {
                serial.requestPort().then(selectedPort => {
                    port = selectedPort;
                    connect();
                }).catch(error => {
                    statusDisplay.textContent = error;
                });
            }
        });

        serial.getPorts().then(ports => {
            if (ports.length === 0) {
                statusDisplay.textContent = 'No device found.';
            } else {
                statusDisplay.textContent = 'Connecting...';
                port = ports[0];
                connect();
            }
        });

        applyButton.addEventListener('click', function () {
            sendJson(JSON.stringify(config_dict));
        });

        reloaddefaultsButton.addEventListener('click', function () {
            sendJson('{"reload_defaults":true}');
        });

        saveflashButton.addEventListener('click', async function () {
            sendJson(JSON.stringify(config_dict));
            await sleep(100);
            sendJson('{"save_to_flash":true}');
        });
    }

    // Empty if not connected
    function sendJson() { }

    function init() {
        initAudio();
        initWebUSB();

        canvas = document.getElementById('canvasID');
        canvasContext = canvas.getContext('2d');
        canvasWidth = parseFloat(window.getComputedStyle(canvas, null).width);
        canvasHeight = parseFloat(window.getComputedStyle(canvas, null).height);

        addFitlerSliders(filter_count);
        configUIElements();

        drawCurve();
    }

})();
