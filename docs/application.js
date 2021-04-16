(function () {
    'use strict';

    // init() once the page has finished loading.
    window.onload = init;

    var filter_count = 7;
    var filters = [];
    var live_update = false;

    var config_dict = JSON.parse(
        '{"volume": 0.7, "filter_type": 1, "filter_count": 7, "filter_fc": [ 1000, 1000, 1000, 1000, 1000, 1000,1000 ], "filter_db": [ 0, 0, 0, 0, 0, 0, 0 ], "filter_q": [ 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1 ], "enhance_bass": false, "enhance_bass_lr_vol": 1, "enhance_bass_bass_vol": 0.3, "enhance_bass_high_pass": 0, "enhance_bass_cutoff": 4 }'
    );

    var canvas;
    var canvasContext;
    var canvasWidth = 0;
    var canvasHeight = 0;

    var curveColor = "rgb(224,27,106)";
    var gridColor = "rgb(100,100,100)";
    var textColor = "rgb(81,127,207)";

    var dbScale = 14;
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

        var noctaves = 9;

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

    function frequencyHandler(event, ui, i) {
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

    function resonanceHandler(event, ui, i) {
        var value = Math.floor(ui.value * 100) / 100;
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = "Q = " + value + " dB";
        var filter = filters[i]
        filter.Q.value = value;
        config_dict.filter_q[i] = parseFloat(value);
        drawCurve();
    }

    function gainHandler(event, ui, i) {
        var value = Math.floor(ui.value * 100) / 100;
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = "gain = " + value;
        var filter = filters[i]
        filter.gain.value = value;
        config_dict.filter_db[i] = parseFloat(value);
        drawCurve();
    }

    function volumeHandler(event, ui, i) {
        var value = Math.floor(ui.value * 100) / 100;
        var info = document.getElementById(ui.id + '-value');
        info.innerHTML = value;
        config_dict.volume = parseFloat(value);
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
        handler(0, slider, i);
        slider.oninput = function () { handler(0, slider, i); };
    }

    function processReceivedText(text) {
        var receiver_lines = document.querySelector("#receiver_lines");
        if (receiver_lines) {
            receiver_lines.innerHTML = receiver_lines.innerHTML + text + '</br>';
        }
        config_dict = JSON.parse(text);
        processConfigDict();
    }

    function processConfigDict() {
        if (config_dict) {
            console.log(config_dict);
            if (filter_count != config_dict.filter_count) {
                console.log('Number of filters must match with the DSP config!');
                return;
            }
            configureFilterSliders(filter_count);
            configureSlider("volume", 0, config_dict.volume, 0, 0.8, volumeHandler);
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
                sendJson = function(text) {
                    port.send(new TextEncoder('utf-8').encode(''));
                    port.send(new TextEncoder('utf-8').encode(text));
                }
                
                var text_output = '';

                port.onReceive = data => {
                    let textDecoder = new TextDecoder();
                    let text = textDecoder.decode(data);
                    if (text.includes('<<EOF>>')) {
                        var text_split = text.split('<<EOF>>');
                        text_output = text_output + text_split[0];
                        console.log(text_output);
                        processReceivedText(text_output);
                        text_output = text_split[1] || '';
                    } else {
                        text_output = text_output + text;
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

        saveflashButton.addEventListener('click', function () {
            sendJson('{"save_to_flash":true}');
        });
    }

    // Empty if not connected
    function sendJson() {}

    function init() {
        initAudio();
        initWebUSB();

        canvas = document.getElementById('canvasID');
        canvasContext = canvas.getContext('2d');
        canvasWidth = parseFloat(window.getComputedStyle(canvas, null).width);
        canvasHeight = parseFloat(window.getComputedStyle(canvas, null).height);

        addFitlerSliders(filter_count);
        configureFilterSliders(filter_count);
        configureSlider("volume", 0, config_dict.volume, 0, 0.8, volumeHandler);

        drawCurve();
    }

})();
