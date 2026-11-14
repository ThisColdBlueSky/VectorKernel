================================================================================
Vector Sysfs Control Subsystem & Thermal Architecture Documentation
================================================================================

:Target Hardware: POCO X4 Pro 5G / Redmi Note 11 Pro+ 5G (``veux`` / ``peux``)
:Target SoC: Qualcomm Snapdragon 695 5G (``sm6375``)
:Kernel Driver Source: ``drivers/power/supply/qcom/wt_chg.c``
:Sysfs Mount Point: ``/sys/kernel/vector/``

Overview
========

Vector is an in-kernel hardware management and thermal override subsystem developed
specifically for the Xiaomi Snapdragon 695 platform (``veux`` / ``peux``). It provides
granular, real-time control over battery charging parameters, charging bypass, and
subsystem thermal throttling (charging, CPU clusters, GPU, and display backlight).

The primary goal of the Vector subsystem is to liberate hardware performance and
charging speed from overly restrictive OEM userspace daemon throttling (such as
Xiaomi's ``mi_thermald``), while simultaneously providing a mathematically and
chemically grounded multi-tier hardware safety architecture.

All runtime parameters are exposed under the unified sysfs hierarchy::

    /sys/kernel/vector/

The nodes are accessible via root access and can be configured interactively using
shell commands, automated boot scripts (e.g., Magisk/KernelSU ``service.d``), or
via the official companion application **Vector Link**.

--------------------------------------------------------------------------------

Multi-Tiered Safety Architecture: Why It Is Safe vs. Unsafe
============================================================

A common misconception in mobile device performance tuning is that disabling OEM
thermal mitigations (e.g., ``mi_thermald``) removes all safety nets and risks hardware
damage. In reality, the Vector architecture implements a robust, three-tiered
defense-in-depth model where real physical safety is preserved at the silicon and
battery chemistry level.

1. Tier 1: Hardware-Level Silicon & PMIC Safety (Unbypassable)
--------------------------------------------------------------
At the lowest physical layer, hardware safety is governed by hardwired circuitry
embedded directly into the Qualcomm PMIC (Power Management IC) and the Snapdragon SoC:

- **Qualcomm PMIC Protection Circuits:** The charging controller contains hardwired
  Over-Voltage Protection (OVP), Over-Current Protection (OCP), Reverse-Current
  Protection, and catastrophic thermal shutdown circuits. Even if software were to
  request an unsafe current, the PMIC hardware autonomously limits or severs the power rail.
- **Silicon TSENS & LMh (Limits Management Hardware):** The Snapdragon 695 SoC
  contains internal thermal sensor diodes (TSENS) across CPU cores, GPU, and modem
  silicon. If junction silicon temperatures approach critical physical limits
  (typically 105°C - 115°C junction temperature, :math:`T_{jMax}`), hardware-level
  thermal mitigation mechanisms (LMh) autonomously clamp clocks or execute a hard
  protective emergency reboot independently of any operating system or kernel code.

2. Tier 2: Kernel JEITA Battery Chemistry Protection
----------------------------------------------------
Lithium-ion battery degradation and safety are dictated by battery temperature and
voltage, standardized internationally by the Japan Electronics and Information
Technology Industries Association (JEITA) specifications.

Vector interfaces directly with the Qualcomm Battery Management System (BMS) fuel gauge
ADC thermistor (``BATT_QG_TEMP``), which measures the **true internal chemistry temperature**
of the battery cell:

- **Cold (< 0°C):** Charging is disabled to prevent lithium plating.
- **Cool (0°C – 15°C):** Charging current is reduced to protect electrolyte mobility.
- **Normal (15°C – 45°C):** Full-speed fast charging is safe and permitted.
- **Warm (45°C – 48°C):** Kernel JEITA automatically curtails charging voltage and current.
- **Hot / Overheat (>= 48°C / 60°C):** Charging is shut off completely by kernel driver logic
  (``POWER_SUPPLY_HEALTH_OVERHEAT``).

**Fail-Safe Mechanism:**
If the BMS fuel gauge temperature reading fails or returns an error (``rc < 0``), the
Vector driver immediately defaults to `fail-safe mode`, actively engaging 100% of stock
vendor throttling and cutting off aggressive charging.

3. Tier 3: Vector Dynamic Guardrails & Hysteresis
-------------------------------------------------
In addition to hardware and JEITA safety, Vector introduces kernel-enforced dynamic
guardrails:

- **Battery Tripwire (``battery_trip_decic``):** The driver continuously monitors real
  battery cell temperature. If the cell reaches the user-configured trip threshold
  (default: 45.0°C), all charging, CPU, GPU, and display overrides are immediately
  and atomically revoked. Stock vendor throttling is instantly reinstated.
- **Hysteresis Band (``battery_clear_decic``):** To prevent rapid fluttering between
  throttled and unthrottled states, overrides remain disabled until the cell cools
  down to the clear threshold (default: 42.0°C), enforcing at least a 2.0°C hysteresis.
- **Chassis Skin Backstop (``skin_override_max_mc``):** An optional guardrail monitoring
  the chassis surface thermistor (``sdm-skin-therm-usr``). If enabled, overrides are
  revoked if the phone's exterior becomes uncomfortably hot to hold (e.g. >= 50.0°C).

--------------------------------------------------------------------------------

Why Disabling Xiaomi's ``mi_thermald`` Doesn't Matter Much
===========================================================

What ``mi_thermald`` Actually Does
----------------------------------
In stock MIUI and HyperOS, ``mi_thermald`` is a userspace daemon running in Android userland.
Its operational logic is driven primarily by **exterior hand-feel (chassis touch temperature)**
and cosmetic thermal tables rather than true silicon or battery limits.

Specifically:
- Xiaomi's daemon monitors superficial chassis thermistors located near the phone's frame
  and USB port.
- During standard 67W Turbo charging, the USB connector and charging IC naturally generate
  localized surface warmth (~37°C–39°C), even while the actual battery cell inside is still
  completely cool (~33°C–35°C).
- ``mi_thermald`` reacts to this surface warmth by aggressively stepping down charging current
  through arbitrary throttle levels (from 67W down to 18W, 10W, or even 5W).
- During 3D gaming or intensive workloads, ``mi_thermald`` prematurely downclocks the CPU
  and GPU when the chassis warms up, even though the SoC silicon junction temperature is
  sitting comfortably at 55°C–65°C (well below the 105°C silicon threshold).

Why Bypassing It Is Safe
------------------------
Bypassing ``mi_thermald`` via Vector's ``ignore_vendor_*`` nodes replaces subjective,
overly aggressive userspace throttling with **objective, hardware-grounded thermal limits**:

1. **Silicon is built for heat:** Silicon semiconductors operate completely within design
   tolerances up to 90°C–100°C. Throttling a CPU at 40°C chassis temperature provides no
   silicon longevity benefit; it merely compromises user experience and frame stability.
2. **Battery safety is guarded by BMS, not userspace:** Battery chemistry is protected
   directly by the kernel fuel gauge thermistor and JEITA rules. Vector ensures charging
   is unthrottled while the battery is below 45.0°C, and instantly enforces full throttle
   if 45.0°C is reached.
3. **No risk of thermal runaway:** Thermal runaway in lithium polymer batteries occurs at
   temperatures exceeding 60°C–70°C or during overvoltage (> 4.45V). Vector's JEITA cutoffs
   at 48.0°C and PMIC hardware overvoltage protection make thermal runaway physically
   impossible under normal operation.

Consequently, disabling ``mi_thermald`` removes artificial performance penalties while
preserving the real, scientific safety limits of the device.

--------------------------------------------------------------------------------

Comprehensive Sysfs Node Reference
==================================

The following section documents every node exposed in ``/sys/kernel/vector/``.

1. ``charge_stop_pct``
-----------------------
- **Path:** ``/sys/kernel/vector/charge_stop_pct``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Integer or Dual-Integer (``"stop"`` or ``"stop resume"``)
- **Range:** ``0`` (disabled), ``100`` (disabled), or ``50`` to ``99``
- **Default:** ``0`` (disabled / charge to 100%)

**Description:**
Controls the battery charge limiter. When enabled, the kernel continuously samples the
battery state of charge (SoC) using true Coulomb Counter capacity (``cc_soc``) and
fuel gauge metrics.

Once the battery percentage reaches the ``charge_stop_pct`` threshold:
- Fast charging is halted.
- The charging circuit transitions to `Hold Mode`, holding battery level while powering the
  system from USB.
- The kernel waits until battery capacity falls to ``charge_resume_pct`` before resuming charging.

Writing ``0`` or ``100`` turns off the limiter and clears the hold state.
Writing two space-separated integers (e.g. ``echo "80 75" > charge_stop_pct``) sets both
the stop and resume thresholds atomically.

2. ``charge_resume_pct``
-------------------------
- **Path:** ``/sys/kernel/vector/charge_resume_pct``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Integer
- **Range:** ``20`` to ``(charge_stop_pct - 2)``
- **Default:** ``0``

**Description:**
Sets the battery percentage at which charging resumes after the limiter has paused it.
The kernel strictly enforces a minimum hysteresis gap of at least 2% below ``charge_stop_pct``.
This prevents rapid charging cycling (hunting) when the battery fluctuates around the target limit.

3. ``charge_limit_state``
--------------------------
- **Path:** ``/sys/kernel/vector/charge_limit_state``
- **Mode:** Read-Only (``0444``)
- **Data Type:** Integer
- **Values:**
  - ``0``: Limiter is inactive / disabled.
  - ``1``: Limiter is active; currently charging (or cable unplugged).
  - ``2``: Limiter is active; holding at limit, waiting for battery to drop to resume level.

**Description:**
Provides real-time feedback on the state machine of the charge limiter. Supports
``sysfs_notify`` for instant event notification in userspace applications.

4. ``bypass_charger``
----------------------
- **Path:** ``/sys/kernel/vector/bypass_charger``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Boolean Integer (``0`` or ``1``)
- **Default:** ``0`` (disabled)

**Description:**
Enables direct charging bypass (pass-through power). When set to ``1``:
- The battery charging path is disabled via the main IIO charging channel.
- System power is supplied directly from the USB charger input.
- Battery idle state is maintained, eliminating battery charging heat and electrochemical
  cycling during heavy loads (gaming, navigation, emulation).

**Safety Backstops:**
- Automatically resets to ``0`` when the USB cable is unplugged.
- Automatically resets to ``0`` if battery capacity drops below 10% to prevent unexpected
  system shutdowns under transient peak power draws.

5. ``usb_icl_override_ma``
---------------------------
- **Path:** ``/sys/kernel/vector/usb_icl_override_ma``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Integer (milliamperes)
- **Range:** ``0`` (stock / no override), or ``100`` to ``1500`` mA
- **Default:** ``0`` (stock)

**Description:**
Overrides the Input Current Limit (ICL) for power sources identified as standard USB (SDP),
which default to 500 mA. Nonzero values below 100 mA are clamped to 100 mA; values exceeding
1500 mA are clamped to 1500 mA.
This setting automatically clears back to ``0`` whenever the USB cable is disconnected.
It does not interfere with Quick Charge (QC), USB-PD, or Xiaomi 67W proprietary chargers.

6. ``ignore_vendor_charging_thermal``
--------------------------------------
- **Path:** ``/sys/kernel/vector/ignore_vendor_charging_thermal``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Boolean Integer (``0`` or ``1``)
- **Default:** ``0`` (enforce vendor thermal daemon throttle)

**Description:**
The master thermal control switch. When set to ``1``:
- Ignores throttling requests generated by Xiaomi's ``mi_thermald`` for fast charging.
- Allows full-rate fast charging to continue while the battery cell remains below the safety
  tripwire (``battery_trip_decic``).
- Acts as the operational prerequisite for CPU, GPU, and display thermal overrides.
- If the battery reaches ``battery_trip_decic`` or JEITA hot limits, this override is
  automatically revoked in kernel space.

7. ``vendor_thermal_max_level``
--------------------------------
- **Path:** ``/sys/kernel/vector/vendor_thermal_max_level``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Integer (thermal mitigation step)
- **Range:** ``0`` to ``24``
- **Default:** ``18`` (approximately 3500 mA fast-charging cap)

**Description:**
Defines the highest vendor throttle step allowed when ``ignore_vendor_charging_thermal`` is active.
Xiaomi's thermal daemon requests mitigation steps between ``0`` (unthrottled) and ``24`` (maximum
throttling / ~500 mA).
The kernel calculates effective throttling as:
``effective_step = min(requested_step, vendor_thermal_max_level)``.
Setting this node to ``0`` disables vendor charging throttling completely. Setting it to ``12`` caps
throttling halfway, providing balanced thermal behavior.

