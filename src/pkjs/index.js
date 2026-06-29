var STORAGE_KEY_TEMPERATURE = 'lastTemperature';
var STORAGE_KEY_AUTO_ROTATE = 'autoRotate';
var STORAGE_KEY_FIXED_IMAGE_INDEX = 'fixedImageIndex';
var STORAGE_KEY_BG_COLOR = 'bgColor';
var PLACEHOLDER_TEMPERATURE = '--°';
var EMBLEM_LABELS = [
  'Champions of Fenris',
  'Bloodmaws',
  'Seawolves',
  'Sons of Morkai',
  'Red Moons',
  'Deathwolves',
  'Stormwolves',
  'Ironwolves',
  'Drakeslayers',
  'Blackmanes',
  'Firehowlers',
  'Grimbloods'
];
var BG_COLOR_OPTIONS = [
  { value: 0, label: 'Fenrisian Grey' },
  { value: 1, label: 'Russ Grey' },
  { value: 2, label: 'The Fang' },
  { value: 3, label: 'White' }
];
var TEMPERATURE_REFRESH_MS = 15 * 60 * 1000;

var lastTemperature = localStorage.getItem(STORAGE_KEY_TEMPERATURE);
var refreshTimer = null;
var temperatureRequestInFlight = false;
var pendingAppMessages = [];
var appMessageInFlight = false;

function getAutoRotateSetting() {
  return localStorage.getItem(STORAGE_KEY_AUTO_ROTATE) !== 'false';
}

function getFixedImageIndexSetting() {
  var value = parseInt(localStorage.getItem(STORAGE_KEY_FIXED_IMAGE_INDEX) || '0', 10);
  if (isNaN(value) || value < 0 || value >= EMBLEM_LABELS.length) {
    return 0;
  }
  return value;
}

function getBackgroundColorSetting() {
  var value = parseInt(localStorage.getItem(STORAGE_KEY_BG_COLOR) || '0', 10);
  if (isNaN(value) || value < 0 || value >= BG_COLOR_OPTIONS.length) {
    return 0;
  }
  return value;
}

function isWeatherRequest(payload) {
  return payload && (payload[1] === 1 || payload.WEATHER_REQUEST === 1 || payload.REQUEST_WEATHER === 1);
}

function flushPendingAppMessage() {
  var payload;

  if (appMessageInFlight || pendingAppMessages.length === 0) {
    return;
  }

  payload = pendingAppMessages.shift();
  appMessageInFlight = true;

  Pebble.sendAppMessage(payload, function() {
    appMessageInFlight = false;
    flushPendingAppMessage();
  }, function(error) {
    console.log('send failed: ' + JSON.stringify(error));
    appMessageInFlight = false;
    pendingAppMessages.unshift(payload);
    setTimeout(flushPendingAppMessage, 1000);
  });
}

function queueAppMessage(payload) {
  pendingAppMessages.push(payload);
  flushPendingAppMessage();
}

function sendTemperature(value) {
  lastTemperature = value;
  if (value !== PLACEHOLDER_TEMPERATURE) {
    localStorage.setItem(STORAGE_KEY_TEMPERATURE, value);
  }
  console.log('queue temperature: ' + value);
  queueAppMessage({
    0: value
  });
}

function sendSettings() {
  console.log('queue settings');
  queueAppMessage({
    2: getAutoRotateSetting() ? 1 : 0,
    3: getFixedImageIndexSetting(),
    4: getBackgroundColorSetting()
  });
}

function requestTemperature() {
  if (temperatureRequestInFlight) {
    return;
  }

  temperatureRequestInFlight = true;

  if (!navigator.geolocation) {
    if (lastTemperature === null) {
      sendTemperature(PLACEHOLDER_TEMPERATURE);
    }
    temperatureRequestInFlight = false;
    return;
  }

  navigator.geolocation.getCurrentPosition(function(position) {
    var latitude = position.coords.latitude;
    var longitude = position.coords.longitude;
    var url = 'https://api.open-meteo.com/v1/forecast?latitude=' +
      encodeURIComponent(latitude) +
      '&longitude=' +
      encodeURIComponent(longitude) +
      '&current=temperature_2m&temperature_unit=celsius' +
      '&refresh=' + Date.now();

    var xhr = new XMLHttpRequest();
    xhr.onload = function() {
      try {
        var response = JSON.parse(xhr.responseText);
        if (response && response.current && typeof response.current.temperature_2m === 'number') {
          sendTemperature(Math.round(response.current.temperature_2m) + '°');
          temperatureRequestInFlight = false;
          return;
        }
      } catch (e) {
        console.log('weather parse failed: ' + e);
      }

      if (lastTemperature === null) {
        sendTemperature(PLACEHOLDER_TEMPERATURE);
      }
      temperatureRequestInFlight = false;
    };

    xhr.onerror = function() {
      if (lastTemperature === null) {
        sendTemperature(PLACEHOLDER_TEMPERATURE);
      }
      temperatureRequestInFlight = false;
    };

    xhr.open('GET', url);
    xhr.setRequestHeader('Cache-Control', 'no-cache');
    xhr.setRequestHeader('Pragma', 'no-cache');
    xhr.send();
  }, function(error) {
    console.log('geolocation failed: ' + JSON.stringify(error));
    if (lastTemperature === null) {
      sendTemperature(PLACEHOLDER_TEMPERATURE);
    }
    temperatureRequestInFlight = false;
  }, {
    enableHighAccuracy: false,
    maximumAge: 60 * 1000,
    timeout: 15000
  });
}

