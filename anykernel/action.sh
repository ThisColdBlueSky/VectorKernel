#!/system/bin/sh
# Vector Kernel Action Script
# Executed via KernelSU / APatch / Magisk Manager Action button or terminal
# Author: rhythmcache & THIS COLD BLUE SKY

MODDIR="$(dirname "$(readlink -f "$0")")"
[ -d "$MODDIR" ] || MODDIR="/data/adb/modules/vector_kernel"

echo "***************************************************"
echo "             Vector Kernel Action Menu             "
echo "     POCO X4 Pro 5G / Redmi Note 11 Pro 5G         "
echo "                 (veux / peux)                     "
echo "***************************************************"
echo ""

# Locate magiskboot
MB="$MODDIR/tools/magiskboot"
[ -x "$MB" ] || MB=$(which magiskboot 2>/dev/null)
[ -x "$MB" ] || MB="/data/adb/magisk/magiskboot"
[ -x "$MB" ] || MB="/data/adb/ksu/bin/magiskboot"
[ -x "$MB" ] || MB="/data/adb/ap/bin/magiskboot"

if [ ! -x "$MB" ]; then
    echo "! Error: magiskboot binary not found. Cannot proceed."
    exit 1
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

# Helper to ensure block device is Read-Write
ensure_block_rw() {
    local blk="$1"
    if [ -b "$blk" ]; then
        if [ "$(blockdev --getro "$blk" 2>/dev/null)" = "1" ]; then
            echo "- Setting partition $(basename "$blk") to Read-Write..."
            blockdev --setrw "$blk" 2>/dev/null
        fi
    fi
}

# Parse CLI arguments if provided
ACTION_MODE=""
TARGET_SLOTS=""

case "$1" in
    *reflash*|*flash*) ACTION_MODE="reflash" ;;
    *restore*)         ACTION_MODE="restore" ;;
esac

case "$2" in
    *both*)             TARGET_SLOTS="$CURRENT_SLOT $OTHER_SLOT" ;;
    *inactive*|*other*) TARGET_SLOTS="$OTHER_SLOT" ;;
    *active*)           TARGET_SLOTS="$CURRENT_SLOT" ;;
    *a*)                TARGET_SLOTS="_a" ;;
    *b*)                TARGET_SLOTS="_b" ;;
esac

echo "- Current active slot: $CURRENT_SLOT"
echo "- Inactive (OTA) slot: $OTHER_SLOT"
echo ""

# Interactive Action Selection if not specified via CLI
if [ -z "$ACTION_MODE" ]; then
    echo "- Select an option:"
    echo "  - Press VOLUME UP   (+) for Reflash Vector Kernel"
    echo "  - Press VOLUME DOWN (-) for Restore Backup (boot, vendor_boot, dtbo)"
    echo "  (Waiting 20s... defaults to Cancel)"
    echo ""

    choose_key 20
    key_action=$?

    if [ $key_action -eq 1 ]; then
        ACTION_MODE="reflash"
    elif [ $key_action -eq 2 ]; then
        ACTION_MODE="restore"
    else
        echo "- No selection made or timed out. Exiting."
        exit 0
    fi
fi

# Interactive Slot Selection if not specified via CLI
if [ -z "$TARGET_SLOTS" ]; then
    echo ""
    echo "- Select target slot:"
    echo "  - Press VOLUME UP   (+) for Active Slot ($CURRENT_SLOT)"
    echo "  - Press VOLUME DOWN (-) for Inactive or Both Slots..."
    echo "  (Waiting 15s... defaults to Active Slot)"
    echo ""

    choose_key 15
    key_slot=$?

    if [ $key_slot -eq 2 ]; then
        echo "- Choose target slot:"
        echo "  - Press VOLUME UP   (+) for Inactive Slot ($OTHER_SLOT - post-OTA)"
        echo "  - Press VOLUME DOWN (-) for Both Slots ($CURRENT_SLOT & $OTHER_SLOT)"
        echo "  (Waiting 15s... defaults to Inactive Slot)"
        echo ""

        choose_key 15
        key_sub=$?
        if [ $key_sub -eq 2 ]; then
            echo "- Selected: Both Slots ($CURRENT_SLOT & $OTHER_SLOT)"
            TARGET_SLOTS="$CURRENT_SLOT $OTHER_SLOT"
        else
            echo "- Selected: Inactive Slot only ($OTHER_SLOT)"
            TARGET_SLOTS="$OTHER_SLOT"
        fi
    else
        echo "- Selected: Active Slot only ($CURRENT_SLOT)"
        TARGET_SLOTS="$CURRENT_SLOT"
    fi