8. ``ignore_vendor_cpu_thermal``
---------------------------------
- **Path:** ``/sys/kernel/vector/ignore_vendor_cpu_thermal``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Boolean Integer (``0`` or ``1``)
- **Default:** ``0`` (enforce vendor CPU frequency limits)

**Description:**
Prevents ``mi_thermald`` from imposing cooling state frequency caps on the CPU clusters.
When enabled:
- Little and Big CPU cluster frequencies are uncoupled from userspace thermal throttling.
- Sustains maximum clock speeds during gaming and sustained processing.
- Governed by the battery tripwire and optional chassis skin limit.

9. ``cpu_little_override_max_state``
-------------------------------------
- **Path:** ``/sys/kernel/vector/cpu_little_override_max_state``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Unsigned Integer (cooling state index)
- **Range:** ``0`` to maximum thermal cooling states
- **Default:** ``0`` (unthrottled / full 1.80 GHz)

**Description:**
Controls the maximum cooling state index for the Cortex-A55 Little cluster while
``ignore_vendor_cpu_thermal`` is enabled.
- State ``0`` corresponds to maximum unthrottled frequency (1.80 GHz).
- Higher numbers allow the kernel thermal engine to throttle the Little cluster down to
  that specific cooling state floor.

10. ``cpu_big_override_max_state``
-----------------------------------
- **Path:** ``/sys/kernel/vector/cpu_big_override_max_state``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Unsigned Integer (cooling state index)
- **Range:** ``0`` to maximum thermal cooling states
- **Default:** ``0`` (unthrottled / full 2.21 GHz)

