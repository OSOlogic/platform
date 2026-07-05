// Blink LED — Node.js. Toggle an osodb tag once per second over REST.
const BASE = process.env.OSO_URL || "http://127.0.0.1:8080";
const TAG  = process.env.OSO_TAG || "hass.switch.led";
let state = 0;
setInterval(async () => {
  state ^= 1;
  await fetch(`${BASE}/var/${TAG}`, { method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ value: state }) });
  console.log(`${TAG} = ${state}`);
}, 1000);
