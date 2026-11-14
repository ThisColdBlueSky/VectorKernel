# Vector Sysfs Control Subsystem & Thermal Architecture Documentation

- **Target Hardware:** POCO X4 Pro 5G / Redmi Note 11 Pro+ 5G (`veux` / `peux`)
- **Target SoC:** Qualcomm Snapdragon 695 5G (`sm6375`)
- **Kernel Driver Source:** [`drivers/power/supply/qcom/wt_chg.c`](file:///data/data/com.termux/files/home/VectorKernel/drivers/power/supply/qcom/wt_chg.c)
- **Sysfs Mount Point:** `/sys/kernel/vector/`

---

## 1. Overview

**Vector** is an in-kernel hardware management and thermal override subsystem developed specifically for the Xiaomi Snapdragon 695 platform (`veux` / `peux`). It provides granular, real-time control over battery charging parameters, charging bypass, and subsystem thermal throttling (charging, CPU clusters, GPU, and display backlight).

The primary goal of the Vector subsystem is to liberate hardware performance and charging speed from overly restrictive OEM userspace daemon throttling (such as Xiaomi's `mi_thermald`), while simultaneously providing a mathematically and chemically grounded multi-tier hardware safety architecture.

All runtime parameters are exposed under the unified sysfs hierarchy:
```
/sys/kernel/vector/
```
The nodes are accessible via root access and can be configured interactively using shell commands, automated boot scripts (e.g., Magisk/KernelSU `service.d`), or via the official companion application **Vector Link**.

---

## 2. Multi-Tiered Safety Architecture: Why It Is Safe vs. Unsafe

A common misconception in mobile device performance tuning is that disabling OEM thermal mitigations (e.g., `mi_thermald`) removes all safety nets and risks hardware damage. In reality, the Vector architecture implements a robust, three-tiered defense-in-depth model where real physical safety is preserved at the silicon and battery chemistry level.

```
+-----------------------------------------------------------------------+
|                       Vector Safety Hierarchy                         |
+-----------------------------------------------------------------------+
| Tier 1: Hardware-Level Silicon & PMIC (Hardwired, Unbypassable)       |
|   - Qualcomm PMIC OVP / OCP / Reverse-Current Protective Cutoffs      |
|   - Snapdragon Silicon TSENS & LMh Automatic Thermal Shutdown         |
+-----------------------------------------------------------------------+
| Tier 2: Kernel JEITA Battery Chemistry Protection                     |
|   - True battery cell ADC thermistor reading (BATT_QG_TEMP)           |
|   - Automatic JEITA step current reductions (<15°C, >45°C, >48°C)     |
|   - Fail-safe fallback to stock throttling on sensor read error       |
+-----------------------------------------------------------------------+
| Tier 3: Vector Dynamic Guardrails & Hysteresis                        |
|   - Real-time cell tripwire (battery_trip_decic, default 45.0°C)      |
|   - Enforced 2.0°C hysteresis band (battery_clear_decic)              |
|   - Optional chassis skin backstop (skin_override_max_mc)             |
|   - Charging bypass auto-clearing (<10% SoC or cable unplug)          |
+-----------------------------------------------------------------------+
```

### Tier 1: Hardware-Level Silicon & PMIC Safety (Unbypassable)
At the lowest physical layer, hardware safety is governed by hardwired circuitry embedded directly into the Qualcomm PMIC (Power Management IC) and the Snapdragon SoC:
- **Qualcomm PMIC Protection Circuits:** The charging controller contains hardwired Over-Voltage Protection (OVP), Over-Current Protection (OCP), Reverse-Current Protection, and catastrophic thermal shutdown circuits. Even if software were to request an unsafe current, the PMIC hardware autonomously limits or severs the power rail.
- **Silicon TSENS & LMh (Limits Management Hardware):** The Snapdragon 695 SoC contains internal thermal sensor diodes (TSENS) across CPU cores, GPU, and modem silicon. If junction silicon temperatures approach critical physical limits (typically 105°C - 115°C junction temperature, $T_{jMax}$), hardware-level thermal mitigation mechanisms (LMh) autonomously clamp clocks or execute a hard protective emergency reboot independently of any operating system or kernel code.

### Tier 2: Kernel JEITA Battery Chemistry Protection
Lithium-ion battery degradation and safety are dictated by battery temperature and voltage, standardized internationally by the Japan Electronics and Information Technology Industries Association (JEITA) specifications.

Vector interfaces directly with the Qualcomm Battery Management System (BMS) fuel gauge ADC thermistor (`BATT_QG_TEMP`), which measures the **true internal chemistry temperature** of the battery cell:
- **Cold (< 0°C):** Charging is disabled to prevent lithium plating.
- **Cool (0°C – 15°C):** Charging current is reduced to protect electrolyte mobility.
- **Normal (15°C – 45°C):** Full-speed fast charging is safe and permitted.
- **Warm (45°C – 48°C):** Kernel JEITA automatically curtails charging voltage and current.
- **Hot / Overheat (>= 48°C / 60°C):** Charging is shut off completely by kernel driver logic (`POWER_SUPPLY_HEALTH_OVERHEAT`).

> **Fail-Safe Mechanism:**
> If the BMS fuel gauge temperature reading fails or returns an error (`rc < 0`), the Vector driver immediately defaults to **fail-safe mode**, actively engaging 100% of stock vendor throttling and cutting off aggressive charging.

### Tier 3: Vector Dynamic Guardrails & Hysteresis
In addition to hardware and JEITA safety, Vector introduces kernel-enforced dynamic guardrails:
- **Battery Tripwire (`battery_trip_decic`):** The driver continuously monitors real battery cell temperature. If the cell reaches the user-configured trip threshold (default: 45.0°C), all charging, CPU, GPU, and display overrides are immediately and atomically revoked. Stock vendor throttling is instantly reinstated.
- **Hysteresis Band (`battery_clear_decic`):** To prevent rapid fluttering between throttled and unthrottled states, overrides remain disabled until the cell cools down to the clear threshold (default: 42.0°C), enforcing at least a 2.0°C hysteresis.
- **Chassis Skin Backstop (`skin_override_max_mc`):** An optional guardrail monitoring the chassis surface thermistor (`sdm-skin-therm-usr`). If enabled, overrides are revoked if the phone's exterior becomes uncomfortably hot to hold (e.g. >= 50.0°C).

---

## 3. Why Disabling Xiaomi's `mi_thermald` Doesn't Matter Much

### What `mi_thermald` Actually Does
In stock MIUI and HyperOS, `mi_thermald` is a userspace daemon running in Android userland. Its operational logic is driven primarily by **exterior hand-feel (chassis touch temperature)** and cosmetic thermal tables rather than true silicon or battery limits.

Specifically:
- Xiaomi's daemon monitors superficial chassis thermistors located near the phone's frame and USB port.
- During standard 67W Turbo charging, the USB connector and charging IC naturally generate localized surface warmth (~37°C–39°C), even while the actual battery cell inside is still completely cool (~33°C–35°C).
- `mi_thermald` reacts to this surface warmth by aggressively stepping down charging current through arbitrary throttle levels (from 67W down to 18W, 10W, or even 5W).
- During 3D gaming or intensive workloads, `mi_thermald` prematurely downclocks the CPU and GPU when the chassis warms up, even though the SoC silicon junction temperature is sitting comfortably at 55°C–65°C (well below the 105°C silicon threshold).

### Why Bypassing It Is Safe
Bypassing `mi_thermald` via Vector's `ignore_vendor_*` nodes replaces subjective, overly aggressive userspace throttling with **objective, hardware-grounded thermal limits**:

1. **Silicon is built for heat:** Silicon semiconductors operate completely within design tolerances up to 90°C–100°C. Throttling a CPU at 40°C chassis temperature provides no silicon longevity benefit; it merely compromises user experience and frame stability.
2. **Battery safety is guarded by BMS, not userspace:** Battery chemistry is protected directly by the kernel fuel gauge thermistor and JEITA rules. Vector ensures charging is unthrottled while the battery is below 45.0°C, and instantly enforces full throttle if 45.0°C is reached.
3. **No risk of thermal runaway:** Thermal runaway in lithium polymer batteries occurs at temperatures exceeding 60°C–70°C or during overvoltage (> 4.45V). Vector's JEITA cutoffs at 48.0°C and PMIC hardware overvoltage protection make thermal runaway physically impossible under normal operation.

Consequently, disabling `mi_thermald` removes artificial performance penalties while preserving the real, scientific safety limits of the device.

---

## 4. Comprehensive Sysfs Node Reference

The following table summarizes all nodes exposed under `/sys/kernel/vector/`:

| Node Name | Permissions | Data Type | Default Value | Valid Range / Format | Purpose |
| :--- | :---: | :---: | :---: | :---: | :--- |
| `charge_stop_pct` | RW (`0660`) | Integer / Dual | `0` (off) | `0`, `100`, or `50..99` | Stop charging at target percentage |
| `charge_resume_pct` | RW (`0660`) | Integer | `0` (off) | `20..(stop - 2)` | Resume charging threshold |
| `charge_limit_state` | RO (`0444`) | Integer | `0` | `0`, `1`, `2` | State: `0`=off, `1`=charging, `2`=holding |
| `bypass_charger` | RW (`0660`) | Boolean | `0` | `0` or `1` | Direct USB power pass-through |
| `usb_icl_override_ma`| RW (`0660`) | Integer (mA) | `0` (stock) | `0` or `100..1500` | Overrides SDP USB 500mA input cap |
| `ignore_vendor_charging_thermal` | RW (`0660`) | Boolean | `0` | `0` or `1` | Master toggle to ignore `mi_thermald` |
| `vendor_thermal_max_level` | RW (`0660`) | Integer | `18` | `0..24` | Caps vendor charging throttle step |
| `ignore_vendor_cpu_thermal` | RW (`0660`) | Boolean | `0` | `0` or `1` | Ignores `mi_thermald` CPU downclocking |
| `cpu_little_override_max_state` | RW (`0660`) | Unsigned Int | `0` | `0..N` | Little cluster cooling state floor |
| `cpu_big_override_max_state` | RW (`0660`) | Unsigned Int | `0` | `0..N` | Big cluster cooling state floor |
| `ignore_vendor_gpu_thermal` | RW (`0660`) | Boolean | `0` | `0` or `1` | Ignores `mi_thermald` GPU downclocking |
| `gpu_override_max_state` | RW (`0660`) | Unsigned Int | `0` | `0..N` | GPU cooling state floor |
| `ignore_vendor_backlight_thermal` | RW (`0660`) | Boolean | `0` | `0` or `1` | Stops screen dimming when warm |
| `battery_trip_decic` | RW (`0660`) | Unsigned Int | `450` (45.0°C)| `400..470` | Hard battery temperature tripwire |
| `battery_clear_decic`| RW (`0660`) | Unsigned Int | `420` (42.0°C)| `300..(trip - 20)` | Cool-down re-enable threshold |
| `skin_override_max_mc`| RW (`0660`) | Integer (m°C)| `0` (off) | `0` or `45000..60000` | Optional chassis skin backstop |

---

### Detailed Breakdown of Every Node

#### 1. `charge_stop_pct`
- **Path:** `/sys/kernel/vector/charge_stop_pct`
- **Description:** Controls the battery charge limiter. When enabled, the kernel continuously samples battery state of charge (SoC) using true Coulomb Counter capacity (`cc_soc`) and fuel gauge metrics.
- **Behavior:** Once the battery percentage reaches the `charge_stop_pct` threshold, charging transitions to **Hold Mode**, powering the system directly from USB while waiting for the battery level to fall to `charge_resume_pct`.
- **Syntax:**
  - `echo 0 > charge_stop_pct`: Disables limiter (charges to 100%).
  - `echo "80 75" > charge_stop_pct`: Sets stop threshold to 80% and resume threshold to 75% simultaneously.

#### 2. `charge_resume_pct`
- **Path:** `/sys/kernel/vector/charge_resume_pct`
- **Description:** Sets the battery percentage at which charging resumes after the limiter has paused it.
- **Constraint:** Strictly enforced to be at least 2% below `charge_stop_pct` (`resume <= stop - 2`). This hysteresis gap stops charging from continuously clicking on and off.

#### 3. `charge_limit_state`
- **Path:** `/sys/kernel/vector/charge_limit_state`
- **Description:** Read-only state machine monitor:
  - `0`: Limiter off / disabled.
  - `1`: Limiter active; battery is currently charging (or cable is unplugged).
  - `2`: Limiter active; battery reached stop target and is holding at limit.
- **Events:** Fires `sysfs_notify` on state transitions for instant updates in apps.

#### 4. `bypass_charger`
- **Path:** `/sys/kernel/vector/bypass_charger`
- **Description:** Direct charging bypass (pass-through power). When set to `1`:
  - Disables battery charging via the main IIO charging channel.
  - Powers the phone directly from the USB charger.
  - Battery stays idle, completely eliminating charging heat and electrochemical cycling.
- **Safety Backstops:** Automatically resets to `0` when the USB cable is unplugged, or if battery drops below 10%.

#### 5. `usb_icl_override_ma`
- **Path:** `/sys/kernel/vector/usb_icl_override_ma`
- **Description:** Overrides the Input Current Limit (ICL) for chargers detected as standard USB (SDP), which normally draw only 500 mA.
- **Range:** Nonzero values below 100 mA are raised to 100 mA; values above 1500 mA are clamped to 1500 mA.
- **Reset:** Clears back to `0` automatically when unplugged. DCP/HVDCP/PD/Xiaomi 67W fast chargers are unaffected.

#### 6. `ignore_vendor_charging_thermal`
- **Path:** `/sys/kernel/vector/ignore_vendor_charging_thermal`
- **Description:** The master thermal switch. When set to `1`:
  - Bypasses `mi_thermald` charging throttle tables.
  - Allows full fast-charging speed while the battery cell remains below `battery_trip_decic`.
  - Required as the master prerequisite for CPU, GPU, and display overrides.

#### 7. `vendor_thermal_max_level`
- **Path:** `/sys/kernel/vector/vendor_thermal_max_level`
- **Description:** Caps the highest vendor throttle step allowed when `ignore_vendor_charging_thermal` is active.
- **Range:** `0` (ignores vendor throttling completely) to `24` (maximum vendor throttle / ~500 mA). Default is `18` (~3500 mA cap).

#### 8. `ignore_vendor_cpu_thermal`
- **Path:** `/sys/kernel/vector/ignore_vendor_cpu_thermal`
- **Description:** Stops `mi_thermald` from capping CPU cluster frequencies based on chassis skin temperature. Keeps Little and Big cores at full performance during intense workloads.

#### 9. `cpu_little_override_max_state`
- **Path:** `/sys/kernel/vector/cpu_little_override_max_state`
- **Description:** Controls the cooling state floor for the Cortex-A55 Little cluster.
- **Values:** `0` = unthrottled maximum frequency (1.80 GHz). Higher numbers allow the kernel to throttle down to that specific cooling state index.

#### 10. `cpu_big_override_max_state`
- **Path:** `/sys/kernel/vector/cpu_big_override_max_state`
- **Description:** Controls the cooling state floor for the Cortex-A76 Big cluster.
- **Values:** `0` = unthrottled maximum frequency (2.21 GHz).

#### 11. `ignore_vendor_gpu_thermal`
- **Path:** `/sys/kernel/vector/ignore_vendor_gpu_thermal`
- **Description:** Prevents `mi_thermald` from downclocking the Adreno 619 GPU based on frame warmth. Eliminates thermal stutter in 3D gaming.

#### 12. `gpu_override_max_state`
- **Path:** `/sys/kernel/vector/gpu_override_max_state`
- **Description:** Controls the cooling state floor for the Adreno GPU. `0` = unthrottled (840 MHz).

#### 13. `ignore_vendor_backlight_thermal`
- **Path:** `/sys/kernel/vector/ignore_vendor_backlight_thermal`
- **Description:** Stops `mi_thermald` from forcibly dimming the AMOLED screen when the device is warm.

#### 14. `battery_trip_decic`
- **Path:** `/sys/kernel/vector/battery_trip_decic`
- **Description:** The primary kernel safety tripwire. Sets the maximum battery temperature (in decidegrees Celsius, where $10 = 1.0\text{°C}$) at which overrides are permitted to run.
- **Action:** If the battery cell hits this value (default `450` = 45.0°C), all overrides are instantly cancelled and stock vendor throttling is engaged.

#### 15. `battery_clear_decic`
- **Path:** `/sys/kernel/vector/battery_clear_decic`
- **Description:** The cool-down re-enable threshold. After a thermal trip, overrides remain disabled until the cell cools below this value.
- **Constraint:** Clamped to `battery_clear_decic <= battery_trip_decic - 20` to guarantee at least 2.0°C of hysteresis.

#### 16. `skin_override_max_mc`
- **Path:** `/sys/kernel/vector/skin_override_max_mc`
- **Description:** Optional exterior chassis backstop (in millidegrees Celsius, where $1000 = 1.0\text{°C}$).
- **Action:** Monitors `sdm-skin-therm-usr`. If the frame reaches this temperature (e.g. `50000` = 50.0°C), CPU, GPU, and display overrides are revoked until it cools 2.0°C below the threshold.

---

## 5. Recommended Profiles

### A. Daily Longevity Profile
Optimized for overnight charging and multi-year battery health:
```sh
echo "80 75" > /sys/kernel/vector/charge_stop_pct
echo 0 > /sys/kernel/vector/bypass_charger
echo 0 > /sys/kernel/vector/ignore_vendor_charging_thermal
echo 0 > /sys/kernel/vector/ignore_vendor_cpu_thermal
```

### B. Unthrottled Gaming & Emulation Profile
Maximum sustained GPU/CPU performance, powered directly from USB without heating the battery:
```sh
echo 1 > /sys/kernel/vector/bypass_charger
echo 1 > /sys/kernel/vector/ignore_vendor_charging_thermal
echo 0 > /sys/kernel/vector/vendor_thermal_max_level
echo 1 > /sys/kernel/vector/ignore_vendor_cpu_thermal
echo 1 > /sys/kernel/vector/ignore_vendor_gpu_thermal
echo 1 > /sys/kernel/vector/ignore_vendor_backlight_thermal
echo 440 > /sys/kernel/vector/battery_trip_decic
echo 410 > /sys/kernel/vector/battery_clear_decic
echo 50000 > /sys/kernel/vector/skin_override_max_mc
```

### C. Factory Stock Reset
Reverts all parameters to stock kernel defaults:
```sh
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
```

---

## 6. Boot Persistence via Magisk / KernelSU

To persist settings across reboots without the Vector Link app, create an executable script in `/data/adb/service.d/99-vector-link.sh`:

```sh
#!/system/bin/sh
VDIR="/sys/kernel/vector"

# Wait up to 60s for the sysfs node to initialize
for i in $(seq 1 60); do
    if [ -d "$VDIR" ]; then
        break
    fi
    sleep 1
done

[ ! -d "$VDIR" ] && exit 0

# Apply your desired settings
echo "80 75" > "$VDIR/charge_stop_pct"
echo 1 > "$VDIR/ignore_vendor_charging_thermal"
echo 0 > "$VDIR/vendor_thermal_max_level"
echo 1 > "$VDIR/ignore_vendor_cpu_thermal"
echo 1 > "$VDIR/ignore_vendor_gpu_thermal"
echo 1 > "$VDIR/ignore_vendor_backlight_thermal"
echo 450 > "$VDIR/battery_trip_decic"
echo 420 > "$VDIR/battery_clear_decic"
```

Set executable permissions:
```sh
chmod 0755 /data/adb/service.d/99-vector-link.sh
chown root:root /data/adb/service.d/99-vector-link.sh
```