**Description:**
Controls the maximum cooling state index for the Cortex-A76 Big cluster while
``ignore_vendor_cpu_thermal`` is enabled.
- State ``0`` maintains unthrottled maximum frequency (2.21 GHz).
- Higher numbers configure an intermediate cooling limit for power-saving profiles.

11. ``ignore_vendor_gpu_thermal``
----------------------------------
- **Path:** ``/sys/kernel/vector/ignore_vendor_gpu_thermal``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Boolean Integer (``0`` or ``1``)
- **Default:** ``0`` (enforce vendor GPU frequency limits)

**Description:**
Prevents ``mi_thermald`` from downclocking the Adreno 619 GPU based on exterior chassis
temperatures. Eliminates thermal frame drops in demanding 3D games and graphics workloads.

12. ``gpu_override_max_state``
-------------------------------
- **Path:** ``/sys/kernel/vector/gpu_override_max_state``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Unsigned Integer (cooling state index)
- **Range:** ``0`` to maximum GPU cooling states
- **Default:** ``0`` (unthrottled / full 840 MHz)

**Description:**
Sets the cooling state ceiling for the GPU while ``ignore_vendor_gpu_thermal`` is enabled.
``0`` keeps the GPU unthrottled at maximum operational frequency.

13. ``ignore_vendor_backlight_thermal``
----------------------------------------
- **Path:** ``/sys/kernel/vector/ignore_vendor_backlight_thermal``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Boolean Integer (``0`` or ``1``)
- **Default:** ``0`` (allow vendor thermal screen dimming)