fi
echo ""

# ---------------------------------------------------------
# Action 1: Reflash Vector Kernel
# ---------------------------------------------------------
if [ "$ACTION_MODE" = "reflash" ]; then
    echo "==================================================="
    echo "         Reflashing Vector Kernel                  "
    echo "==================================================="

    if [ ! -f "$MODDIR/Image" ]; then
        echo "! Error: Kernel Image not found at $MODDIR/Image"
        exit 1
    fi
    if [ ! -f "$MODDIR/dtb" ]; then
        echo "! Error: DTB not found at $MODDIR/dtb"
        exit 1
    fi

    WORK_DIR="/dev/tmp/vector_action_flash"
    rm -rf "$WORK_DIR"
    mkdir -p "$WORK_DIR"

    for slot in $TARGET_SLOTS; do
        echo "- Processing slot $slot..."
        BOOT_BLK="/dev/block/by-name/boot$slot"
        VBOOT_BLK="/dev/block/by-name/vendor_boot$slot"
        DTBO_BLK="/dev/block/by-name/dtbo$slot"

        ensure_block_rw "$BOOT_BLK"
        ensure_block_rw "$VBOOT_BLK"
        ensure_block_rw "$DTBO_BLK"

        # Safety auto-backup if not already backed up
        if [ ! -f "/data/adb/vector_kernel_backup/boot$slot.img" ] && [ ! -f "/sdcard/Vector_Backups/boot$slot.img" ]; then
            echo "- Creating safety backup for slot $slot..."
            mkdir -p /data/adb/vector_kernel_backup /sdcard/Vector_Backups 2>/dev/null
            [ -b "$BOOT_BLK" ] && dd if="$BOOT_BLK" of="/data/adb/vector_kernel_backup/boot$slot.img" bs=4M 2>/dev/null
            [ -b "$VBOOT_BLK" ] && dd if="$VBOOT_BLK" of="/data/adb/vector_kernel_backup/vendor_boot$slot.img" bs=4M 2>/dev/null
            [ -b "$DTBO_BLK" ] && dd if="$DTBO_BLK" of="/data/adb/vector_kernel_backup/dtbo$slot.img" bs=4M 2>/dev/null
            cp -f /data/adb/vector_kernel_backup/*$slot.img /sdcard/Vector_Backups/ 2>/dev/null
        fi

        # Flash boot
        if [ -b "$BOOT_BLK" ]; then
            echo "- Flashing kernel Image into boot$slot..."
            cd "$WORK_DIR" && rm -rf *
            "$MB" unpack "$BOOT_BLK" >/dev/null 2>&1
            cp -f "$MODDIR/Image" kernel
            "$MB" repack "$BOOT_BLK" new-boot.img >/dev/null 2>&1
            if [ -f new-boot.img ]; then
                dd if=new-boot.img of="$BOOT_BLK" bs=4M conv=fsync 2>/dev/null
                echo "- boot$slot updated successfully"
            else
                echo "! Error: Failed to repack boot$slot. Aborting."
                rm -rf "$WORK_DIR"
                exit 1
            fi
        fi

        # Flash vendor_boot
        if [ -b "$VBOOT_BLK" ]; then
            echo "- Flashing DTB into vendor_boot$slot..."
            cd "$WORK_DIR" && rm -rf *
            "$MB" unpack "$VBOOT_BLK" >/dev/null 2>&1
            cp -f "$MODDIR/dtb" dtb
            "$MB" repack "$VBOOT_BLK" new-vendor_boot.img >/dev/null 2>&1
            if [ -f new-vendor_boot.img ]; then
                dd if=new-vendor_boot.img of="$VBOOT_BLK" bs=4M conv=fsync 2>/dev/null
                echo "- vendor_boot$slot updated successfully"
            else
                echo "! Error: Failed to repack vendor_boot$slot. Aborting."
                rm -rf "$WORK_DIR"
                exit 1
            fi
        fi

        # Erase DTBO
        if [ -b "$DTBO_BLK" ]; then
            echo "- Erasing dtbo$slot..."
            DTBO_SIZE=$(blockdev --getsize64 "$DTBO_BLK" 2>/dev/null)
            if [ -n "$DTBO_SIZE" ] && [ "$DTBO_SIZE" -gt 0 ]; then
                dd if=/dev/zero of="$DTBO_BLK" count=1 bs="$DTBO_SIZE" conv=fsync 2>/dev/null
            fi
        fi
    done

    cd /
    rm -rf "$WORK_DIR" 2>/dev/null
    sync

    echo ""
    echo "***************************************************"
    echo "  Vector Kernel reflashed to $TARGET_SLOTS successfully! "
    echo "***************************************************"
    echo "- Please reboot your device now"
    echo ""
    exit 0
fi

# ---------------------------------------------------------
# Action 2: Restore Old Boot Images
# ---------------------------------------------------------
if [ "$ACTION_MODE" = "restore" ]; then
    echo "==================================================="
    echo "             Restoring Kernel Backup               "
    echo "==================================================="

    BACKUP_SRC=""
    if [ -d "/data/adb/vector_kernel_backup" ] && [ -n "$(ls /data/adb/vector_kernel_backup/*.img 2>/dev/null)" ]; then
        BACKUP_SRC="/data/adb/vector_kernel_backup"
    elif [ -d "/sdcard/Vector_Backups" ] && [ -n "$(ls /sdcard/Vector_Backups/*.img 2>/dev/null)" ]; then
        BACKUP_SRC="/sdcard/Vector_Backups"
    elif [ -d "/data/media/0/Vector_Backups" ] && [ -n "$(ls /data/media/0/Vector_Backups/*.img 2>/dev/null)" ]; then
        BACKUP_SRC="/data/media/0/Vector_Backups"
    fi

    if [ -z "$BACKUP_SRC" ]; then
        echo "! Error: No partition backups found in /data/adb/vector_kernel_backup or /sdcard/Vector_Backups."
        exit 1
    fi

    echo "- Using backup source: $BACKUP_SRC"
    for slot in $TARGET_SLOTS; do
        echo "- Restoring slot $slot..."
        BOOT_BLK="/dev/block/by-name/boot$slot"
        VBOOT_BLK="/dev/block/by-name/vendor_boot$slot"
        DTBO_BLK="/dev/block/by-name/dtbo$slot"

        ensure_block_rw "$BOOT_BLK"
        ensure_block_rw "$VBOOT_BLK"
        ensure_block_rw "$DTBO_BLK"

        # Restore boot
        B_IMG="$BACKUP_SRC/boot$slot.img"
        [ -f "$B_IMG" ] || B_IMG="/sdcard/Vector_Backups/boot$slot.img"
        if [ -f "$B_IMG" ] && [ -b "$BOOT_BLK" ]; then
            echo "- Restoring boot$slot from $(basename "$B_IMG")..."
            dd if="$B_IMG" of="$BOOT_BLK" bs=4M conv=fsync 2>/dev/null
            echo "- boot$slot restored successfully"
        else
            echo "! Note: boot backup for $slot not found ($B_IMG)"
        fi

        # Restore vendor_boot
        VB_IMG="$BACKUP_SRC/vendor_boot$slot.img"
        [ -f "$VB_IMG" ] || VB_IMG="/sdcard/Vector_Backups/vendor_boot$slot.img"
        if [ -f "$VB_IMG" ] && [ -b "$VBOOT_BLK" ]; then
            echo "- Restoring vendor_boot$slot from $(basename "$VB_IMG")..."
            dd if="$VB_IMG" of="$VBOOT_BLK" bs=4M conv=fsync 2>/dev/null
            echo "- vendor_boot$slot restored successfully"
        else
            echo "! Note: vendor_boot backup for $slot not found ($VB_IMG)"
        fi

        # Restore dtbo
        DTBO_IMG="$BACKUP_SRC/dtbo$slot.img"
        [ -f "$DTBO_IMG" ] || DTBO_IMG="/sdcard/Vector_Backups/dtbo$slot.img"
        if [ -f "$DTBO_IMG" ] && [ -b "$DTBO_BLK" ]; then
            echo "- Restoring dtbo$slot from $(basename "$DTBO_IMG")..."
            dd if="$DTBO_IMG" of="$DTBO_BLK" bs=4M conv=fsync 2>/dev/null
            echo "- dtbo$slot restored successfully"
        else
            echo "! Note: dtbo backup for $slot not found ($DTBO_IMG)"
        fi
    done

    sync

    echo ""
    echo "***************************************************"
    echo "  Backup restored to $TARGET_SLOTS successfully!   "
    echo "***************************************************"
    echo "- Partitions have been restored"
    echo "- Please reboot your device now"
    echo ""
    exit 0
fi
