Pebble.addEventListener("ready", function(e) {
    console.log("JS app ready");
});

Pebble.addEventListener("showConfiguration", function(e) {
    console.log("Showing config");

    Pebble.openURL("http://code.offend.me.uk/centrick_settings.html");
});

Pebble.addEventListener("webviewclosed", function(e) {
    var config = JSON.parse(decodeURIComponent(e.response));

    console.log("Config:" + JSON.stringify(config));

    Pebble.sendAppMessage({
        "invert": config.invert == "yes" && 1 || 0,
        "order": config.order == "in" && 1 || 0,
    }, function(e) {
        console.log("Delivered");
    }, function(e) {
        console.log("Failed: " + e.error.message);
    });
});
