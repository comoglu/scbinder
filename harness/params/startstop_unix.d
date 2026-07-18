#
# Minimal startstop for the scbinder regression harness:
# just the rings and binder_ew itself (no statmgr, no pickers).
#
nRing  2
Ring   PICK_RING    1024
Ring   STATUS_RING  1024

MyModuleId    MOD_STARTSTOP
HeartbeatInt  30
MyClassName   TS
MyPriority     0
LogFile        1
KillDelay      5
HardKillDelay  10

Process        "binder_ew binder_ew.d"
Class/Priority NONE 0
