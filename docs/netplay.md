# Netplay

RetroPvP uses RetroArch's built-in Netplay. Both players must run the same RetroPvP frontend, core revision, and matching ROM. The session synchronizes inputs; the director runs inside the core so player 2's actions are part of the same emulated frame stream.

## Host and join

Use the window's RetroPvP menu:

- Host Directly uses direct connectivity and needs an internet-reachable host port, usually through router port mapping.
- Host via Relay routes the session through the selected RetroArch relay.
- Copy Host Address copies the current direct address or relay invite.
- Join Game… accepts the host address or relay invite.
- Disconnect ends the current Netplay session.

## Relay settings

Choose Relay Region… before hosting via relay. The supported built-in locations are New York, Western Europe (Madrid), Southeast Brazil, and Southeast Asia. The Custom option accepts a relay host or host|port. A custom relay must implement RetroArch's Netplay MITM protocol and be reachable by both peers.

The project does not operate a public relay service. Relay location changes the network route and may affect latency; successful connection does not by itself verify rollback stability or gameplay quality.
