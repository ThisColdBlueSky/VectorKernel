#!/bin/sh
# Vector Kernel Magisk/KernelSU Installer
# Author: rhythmcache & THIS COLD BLUE SKY

ui_print "***************************************************"
ui_print "               Vector Kernel Flasher               "
ui_print "     POCO X4 Pro 5G / Redmi Note 11 Pro 5G         "
ui_print "                 (veux / peux)                     "
ui_print "***************************************************"
ui_print ""

# Ensure tools permissions
chmod -R 755 "$MODPATH/tools" 2>/dev/null
export PATH="$MODPATH/tools:$PATH"

MB="$MODPATH/tools/magiskboot"
[ -x "$MB" ] || MB=$(which magiskboot 2>/dev/null)
[ -x "$MB" ] || MB="/data/adb/magisk/magiskboot"
[ -x "$MB" ] || MB="/data/adb/ksu/bin/magiskboot"
[ -x "$MB" ] || MB="/data/adb/ap/bin/magiskboot"

if [ ! -x "$MB" ]; then
    abort "! Error: magiskboot binary not found. Cannot proceed."
fi

# Detect Current Slot
CURRENT_SLOT=$(getprop ro.boot.slot_suffix 2>/dev/null)
[ -z "$CURRENT_SLOT" ] && CURRENT_SLOT=$(grep -o 'androidboot.slot_suffix=.*$' /proc/cmdline | cut -d\  -f1 | cut -d= -f2)
[ -z "$CURRENT_SLOT" ] && CURRENT_SLOT="_a"

if [ "$CURRENT_SLOT" = "_a" ]; then
    OTHER_SLOT="_b"
else
    OTHER_SLOT="_a"
fi

# Keypress detection helper (Vol+ = 1, Vol- = 2, Timeout = 0)
choose_key() {
    local timeout=$1
    while true; do
        event=$(timeout ${timeout} getevent -qlc 1 2>/dev/null)
        exitcode=$?
        if [ "$exitcode" -eq 124 ] || [ "$exitcode" -eq 143 ]; then
            return 0
        fi
        if echo "$event" | grep -q "KEY_VOLUMEUP"; then
            return 1
        elif echo "$event" | grep -q "KEY_VOLUMEDOWN"; then
            return 2
        fi
    done
}

# Helper to check and ensure block device is Read-Write
ensure_block_rw() {
    local blk="$1"
    if [ -b "$blk" ]; then
        if [ "$(blockdev --getro "$blk" 2>/dev/null)" = "1" ]; then
            ui_print "- Setting partition $(basename "$blk") to Read-Write..."
            blockdev --setrw "$blk" 2>/dev/null
        fi
    fi
}

# --- STEP 1: Slot Selection ---
ui_print "- Current active slot: $CURRENT_SLOT"
ui_print ""
ui_print "- Select target slot to flash:"
ui_print "  - Press VOLUME UP   (+) for Active Slot only ($CURRENT_SLOT)"
ui_print "  - Press VOLUME DOWN (-) for Both Slots ($CURRENT_SLOT & $OTHER_SLOT)"
ui_print "  (Waiting 10s... defaults to Active Slot)"
ui_print ""

TARGET_SLOTS="$CURRENT_SLOT"
choose_key 10
key_slot=$?

if [ $key_slot -eq 2 ]; then
    ui_print "- Selected: Both Slots ($CURRENT_SLOT & $OTHER_SLOT)"
    TARGET_SLOTS="$CURRENT_SLOT $OTHER_SLOT"
else
    ui_print "- Selected: Active Slot only ($CURRENT_SLOT)"
    TARGET_SLOTS="$CURRENT_SLOT"
fi
ui_print ""

# --- STEP 2: Backup Selection ---
ui_print "- Backup current kernel partitions?"
ui_print "  - Press VOLUME UP   (+) for YES (Backup to /data/adb & /sdcard)"
ui_print "  - Press VOLUME DOWN (-) for NO  (Skip backup)"
ui_print "  (Waiting 10s... defaults to YES)"
ui_print ""

choose_key 10
key_backup=$?

BACKUP_DIR="/data/adb/vector_kernel_backup"
SDCARD_DIR="/data/media/0/Vector_Backups"
[ -d /sdcard ] && SDCARD_DIR="/sdcard/Vector_Backups"

