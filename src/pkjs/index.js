var STORAGE_KEY_TEMPERATURE = 'lastTemperature';
var PLACEHOLDER_TEMPERATURE = '--°';
var lastTemperature = localStorage.getItem(STORAGE_KEY_TEMPERATURE);

function sendTemperature(value) {
  lastTemperature = value;
  if (value !== PLACEHOLDER_TEMPERATURE) {
    localStorage.setItem(STORAGE_KEY_TEMPERATURE, value);
  }
  Pebble.sendAppMessage({
    0: value
  }, function() {
    console.log('temperature sent: ' + value);
  }, function(error) {
    console.log('send failed: ' + JSON.stringify(error));
  });
}

function requestTemperature() {
  if (!navigator.geolocation) {
    if (lastTemperature === null) {
      sendTemperature(PLACEHOLDER_TEMPERATURE);
    }
    return;
  }

  navigator.geolocation.getCurrentPosition(function(position) {
    var latitude = position.coords.latitude;
    var longitude = position.coords.longitude;
    var url = 'https://api.open-meteo.com/v1/forecast?latitude=' +
      encodeURIComponent(latitude) +
      '&longitude=' +
      encodeURIComponent(longitude) +
      '&current=temperature_2m&temperature_unit=celsius';

    var xhr = new XMLHttpRequest();
    xhr.onload = function() {
      try {
        var response = JSON.parse(xhr.responseText);
        if (response && response.current && typeof response.current.temperature_2m === 'number') {
          sendTemperature(Math.round(response.current.temperature_2m) + '°');
          return;
        }
      } catch (e) {
        console.log('weather parse failed: ' + e);
      }

      if (lastTemperature === null) {
        sendTemperature(PLACEHOLDER_TEMPERATURE);
      }
    };

    xhr.onerror = function() {
      if (lastTemperature === null) {
        sendTemperature(PLACEHOLDER_TEMPERATURE);
      }
    };

    xhr.open('GET', url);
    xhr.send();
  }, function(error) {
    console.log('geolocation failed: ' + JSON.stringify(error));
    if (lastTemperature === null) {
      sendTemperature(PLACEHOLDER_TEMPERATURE);
    }
  }, {
    enableHighAccuracy: false,
    maximumAge: 30 * 60 * 1000,
    timeout: 15000
  });
}

Pebble.addEventListener('ready', function() {
  console.log('pkjs ready');
  requestTemperature();
});

Pebble.addEventListener('appmessage', function(e) {
  console.log('appmessage received: ' + JSON.stringify(e.payload || {}));
  requestTemperature();
});
