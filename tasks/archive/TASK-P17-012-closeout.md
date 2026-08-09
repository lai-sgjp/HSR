# TASK-P17-012 - Uniform Frontend Module Dynamic Mount

Status: `PASS / USER ACCEPTED`

Party, Map, Challenge, Quest, and Save are created by
`UHSRUIManagerSubsystem` and mounted as the single active child of
`ModuleContentHost`. The frontend root no longer relies on pre-placed module
instances or Visibility switching. Character and Inventory keep their
dedicated accepted paths.

Codex evidence recorded a successful `HSREditor Win64 Development` build,
`HSR.UI.FrontendDynamicMount` 3/3, the Challenge Directory and Frontend
Navigation regression set 14/14, and `git diff --check` success. The user
confirmed all five dynamic module routes in Editor/PIE, including the
Challenge Directory display issue.

The existing `HSR.UI.ScreenLifecycle.TravelRestore` inventory-bind failure is
retained as a pre-existing regression boundary and is outside TASK-P17-012.
Packaged/Shipping, physical-controller, multi-resolution, network, and
independent-review evidence were not run in this closeout.
