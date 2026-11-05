# EBaC's containers

How to install and use them is in the main README: **[Installing with
Docker](../README.md#installing-with-docker)**, and, for Home Assistant,
**[the EBaC app](https://github.com/neilbrookins/ebac-ha-app)**.

This folder is for anyone changing them. Both containers are built from
these files, so a change here reaches both, and they never drift apart:

| File | What it is | Standalone image | Home Assistant app |
|---|---|---|---|
| `gestures.js` | the web page's taps, swipes and keyboard handling | yes | yes |
| `make-index.sh` | adds gestures.js to ttyd's own page, at build time | yes | yes |
| `ebac-web` | starts ttyd: the page, its colours, EBaC in it | yes | yes |
| `ebac-session` | starts EBaC for one visit, with that user's history | yes | yes |
| `ebac-setup` | makes EBaC's folder in `/data` at each start | yes | yes |
| `www/cgi-bin/` | runs ebac-api (and ebac-admin) for the web server | yes | yes |
| `Dockerfile` | the standalone image | yes | |
| `ebac-start` | the standalone image's start (its settings) | yes | |
| `docker-compose.yml` | an example, for Docker Compose users | yes | |

Only how each starts differs: here `ebac-start`, reading environment
variables; in the app, Home Assistant's services (its options, its login,
announcing ebac-api to Home Assistant), which call the same `ebac-web`.

- **The standalone image**, `ghcr.io/neilbrookins/ebac`, is built, tested
  and published by [.github/workflows/docker.yml](../.github/workflows/docker.yml)
  at each change to `main`, for amd64 and arm64.
- **The app** builds on each Home Assistant, from EBaC's source at the
  commit its Dockerfile pins (`EBAC_COMMIT`), these files included: a new
  app version, pinning a newer commit, brings both the program and these.

Both use the same Alpine release (the app's base image is Home
Assistant's, on that Alpine), so the same ttyd. When the app moves to a
newer base image, move `ALPINE` in the Dockerfile with it.
