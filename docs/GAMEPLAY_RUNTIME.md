# Recovered gameplay subset

The engine now has an experimental single-player gameplay path, beyond the
scene viewer. This is still a partial reconstruction. It does not execute the
full GE6 startup or provide original PhysX behavior.

## Launch and controls

Build, prepare the original resources, and package as described in the README.
The current local package is `build/FS15GameplayAlpha3/FS15MenuRebuilt.exe`.
Choose an empty Career slot, difficulty, map, and Start. Existing saved games
remain read-only and cannot resume a simulation. The original installation and
profile are never written.

Direct engine launch also starts a new experimental session. `--map Map02`
selects Westbridge Hills; Map01 is the default. `--scene` retains the earlier
free-camera viewer. Hidden rendering tests default to scene mode unless
`--play`, `--mission-test`, or a scenario is requested.

| Input | Action |
|---|---|
| WASD / ZQSD / arrows | Accelerate, reverse/brake, steer |
| Space | Handbrake |
| Tab | Select the next original motor vehicle |
| Mouse drag / wheel | Orbit / zoom the following camera |
| E | Attach/detach a nearby compatible implement |
| B | Start/stop the attached tool's work |
| R | Fill from the farm silo or refill a tool at a compatible supply trigger |
| I | Unload a trailer at a compatible original tipping station |
| O | Open/close the combine's original pipe animation |
| F8 | Switch following/free camera |
| F5 / F6 / F7 | Environment-hour preview / shadows |
| Escape | Close the session |

Use the tractor parked near the Brantner trailer to attach it. At the farm silo,
all four original silo trigger meshes must overlap the trailer before filling
is enabled. Grain transfer respects stock, capacity and type compatibility.
The combine transfers to a compatible trailer underneath the open pipe. Seed
supply uses the original station, tool capacity/rate, and fill-type price.

## Reused original behavior and resources

`prepare_mission.py` reads both original `careerVehicles_mapXX.xml` templates.
It stages eleven distinct configurations: thirteen starting objects on Map01
and twelve on Map02. Models, materials, wheels and attachments are original.
The derived runtime I3Ds apply the same dynamic-part links described by
`Vehicle.loadDynamicallyPartsFromXML`; original files stay unchanged.
All observed starting models are rigid; skinned vehicle meshes are rejected
rather than silently flattened. See [recovery aggregates](../analysis/mission-recovery.json).

The Lua host executes recovered `VehicleMotor`, `WheelsUtil`,
`Motorized.loadMotor`, `Fillable`, `MultiSiloTrigger`, `FillTrigger`, `TipTrigger`,
`Trailer.updateTipping`, `Overloading` and `FieldDefinition` behavior. Exact
source slices initialize the original fruit/fill registry, silo defaults,
field channel definitions and silo amount methods. The full `main.lua` and
complete mission constructors do not execute. Local event transport, object
registration and native services are explicitly authored adapters.

The field service reads the original 41 / 21 field definitions, ownership and
parallelograms. Original version-1 fruit and cultivator GRLE maps are decoded;
the fruit grids contain 4096 x 4096 cells. The new 16-bit decoder was compared
with native leaf `0x14028f770` in **107 cases / 35,133,356 values**, including both
complete original fruit maps. No original code image is required by the game
runtime. See [native comparison evidence](../analysis/rle16-validation.json).

Crop type/growth state comes from those maps. Original foliage block meshes,
material IDs, sizes and textures render near the camera. Placement uses an
authored one-metre candidate lattice with deterministic jitter, rather than
the complete GE6 foliage generator. Density state is one-based relative to
the I3D state arrays. Crop wind, distant detail shaders and exact density
rendering remain incomplete.

The native density adapter provides bounded channel, comparison, mask and
parallelogram operations for the exercised paths. Recovered `FSUtils` performs
cutting, cultivation and wheat sowing. Original work-area nodes determine tool
coverage. Basic harvested litres use cell area and original fruit yield data;
full combine multipliers, fertilization, straw and growth scheduling remain
unimplemented. Worked/seeded changes currently live only in session memory.

## Physics and rendering limits

The original motor curve, ratios, speed limits, wheel torque and braking logic
run through the recovered no-differential path. `setWheelShapeProps` is served
by a temporary planar force integrator in the host. Wheels visually rotate
and steer using original nodes. This is not original GE6/PhysX suspension,
traction, tire slip, component joints or rollover behavior.

