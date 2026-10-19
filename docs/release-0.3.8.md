# MCM2:Reborn 0.3.8

Includes all [0.3.7 fixes and validation notes](release-0.3.7.md).

A final audit found that opening Multiplayer after Internet play could still switch providers and create a LAN network through `eos_online(FALSE)`. The modern menu now returns before either provider-switching or LAN initialization. Selecting LAN explicitly performs that switch, including when coming from Internet play. Two additional regression assertions cover this ordering. Back also closes a dropdown when its list has focus, with a native regression check for that path.

The executable and every menu asset are rebuilt as 0.3.8. The same physical-controller, extended-gameplay and live-network validation limits documented for 0.3.7 apply.
