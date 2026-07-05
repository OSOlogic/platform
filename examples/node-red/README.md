# Node-RED examples

Import a flow via **☰ → Import** in Node-RED, then Deploy.

- **[01-blink-led.json](01-blink-led.json)** — an `inject` every second → a `function` that toggles 0/1 →
  an `http request` that `PUT`s the value to the osodb tag. The node's status shows ON/off.

On a wired OSOLogic install, Node-RED also has the DB-mirror nodes (the `rtmirror` views over `tags`),
so a flow can read/write set-points directly in MariaDB instead of over REST. Same tags either way.