**Description:**
Stops ``mi_thermald`` from forcibly reducing AMOLED screen brightness when the device is warm.
Crucial for outdoor navigation in direct sunlight or extended outdoor gaming sessions.

14. ``battery_trip_decic``
---------------------------
- **Path:** ``/sys/kernel/vector/battery_trip_decic``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Unsigned Integer (decidegrees Celsius, where 10 = 1.0°C)
- **Range:** ``400`` to ``470`` (40.0°C to 47.0°C)
- **Default:** ``450`` (45.0°C)

**Description:**
The primary kernel safety tripwire. Sets the maximum battery cell temperature at which all
charging and performance overrides remain active.
If the true cell temperature from the BMS fuel gauge reaches this value:
- All overrides (charging, CPU, GPU, display) are atomically disengaged.
- Stock thermal throttling is immediately enforced.
- The kernel logs a thermal trip warning.

15. ``battery_clear_decic``
----------------------------
- **Path:** ``/sys/kernel/vector/battery_clear_decic``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Unsigned Integer (decidegrees Celsius)
- **Range:** ``300`` to ``(battery_trip_decic - 20)`` (30.0°C up to trip - 2.0°C)
- **Default:** ``420`` (42.0°C)

**Description:**
The cool-down release threshold. Following a thermal trip, overrides remain disabled until
the battery cell temperature cools below this value.
The kernel strictly enforces ``battery_clear_decic <= battery_trip_decic - 20`` to guarantee
at least 2.0°C of thermal hysteresis.