if [ $key_backup -ne 2 ]; then
    ui_print "- Backing up current partitions..."
    mkdir -p "$BACKUP_DIR"
    mkdir -p "$SDCARD_DIR" 2>/dev/null
    echo "$CURRENT_SLOT" > "$BACKUP_DIR/slot.info"

    for s in $TARGET_SLOTS; do
        if [ -b "/dev/block/by-name/boot$s" ]; then
            ui_print "  - Backing up boot$s..."
            dd if="/dev/block/by-name/boot$s" of="$BACKUP_DIR/boot$s.img" bs=4M 2>/dev/null
            cp -f "$BACKUP_DIR/boot$s.img" "$SDCARD_DIR/" 2>/dev/null
        fi
        if [ -b "/dev/block/by-name/vendor_boot$s" ]; then
            ui_print "  - Backing up vendor_boot$s..."
            dd if="/dev/block/by-name/vendor_boot$s" of="$BACKUP_DIR/vendor_boot$s.img" bs=4M 2>/dev/null
            cp -f "$BACKUP_DIR/vendor_boot$s.img" "$SDCARD_DIR/" 2>/dev/null
        fi
        if [ -b "/dev/block/by-name/dtbo$s" ]; then
            ui_print "  - Backing up dtbo$s..."
            dd if="/dev/block/by-name/dtbo$s" of="$BACKUP_DIR/dtbo$s.img" bs=4M 2>/dev/null
            cp -f "$BACKUP_DIR/dtbo$s.img" "$SDCARD_DIR/" 2>/dev/null
        fi
    done
    ui_print "- Backup saved to $BACKUP_DIR"
    ui_print "- Copy also saved to /sdcard/Vector_Backups/"
else
    ui_print "- Backup skipped by user"
fi
ui_print ""

# --- STEP 3: Kernel & DTB Flashing ---
WORK_DIR="/dev/tmp/vector_flash"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

for slot in $TARGET_SLOTS; do
    ui_print "- Processing slot $slot..."

    BOOT_BLK="/dev/block/by-name/boot$slot"
    VBOOT_BLK="/dev/block/by-name/vendor_boot$slot"
    DTBO_BLK="/dev/block/by-name/dtbo$slot"

    # Ensure partitions are Read-Write
    ensure_block_rw "$BOOT_BLK"
    ensure_block_rw "$VBOOT_BLK"
    ensure_block_rw "$DTBO_BLK"

    # Flash boot partition
    if [ -b "$BOOT_BLK" ] && [ -f "$MODPATH/Image" ]; then
        ui_print "- Flashing kernel Image into boot$slot..."
        cd "$WORK_DIR" && rm -rf *
        "$MB" unpack "$BOOT_BLK" >/dev/null 2>&1
        cp -f "$MODPATH/Image" kernel
        "$MB" repack "$BOOT_BLK" new-boot.img >/dev/null 2>&1
        if [ -f new-boot.img ]; then
            dd if=new-boot.img of="$BOOT_BLK" bs=4M conv=fsync 2>/dev/null
            ui_print "- boot$slot updated successfully"
        else
            abort "! Failed to repack boot$slot. Aborting."
        fi
    fi

    # Flash vendor_boot partition
    VBOOT_BLK="/dev/block/by-name/vendor_boot$slot"
    if [ -b "$VBOOT_BLK" ] && [ -f "$MODPATH/dtb" ]; then
        ui_print "- Flashing DTB into vendor_boot$slot..."
        cd "$WORK_DIR" && rm -rf *
        "$MB" unpack "$VBOOT_BLK" >/dev/null 2>&1
        cp -f "$MODPATH/dtb" dtb
        "$MB" repack "$VBOOT_BLK" new-vendor_boot.img >/dev/null 2>&1
        if [ -f new-vendor_boot.img ]; then
            dd if=new-vendor_boot.img of="$VBOOT_BLK" bs=4M conv=fsync 2>/dev/null
            ui_print "- vendor_boot$slot updated successfully"
        else
            abort "! Failed to repack vendor_boot$slot. Aborting."
        fi
    fi

    # Erase DTBO partition
    DTBO_BLK="/dev/block/by-name/dtbo$slot"
    if [ -b "$DTBO_BLK" ]; then
        ui_print "- Erasing dtbo$slot..."
        DTBO_SIZE=$(blockdev --getsize64 "$DTBO_BLK" 2>/dev/null)
        if [ -n "$DTBO_SIZE" ] && [ "$DTBO_SIZE" -gt 0 ]; then
            dd if=/dev/zero of="$DTBO_BLK" count=1 bs="$DTBO_SIZE" conv=fsync 2>/dev/null
        fi
    fi
done

# Cleanup work dir
cd /
rm -rf "$WORK_DIR" 2>/dev/null

# Clean up installer-only files but KEEP module, action.sh, Image, dtb, and tools
rm -rf "$MODPATH/META-INF" "$MODPATH/anykernel.sh" "$MODPATH/cmdline" "$MODPATH/banner" 2>/dev/null

# Set module permissions
set_perm_recursive "$MODPATH" 0 0 0755 0644
chmod 755 "$MODPATH/action.sh" 2>/dev/null
chmod -R 755 "$MODPATH/tools" 2>/dev/null

ui_print ""
ui_print "***************************************************"
ui_print "      Vector Kernel installed successfully!        "
ui_print "***************************************************"
ui_print ""
ui_print "- Flashed directly to kernel partitions"
ui_print "- Module active: Action button enabled in manager"
ui_print "- Backups saved safely to /sdcard/Vector_Backups/"
ui_print ""
ui_print "- Please reboot your device now"
ui_print ""

exit 0
