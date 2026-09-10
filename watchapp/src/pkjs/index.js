// PebbleKit JS: relays dictated text from the watch to an n8n webhook, and relays the
// result back to the watch. Config (webhook URL + auth token) lives in phone-side
// localStorage only -- the watch never needs it. See ../../../PLAN.md for the full design.

var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
// autoHandleEvents: false -- we don't want Clay relaying webhookUrl/authToken to the
// watch via AppMessage (the watch never uses them, only this file does), so we handle
// 'showConfiguration'/'webviewclosed' ourselves and persist to localStorage instead.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var STORAGE_KEY_WEBHOOK_URL = 'webhookUrl';
var STORAGE_KEY_AUTH_TOKEN = 'authToken';
var REQUEST_TIMEOUT_MS = 10000;
var MAX_RESULT_MESSAGE_LENGTH = 60;

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;
  }

  var settings = clay.getSettings(e.response, false);
  if (settings.webhookUrl) {
    localStorage.setItem(STORAGE_KEY_WEBHOOK_URL, settings.webhookUrl.value);
  }
  if (settings.authToken) {
    localStorage.setItem(STORAGE_KEY_AUTH_TOKEN, settings.authToken.value);
  }
});

function sendResultToWatch(success, message) {
  var dict = { 'resultStatus': success ? 1 : 0 };
  if (message) {
    dict['resultMessage'] = String(message).substring(0, MAX_RESULT_MESSAGE_LENGTH);
  }
  Pebble.sendAppMessage(dict, function() {
    console.log('Delta Notes: result relayed to watch (' + (success ? 'success' : 'failure') + ')');
  }, function(err) {
    console.log('Delta Notes: failed to relay result to watch: ' + JSON.stringify(err));
  });
}

function postNote(noteText, webhookUrl, authToken) {
  var xhr = new XMLHttpRequest();

  xhr.onload = function() {
    if (xhr.status >= 200 && xhr.status < 300) {
      sendResultToWatch(true);
    } else {
      sendResultToWatch(false, 'Server error ' + xhr.status);
    }
  };
  xhr.onerror = function() {
    sendResultToWatch(false, 'Network error');
  };
  xhr.timeout = REQUEST_TIMEOUT_MS;
  xhr.ontimeout = function() {
    sendResultToWatch(false, 'Request timed out');
  };

  xhr.open('POST', webhookUrl);
  xhr.setRequestHeader('Content-Type', 'application/json');
  if (authToken) {
    xhr.setRequestHeader('X-Auth-Token', authToken);
  }
  xhr.send(JSON.stringify({
    text: noteText,
    timestamp: new Date().toISOString()
  }));
}

Pebble.addEventListener('appmessage', function(e) {
  var noteText = e.payload && e.payload.noteText;
  if (!noteText) {
    return;
  }

  var webhookUrl = localStorage.getItem(STORAGE_KEY_WEBHOOK_URL);
  if (!webhookUrl) {
    sendResultToWatch(false, 'Not configured');
    return;
  }

  var authToken = localStorage.getItem(STORAGE_KEY_AUTH_TOKEN);
  postNote(noteText, webhookUrl, authToken);
});

Pebble.addEventListener('ready', function() {
  console.log('Delta Notes: PebbleKit JS ready');
});