16. ``skin_override_max_mc``
-----------------------------
- **Path:** ``/sys/kernel/vector/skin_override_max_mc``
- **Mode:** Read / Write (``0660``)
- **Data Type:** Integer (millidegrees Celsius, where 1000 = 1.0°C)
- **Range:** ``0`` (disabled), or ``45000`` to ``60000`` (45.0°C to 60.0°C)
- **Default:** ``0`` (disabled)

**Description:**
Optional exterior chassis skin backstop. When configured to a positive millidegree value,
the kernel monitors the ``sdm-skin-therm-usr`` thermal zone.
If the phone frame reaches this temperature, CPU, GPU, and display overrides are revoked
independently of battery temperature, reinstating when the frame cools 2.0°C (2000 mC)
below the threshold. This guardrail is intended for handheld comfort.

--------------------------------------------------------------------------------

Recommended Profiles
====================

1. Daily Battery Longevity Profile
----------------------------------
Optimized for overnight charging and maximum multi-year battery health::

    echo "80 75" > /sys/kernel/vector/charge_stop_pct
    echo 0 > /sys/kernel/vector/bypass_charger
    echo 0 > /sys/kernel/vector/ignore_vendor_charging_thermal
    echo 0 > /sys/kernel/vector/ignore_vendor_cpu_thermal

2. Unthrottled Gaming & Emulation Profile
----------------------------------------
Maintains maximum GPU/CPU performance and charges from wall power without heating the battery::

    echo 1 > /sys/kernel/vector/bypass_charger
    echo 1 > /sys/kernel/vector/ignore_vendor_charging_thermal
    echo 0 > /sys/kernel/vector/vendor_thermal_max_level
    echo 1 > /sys/kernel/vector/ignore_vendor_cpu_thermal
    echo 1 > /sys/kernel/vector/ignore_vendor_gpu_thermal
    echo 1 > /sys/kernel/vector/ignore_vendor_backlight_thermal
    echo 440 > /sys/kernel/vector/battery_trip_decic
    echo 410 > /sys/kernel/vector/battery_clear_decic
    echo 50000 > /sys/kernel/vector/skin_override_max_mc

3. Factory Stock Reset
----------------------
Restores all kernel driver parameters to clean factory defaults::

    echo 0 > /sys/kernel/vector/charge_stop_pct
    echo 0 > /sys/kernel/vector/bypass_charger
    echo 0 > /sys/kernel/vector/usb_icl_override_ma
    echo 0 > /sys/kernel/vector/ignore_vendor_charging_thermal
    echo 18 > /sys/kernel/vector/vendor_thermal_max_level
    echo 0 > /sys/kernel/vector/ignore_vendor_cpu_thermal
    echo 0 > /sys/kernel/vector/cpu_little_override_max_state
    echo 0 > /sys/kernel/vector/cpu_big_override_max_state
    echo 0 > /sys/kernel/vector/ignore_vendor_gpu_thermal
    echo 0 > /sys/kernel/vector/gpu_override_max_state
    echo 0 > /sys/kernel/vector/ignore_vendor_backlight_thermal
    echo 450 > /sys/kernel/vector/battery_trip_decic
    echo 420 > /sys/kernel/vector/battery_clear_decic
    echo 0 > /sys/kernel/vector/skin_override_max_mc