Original solid I3D meshes feed a spatially indexed static-wall triangle service.
It blocks the vehicle centre capsule against walls/trunks and constrains the
following camera. It does not supply full vehicle/implement rigid-body
collisions. Attachment following is kinematic; native articulation, moving
three-point linkages and original saved front-weight connections need recovery.
Recovered `AnimatedVehicle.load/update` interprets original XML animation parts,
including chained transforms, visibility, timing, reversal and stop times.
Recovered `Trailer.load/updateTick` controls tip/door animations, delayed discharge
and automatic closing. Authored transform/visibility services connect these rules
to rendering, including initially hidden vehicle geometry. Procedural grain fill
surfaces, unloading particles and other client effects remain incomplete.
See [native and animation recovery](NATIVE_RECOVERY.md) for the bytecode comparison
and the limits of the joint-callback fixture.

The HUD is authored. The earlier recovered frontend remains separate. Audio,
fuel consumption/refuelling, complete shops/economy, persistent save/load,
workers, animals, forestry, multiplayer, mission time/weather and full crop
growth remain unfinished. Closing this alpha discards the session.

## Validation and scenarios

`mission_Map01` and `mission_Map02` exercise 43 gameplay assertions each:
motor travel/braking/steering; original silo overlaps and transfer; overflow,
empty-stock and mixed-grain rejection; original farm unloading; field
definitions; authored in-memory harvest/cultivate/sow fixtures; combine pipe
raycasts and conservation; seed-station refilling/payment; original tip state,
door motion, discharge delay, automatic closure and support-wheel visibility.
These fixtures
do not claim complete original-engine parity.

`mission_harvest` additionally relocates the original combine and cutter to a
mature crop in an originally owned field, drives the actual work area through
the original density map, checks harvested litres and rendered vehicle pixels,
and captures both maps. See [harvest evidence](../analysis/mission-validation.json).

`recovered_vehicle_animation` separately compares 777 complete state samples
against the original normalized bytecode in seven authored service scenarios.
It covers exercised behavior of AnimatedVehicle, Trailer and SpecializationUtil,
without claiming every branch or native physics parity.

`mission_harvest` also captures `--scenario tip` on both maps after four seconds:
the original 8,500 L Brantner tilts and transfers grain into the original farm
trigger. Trailer contents plus discharged grain remain equal to initial capacity,
and farm inventory increases by precisely the discharged amount.

For reproducible manual trials use `--scenario harvest`, `--scenario silo` or
`--scenario tip`.
These explicitly relocate equipment in a disposable new session. Normal Career
Start retains the original starting placements. Scenario screenshots are
labelled as such; they are not evidence of a saved career or original physics.

`world_native_services` separately tests transformed static walls, trigger
exclusion, height checks, GRLE8/16 values and malformed/oversized data rejection.

The packaged Map02 silo scenario was also operated in a visible Windows window.
Pressing R changed the trailer from 0 L to 1,975 L while stock fell to 3,025 L,
then it stopped at 5,000 L with empty stock. Final session telemetry recorded
5,000 L transferred. Mouse dragging changed the following view; E detached and
reattached the filled Brantner, and Tab switched to the next tractor. This is
live input/transfer evidence, separate from scripted harvest and density tests.
The trailer's procedural visible grain surface is still missing.
The packaged recovered frontend was then used to select an empty Career slot,
Normal difficulty and Bjornholm. Start opened the new gameplay process at the
original starting combine. E attached its original nearby cutter and B enabled
the tool. That new session was left open for manual driving.

Packaging verified all recovered source hashes, both original starting fleets,
all four frontend captures, and identical workspace/package scene and gameplay
renders from a different working directory. The local package contains 62
recovered source files and 407 traced resources; no original executable or code
snapshot is included.

The Alpha2 package was launched through the recovered frontend in a real Windows
window: empty Career slot, Normal difficulty, Bjornholm, Start. O visibly opened
and closed the original Sampo pipe through recovered AnimatedVehicle logic.
E attached the starting cutter and B activated it; the session was left open.
This is live launch/animation/input evidence. The new trailer discharge captures
and conservation checks remain separate scripted scenario evidence.