function ensureTemperatureRefreshLoop() {
  if (refreshTimer !== null) {
    return;
  }

  refreshTimer = setInterval(function() {
    requestTemperature();
  }, TEMPERATURE_REFRESH_MS);
}

function buildConfigPage() {
  var autoRotateChecked = getAutoRotateSetting();
  var fixedIndex = getFixedImageIndexSetting();
  var bgColor = getBackgroundColorSetting();
  var optionsHtml = EMBLEM_LABELS.map(function(label, index) {
    return '<label class="option">' +
      '<input type="radio" name="fixedIndex" value="' + index + '"' +
      (index === fixedIndex ? ' checked' : '') + '>' +
      '<span>' + label + '</span>' +
      '</label>';
  }).join('');
  var bgOptionsHtml = BG_COLOR_OPTIONS.map(function(option) {
    return '<label class="option">' +
      '<input type="radio" name="bgColor" value="' + option.value + '"' +
      (option.value === bgColor ? ' checked' : '') + '>' +
      '<span>' + option.label + '</span>' +
      '</label>';
  }).join('');

  return '<!doctype html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width, initial-scale=1">' +
    '<title>Logan Watchface</title>' +
    '<style>' +
    'body{font-family:-apple-system,BlinkMacSystemFont,Helvetica,Arial,sans-serif;background:#cedee7;color:#585673;margin:0;padding:16px;}' +
    'h1{font-size:22px;margin:0 0 12px;}h2{font-size:16px;margin:20px 0 8px;}' +
    '.card{background:rgba(255,255,255,.55);border-radius:12px;padding:14px;box-shadow:0 1px 4px rgba(0,0,0,.08);}' +
    '.option{display:flex;align-items:flex-start;gap:10px;padding:8px 0;font-size:15px;}' +
    '.actions{display:flex;gap:10px;margin-top:18px;}' +
    'button{border:0;border-radius:999px;padding:12px 16px;font-size:15px;font-weight:600;}' +
    '.save{background:#585673;color:#fff;}.cancel{background:#fff;color:#585673;}' +
    '.hint{font-size:13px;opacity:.8;line-height:1.4;margin-top:8px;}' +
    '</style></head><body><h1>Logan Watchface</h1>' +
    '<div class="card">' +
    '<h2>Rotation Mode</h2>' +
    '<label class="option"><input type="radio" name="mode" value="auto"' + (autoRotateChecked ? ' checked' : '') + '><span>Auto Rotate</span></label>' +
    '<label class="option"><input type="radio" name="mode" value="fixed"' + (!autoRotateChecked ? ' checked' : '') + '><span>Fixed Image</span></label>' +
    '<div class="hint">Auto Rotate changes every 5 minutes. Fixed Image keeps one image selected below.</div>' +
    '<h2>Fixed Image Choice</h2>' +
    optionsHtml +
    '<h2>Background</h2>' +
    bgOptionsHtml +
    '<div class="actions">' +
    '<button class="save" id="save">Save</button>' +
    '<button class="cancel" id="cancel" type="button">Cancel</button>' +
    '</div></div>' +
    '<script>' +
    'document.getElementById("save").addEventListener("click",function(){' +
    'var mode=document.querySelector(\'input[name="mode"]:checked\').value;' +
    'var fixed=document.querySelector(\'input[name="fixedIndex"]:checked\').value;' +
    'var bg=document.querySelector(\'input[name="bgColor"]:checked\').value;' +
    'var result={autoRotate:mode==="auto",fixedImageIndex:parseInt(fixed,10),bgColor:parseInt(bg,10)};' +
    'document.location="pebblejs://close#" + encodeURIComponent(JSON.stringify(result));' +
    '});' +
    'document.getElementById("cancel").addEventListener("click",function(){document.location="pebblejs://close#";});' +
    '</script></body></html>';
}

Pebble.addEventListener('ready', function() {
  console.log('pkjs ready');
  sendSettings();
  ensureTemperatureRefreshLoop();
  requestTemperature();
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('appmessage received: ' + JSON.stringify(e.payload || {}));
  if (isWeatherRequest(e.payload)) {
    requestTemperature();
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('data:text/html,' + encodeURIComponent(buildConfigPage()));
});

Pebble.addEventListener('webviewclosed', function(e) {
  var config;

  if (!e.response) {
    return;
  }

  try {
    config = JSON.parse(decodeURIComponent(e.response));
  } catch (error) {
    console.log('config parse failed: ' + error);
    return;
  }

  localStorage.setItem(STORAGE_KEY_AUTO_ROTATE, config.autoRotate ? 'true' : 'false');
  localStorage.setItem(STORAGE_KEY_FIXED_IMAGE_INDEX, String(config.fixedImageIndex));
  localStorage.setItem(STORAGE_KEY_BG_COLOR, String(config.bgColor));
  sendSettings();
});
