# Power Control Loop Design

**Goal:** Build a dedicated closed-loop control task for the high-voltage power supply using AD5593R IO0 as DAC output and IO4 as ADC feedback, with a UI-facing 0.0 to 5.0 kV reference interface.

**Architecture:** A new `tasks/control` task owns the control loop and runs at a fixed 10 ms period. UI code writes only the requested high-voltage setpoint in kV into a shared control context; the control task converts that reference into ADC/DAC engineering units, runs PID, and writes the DAC output. The existing RLS library remains available as a standalone estimator but is not connected to the live control loop in this phase.

**Tech Stack:** STM32H723 HAL, FreeRTOS/CMSIS-OS2, AD5593R driver, PID library, RLS library, LVGL UI.

## Global Constraints

- Control period is fixed at 10 ms and must match the sampling period.
- UI reference input range is 0.0 to 5.0 kV.
- AD5593R IO0 is the control DAC output channel.
- AD5593R IO4 is the feedback ADC input channel.
- AD5593R IO5 remains available as a spare ADC input and is not used by the first control loop.
- The live closed loop uses PID only; RLS is not wired into control decisions in this phase.
- Control code must be isolated from UI rendering code and live in its own task folder.
- The task structure under `tasks/` must keep each task in its own folder.

## Proposed Files

- `firmware/power_control/tasks/control/control_task.h`
- `firmware/power_control/tasks/control/control_task.c`
- `firmware/power_control/tasks/control/control_context.h`
- `firmware/power_control/tasks/control/control_context.c`
- `firmware/power_control/tasks/lcd/lcd_task.c`
- `firmware/power_control/tasks/lcd/lcd_task.h`
- `firmware/power_control/Core/Src/freertos.c`
- `firmware/power_control/CMakeLists.txt`
- `firmware/power_control/app/lib/pid/pid_controller.h`
- `firmware/power_control/app/lib/pid/pid_controller.c`
- `firmware/power_control/app/lib/rls/rls_estimator.h`
- `firmware/power_control/app/lib/rls/rls_estimator.c`

## Control Task

The control task is the only component that reads the feedback ADC and writes the DAC output. It runs on a fixed 10 ms period and owns the timing for the sampling and output update loop. The task uses a PID controller configured with output limiting and anti-windup. The loop operates on engineering units:

- input reference: kV, from the UI shared context
- feedback: kV, derived from the ADC reading
- output: V, converted to the DAC code for IO0

The control loop will treat `0.0 kV` as zero output and `5.0 kV` as full-scale reference. Output is limited to the AD5593R DAC range `0.0 V` to `5.0 V`. The task also stores a snapshot of the current reference, feedback, DAC output, and fault state so the UI can display live status without participating in the loop.

## Shared Control Context

The shared context is the boundary between UI and control logic. It stores:

- requested reference in kV
- latest measured feedback in kV
- latest DAC output in volts
- loop enabled state
- fault flags

The UI writes only the requested reference. The control task updates all other fields after each cycle. The reference is clamped to `0.0 kV` to `5.0 kV` before it reaches the loop. This context must remain task-safe and simple enough that UI code cannot directly disturb control timing.

## LCD/UI Integration

The LCD task remains the user-facing surface. It presents a numeric control for the high-voltage reference in the range `0.0 kV` to `5.0 kV` and pushes changes into the shared control context. It also displays the latest control snapshot so the user can see the requested value, measured value, DAC output, and fault state.

The LCD task must not compute control outputs, read ADC feedback, or write DAC outputs. Its role is limited to human input and status display.

## PID Library

The PID library remains a pure math module with no HAL dependency. It accepts:

- proportional gain
- integral gain
- derivative gain
- output limits
- integral limits
- optional setpoint rate limit
- optional output slew limit
- optional derivative filtering

The controller exposes initialization, reset, and update functions. It must support anti-windup and derivative-on-measurement so that a setpoint change does not create a large derivative kick. The control task owns the gain values and timing; the PID library only performs the computation.

## RLS Library

The RLS library stays available as a standalone estimator for future plant identification or feedforward work. It is compiled into the firmware, but the control task does not call it in this phase. This keeps the first closed-loop version focused on stability and observability instead of online model adaptation.

## Error Handling

The control task must treat ADC read failure, DAC write failure, invalid feedback data, and loop initialization failure as control faults. Faults should be visible in the shared control snapshot and on the LCD. The task should continue running after transient I2C issues, but it should not silently drive the output outside the configured limits.

## Validation

The implementation is considered complete when:

- the firmware builds successfully
- the control task exists as its own folder and thread
- the LCD can change the reference in kV
- the control task runs at the intended fixed period
- IO0 is used for DAC output and IO4 for feedback ADC
- RLS is compiled but not linked into the live loop logic

