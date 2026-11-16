#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AVFoundation/AVFoundation.h>
#import <GameController/GameController.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hid/IOHIDLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#include <os/lock.h>
#include <mach/mach_time.h>
#include "../engine96k/eng_app.h"
#include "../engine96k/eng_mesh.h"
#include "rhi_metal.h"

// ============================================================================
// Native macOS IOHID Fallback for Xbox Nano Controller (045E:0B4E USB / 0B4F BLE)
// macOS GameController.framework does not yet recognize PIDs 0x0B4E / 0x0B4F and
// misclassifies the composite HID descriptor (GamePad + Keyboard + Mouse) as
// GCMouse with zero GCControllers. We subscribe strictly to Report ID 1's 22
// validated gamepad controls, excluding Keyboard/Mouse reports and Consumer
// Record (0x0C:0xB2).
// ============================================================================
enum {
    NANO_LX = 0,
    NANO_LY = 1,
    NANO_RX = 2,
    NANO_RY = 3,
    NANO_LT = 4,
    NANO_RT = 5,
    NANO_HAT = 6,
    NANO_BTN_BASE = 7, // Buttons 1..15 -> indices 7..21
    NANO_CHAN_COUNT = 22,
};
#define NANO_ALL_MASK ((1u << NANO_CHAN_COUNT) - 1u)

typedef enum {
    NANO_STATUS_INACTIVE = 0,
    NANO_STATUS_ABSENT,
    NANO_STATUS_NEEDS_PERMISSION,
    NANO_STATUS_AWAITING_NEUTRAL,
    NANO_STATUS_READY,
} NanoHIDStatus;

typedef struct NanoSlot {
    IOHIDDeviceRef      dev;
    CFArrayRef          elems_owner;
    IOHIDElementRef     chan_elem[NANO_CHAN_COUNT];
    IOHIDElementCookie  chan_cookie[NANO_CHAN_COUNT];
    int                 raw_val[NANO_CHAN_COUNT];
    uint32_t            seen_mask;
    uint64_t            registry_id;
    uint64_t            accept_after;
    int                 product_id; // 0x0B4E (USB) or 0x0B4F (BLE)
    bool                has_live_cb;
    bool                armed;
    bool                cancelling;
} NanoSlot;

static os_unfair_lock g_nano_lock = OS_UNFAIR_LOCK_INIT;
static bool           g_nano_want_active = false;
static GamepadState   g_nano_pad = {0};
static NanoHIDStatus  g_nano_status = NANO_STATUS_INACTIVE;
static int            g_nano_transports = 0; // bit0 = USB (0B4E), bit1 = BLE (0B4F)

static dispatch_queue_t      g_nano_queue = nil;
static dispatch_source_t     g_nano_timer = nil;
static NanoSlot             *g_nano_slots[4] = {NULL};

static int nano_cfnum_int(CFTypeRef ref, int fallback) {
    if (ref && CFGetTypeID(ref) == CFNumberGetTypeID()) {
        int v = fallback;
        if (CFNumberGetValue((CFNumberRef)ref, kCFNumberIntType, &v)) return v;
    }
    return fallback;
}

static bool nano_element_ancestry_ok(IOHIDElementRef el) {
    IOHIDElementRef cur = IOHIDElementGetParent(el);
    bool has_gp = false, has_km = false;
    int depth = 0;
    while (cur && depth++ < 16) {
        if (IOHIDElementGetType(cur) == kIOHIDElementTypeCollection &&
            IOHIDElementGetUsagePage(cur) == kHIDPage_GenericDesktop) {
            uint32_t u = IOHIDElementGetUsage(cur);
            if (u == kHIDUsage_GD_GamePad) has_gp = true;
            if (u == kHIDUsage_GD_Mouse || u == kHIDUsage_GD_Keyboard) has_km = true;
        }
        cur = IOHIDElementGetParent(cur);
    }
    return has_gp && !has_km;
}

static int nano_classify_element(IOHIDElementRef el, bool *out_is_aux) {
    *out_is_aux = false;
    if (IOHIDElementGetReportID(el) != 1) return -1;
    if (IOHIDElementIsRelative(el)) return -1;
    if (IOHIDElementGetReportCount(el) != 1) return -1;
    if (!nano_element_ancestry_ok(el)) return -1;

    IOHIDElementType t = IOHIDElementGetType(el);
    uint32_t up = IOHIDElementGetUsagePage(el);
    uint32_t u  = IOHIDElementGetUsage(el);
    CFIndex mn  = IOHIDElementGetLogicalMin(el);
    CFIndex mx  = IOHIDElementGetLogicalMax(el);
    uint32_t sz = IOHIDElementGetReportSize(el);

    if (t == kIOHIDElementTypeInput_Misc || t == kIOHIDElementTypeInput_Axis) {
        if (up == 1 && mn == 0 && mx == 65535 && sz == 16) {
            if (u == 48) return NANO_LX;
            if (u == 49) return NANO_LY;
            if (u == 50) return NANO_RX;
            if (u == 53) return NANO_RY;
        }
        if (up == 2 && mn == 0 && mx == 1023 && sz == 10) {
            if (u == 197) return NANO_LT;
            if (u == 196) return NANO_RT;
        }
        if (up == 1 && u == 57 && mn == 1 && mx == 8 && sz == 4) {
            return NANO_HAT;
        }
    } else if (t == kIOHIDElementTypeInput_Button) {
        if (up == 9 && u >= 1 && u <= 15 && mn == 0 && mx == 1 && sz == 1) {
            return NANO_BTN_BASE + (int)(u - 1);
        }
        if (up == 12 && u == 178 && mn == 0 && mx == 1 && sz == 1) {
            *out_is_aux = true;
            return -1;
        }
    }
    return -1;
}

static bool nano_raw_valid(int ch, long v) {
    if (ch >= NANO_LX && ch <= NANO_RY) return v >= 0 && v <= 65535;
    if (ch == NANO_LT || ch == NANO_RT) return v >= 0 && v <= 1023;
    if (ch == NANO_HAT) return v >= 0 && v <= 15;
    if (ch >= NANO_BTN_BASE && ch < NANO_CHAN_COUNT) return v == 0 || v == 1;
    return false;
}

static bool nano_slot_is_neutral(const NanoSlot *s) {
    if (s->seen_mask != NANO_ALL_MASK) return false;
    for (int i = NANO_LX; i <= NANO_RY; i++) {
        float norm = ((float)s->raw_val[i] / 32767.5f) - 1.0f;
        if (fabsf(norm) > 0.14f) return false;
    }
    if (s->raw_val[NANO_LT] > 52 || s->raw_val[NANO_RT] > 52) return false;
    int hat = s->raw_val[NANO_HAT];
    if (hat >= 1 && hat <= 8) return false;
    for (int i = NANO_BTN_BASE; i < NANO_CHAN_COUNT; i++) {
        if (s->raw_val[i] != 0) return false;
    }
    return true;
}

static void nano_publish_locked_state(NanoHIDStatus fallback_status) {
    GamepadState merged = {0};
    int open_cnt = 0, armed_cnt = 0, trans_mask = 0;
    for (int i = 0; i < 4; i++) {
        NanoSlot *s = g_nano_slots[i];
        if (!s || s->cancelling) continue;
        open_cnt++;
        trans_mask |= (s->product_id == 0x0B4E) ? 1 : 2;
        if (!s->armed || s->seen_mask != NANO_ALL_MASK) continue;
        armed_cnt++;

        float lx = eng_deadzone(((float)s->raw_val[NANO_LX] / 32767.5f) - 1.0f, 0.14f);
        float ly = eng_deadzone(1.0f - ((float)s->raw_val[NANO_LY] / 32767.5f), 0.14f);
        float rx = eng_deadzone(((float)s->raw_val[NANO_RX] / 32767.5f) - 1.0f, 0.14f);
        float ry = eng_deadzone(1.0f - ((float)s->raw_val[NANO_RY] / 32767.5f), 0.14f);
        float lt = clampf((float)s->raw_val[NANO_LT] / 1023.0f, 0.0f, 1.0f);
        float rt = clampf((float)s->raw_val[NANO_RT] / 1023.0f, 0.0f, 1.0f);

        if (fabsf(lx) > fabsf(merged.lx)) merged.lx = lx;
        if (fabsf(ly) > fabsf(merged.ly)) merged.ly = ly;
        if (fabsf(rx) > fabsf(merged.rx)) merged.rx = rx;
        if (fabsf(ry) > fabsf(merged.ry)) merged.ry = ry;
        if (lt > merged.lt) merged.lt = lt;
        if (rt > merged.rt) merged.rt = rt;

        uint32_t h = 0;
        // Microsoft 15-button Xbox HID layout (Linux BTN_GAMEPAD 0x130..0x13e):
        // 1=A, 2=B, [3 gap], 4=X, 5=Y, [6 gap], 7=LB, 8=RB, [9,10 gaps], 11=View, 12=Menu, 13=Guide, 14=L3, 15=R3
        if (s->raw_val[NANO_BTN_BASE +  0]) h |= GP_BTN_A;
        if (s->raw_val[NANO_BTN_BASE +  1]) h |= GP_BTN_B;
        if (s->raw_val[NANO_BTN_BASE +  3]) h |= GP_BTN_X;
        if (s->raw_val[NANO_BTN_BASE +  4]) h |= GP_BTN_Y;
        if (s->raw_val[NANO_BTN_BASE +  6]) h |= GP_BTN_LB;
        if (s->raw_val[NANO_BTN_BASE +  7]) h |= GP_BTN_RB;
        if (s->raw_val[NANO_BTN_BASE + 10]) h |= GP_BTN_VIEW;
        if (s->raw_val[NANO_BTN_BASE + 11]) h |= GP_BTN_MENU;
        if (s->raw_val[NANO_BTN_BASE + 13]) h |= GP_BTN_L3;
        if (s->raw_val[NANO_BTN_BASE + 14]) h |= GP_BTN_R3;

        switch (s->raw_val[NANO_HAT]) {
            case 1: h |= GP_BTN_DPAD_U; break;
            case 2: h |= GP_BTN_DPAD_U | GP_BTN_DPAD_R; break;
            case 3: h |= GP_BTN_DPAD_R; break;
            case 4: h |= GP_BTN_DPAD_D | GP_BTN_DPAD_R; break;
            case 5: h |= GP_BTN_DPAD_D; break;
            case 6: h |= GP_BTN_DPAD_D | GP_BTN_DPAD_L; break;
            case 7: h |= GP_BTN_DPAD_L; break;
            case 8: h |= GP_BTN_DPAD_U | GP_BTN_DPAD_L; break;
            default: break;
        }
        merged.held |= h;
    }

    NanoHIDStatus st = fallback_status;
    if (open_cnt > 0) {
        merged.connected = (armed_cnt > 0);
        st = (armed_cnt > 0) ? NANO_STATUS_READY : NANO_STATUS_AWAITING_NEUTRAL;
    }

    os_unfair_lock_lock(&g_nano_lock);
    if (g_nano_want_active) {
        g_nano_pad = merged;
        g_nano_status = st;
        g_nano_transports = trans_mask;
    }
    os_unfair_lock_unlock(&g_nano_lock);
}

static void nano_seed_slot(NanoSlot *s) {
    if (!s || s->cancelling) return;
    for (int ch = 0; ch < NANO_CHAN_COUNT; ch++) {
        IOHIDValueRef val = NULL;
        if (IOHIDDeviceGetValue(s->dev, s->chan_elem[ch], &val) == kIOReturnSuccess && val) {
            long iv = IOHIDValueGetIntegerValue(val);
            if (nano_raw_valid(ch, iv)) {
                s->raw_val[ch] = (int)iv;
                s->seen_mask |= (1u << ch);
            }
        }
    }
    if (nano_slot_is_neutral(s)) {
        s->armed = true;
    }
}

static void nano_cancel_slot_idx(int idx) {
    NanoSlot *s = g_nano_slots[idx];
    if (!s) return;
    g_nano_slots[idx] = NULL;
    if (!s->cancelling) {
        s->cancelling = true;
        s->armed = false;
        IOHIDDeviceCancel(s->dev);
    }
}

static void nano_value_cb(void *ctx, IOReturn res, void *sender, IOHIDValueRef val) {
    (void)sender;
    NanoSlot *s = (NanoSlot *)ctx;
    if (!s || s->cancelling || res != kIOReturnSuccess || !val) return;
    if (IOHIDValueGetTimeStamp(val) <= s->accept_after) return;

    IOHIDElementRef el = IOHIDValueGetElement(val);
    IOHIDElementCookie ck = IOHIDElementGetCookie(el);
    int found_ch = -1;
    for (int ch = 0; ch < NANO_CHAN_COUNT; ch++) {
        if (s->chan_cookie[ch] == ck) { found_ch = ch; break; }
    }
    if (found_ch < 0) return;

    if (!s->has_live_cb) {
        s->has_live_cb = true;
        nano_seed_slot(s);
    }

    long iv = IOHIDValueGetIntegerValue(val);
    if (!nano_raw_valid(found_ch, iv)) {
        s->seen_mask = 0;
        s->armed = false;
        nano_publish_locked_state(NANO_STATUS_AWAITING_NEUTRAL);
        return;
    }
    s->raw_val[found_ch] = (int)iv;
    s->seen_mask |= (1u << found_ch);
    if (nano_slot_is_neutral(s)) {
        s->armed = true;
    }
    nano_publish_locked_state(NANO_STATUS_AWAITING_NEUTRAL);
}

static void nano_remove_cb(void *ctx, IOReturn res, void *sender) {
    (void)res; (void)sender;
    NanoSlot *s = (NanoSlot *)ctx;
    if (!s) return;
    for (int i = 0; i < 4; i++) {
        if (g_nano_slots[i] == s) {
            nano_cancel_slot_idx(i);
            break;
        }
    }
    nano_publish_locked_state(NANO_STATUS_ABSENT);
}

static void nano_open_service(io_service_t service, uint64_t reg_id, int pid) {
    int free_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (g_nano_slots[i] && g_nano_slots[i]->registry_id == reg_id) return;
        if (!g_nano_slots[i] && free_idx < 0) free_idx = i;
    }
    if (free_idx < 0) return;
    if (IOHIDCheckAccess(kIOHIDRequestTypeListenEvent) != kIOHIDAccessTypeGranted) return;

    IOHIDDeviceRef dev = IOHIDDeviceCreate(kCFAllocatorDefault, service);
    if (!dev) return;

    CFArrayRef elems = IOHIDDeviceCopyMatchingElements(dev, NULL, kIOHIDOptionsTypeNone);
    if (!elems) { CFRelease(dev); return; }

    CFIndex cnt = CFArrayGetCount(elems);
    IOHIDElementRef chan_el[NANO_CHAN_COUNT] = {NULL};
    IOHIDElementCookie chan_ck[NANO_CHAN_COUNT] = {0};
    uint32_t mask = 0;
    int aux_cnt = 0;
    bool schema_ok = (cnt > 0 && cnt <= 512);

    for (CFIndex i = 0; schema_ok && i < cnt; i++) {
        IOHIDElementRef el = (IOHIDElementRef)CFArrayGetValueAtIndex(elems, i);
        IOHIDElementType et = IOHIDElementGetType(el);
        if (et < kIOHIDElementTypeInput_Misc || et > kIOHIDElementTypeInput_ScanCodes) continue;
        if (IOHIDElementGetReportID(el) != 1) continue;

        bool is_aux = false;
        int ch = nano_classify_element(el, &is_aux);
        if (is_aux) {
            aux_cnt++;
            if (aux_cnt > 1) schema_ok = false;
            continue;
        }
        if (ch < 0 || (mask & (1u << ch)) != 0) {
            schema_ok = false;
            break;
        }
        mask |= (1u << ch);
        chan_el[ch] = el;
        chan_ck[ch] = IOHIDElementGetCookie(el);
    }

    if (!schema_ok || mask != NANO_ALL_MASK) {
        CFRelease(elems);
        CFRelease(dev);
        return;
    }

    NSMutableArray *matchArr = [NSMutableArray arrayWithCapacity:NANO_CHAN_COUNT];
    for (int ch = 0; ch < NANO_CHAN_COUNT; ch++) {
        [matchArr addObject:@{
            @(kIOHIDElementCookieKey): @((uint32_t)chan_ck[ch]),
            @(kIOHIDElementReportIDKey): @(1),
            @(kIOHIDElementUsagePageKey): @(IOHIDElementGetUsagePage(chan_el[ch])),
            @(kIOHIDElementUsageKey): @(IOHIDElementGetUsage(chan_el[ch])),
        }];
    }
    IOHIDDeviceSetInputValueMatchingMultiple(dev, (__bridge CFArrayRef)matchArr);

    if (IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        CFRelease(elems);
        CFRelease(dev);
        return;
    }

    NanoSlot *slot = (NanoSlot *)calloc(1, sizeof(NanoSlot));
    slot->dev = dev;
    slot->elems_owner = elems;
    slot->registry_id = reg_id;
    slot->product_id = pid;
    memcpy(slot->chan_elem, chan_el, sizeof(chan_el));
    memcpy(slot->chan_cookie, chan_ck, sizeof(chan_ck));

    IOHIDDeviceRegisterInputValueCallback(dev, nano_value_cb, slot);
    IOHIDDeviceRegisterRemovalCallback(dev, nano_remove_cb, slot);
    IOHIDDeviceSetDispatchQueue(dev, g_nano_queue);
    IOHIDDeviceSetCancelHandler(dev, ^{
        IOHIDDeviceClose(slot->dev, kIOHIDOptionsTypeNone);
        if (slot->elems_owner) CFRelease(slot->elems_owner);
        CFRelease(slot->dev);
        free(slot);
    });
    slot->accept_after = mach_absolute_time();
    g_nano_slots[free_idx] = slot;
    IOHIDDeviceActivate(dev);
    nano_seed_slot(slot);
}

static void nano_worker_poll(void) {
    os_unfair_lock_lock(&g_nano_lock);
    bool want = g_nano_want_active;
    os_unfair_lock_unlock(&g_nano_lock);

    if (!want) {
        for (int i = 0; i < 4; i++) nano_cancel_slot_idx(i);
        return;
    }

    io_iterator_t iter = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOHIDDevice"), &iter) != KERN_SUCCESS) {
        return;
    }

    io_service_t matched_svcs[4] = {0};
    uint64_t     matched_regs[4] = {0};
    int          matched_pids[4] = {0};
    int          matched_cnt = 0;

    io_service_t svc = 0;
    while ((svc = IOIteratorNext(iter)) != 0) {
        CFTypeRef cfVid   = IORegistryEntryCreateCFProperty(svc, CFSTR("VendorID"), kCFAllocatorDefault, 0);
        CFTypeRef cfPid   = IORegistryEntryCreateCFProperty(svc, CFSTR("ProductID"), kCFAllocatorDefault, 0);
        CFTypeRef cfUP    = IORegistryEntryCreateCFProperty(svc, CFSTR("PrimaryUsagePage"), kCFAllocatorDefault, 0);
        CFTypeRef cfU     = IORegistryEntryCreateCFProperty(svc, CFSTR("PrimaryUsage"), kCFAllocatorDefault, 0);
        CFTypeRef cfTrans = IORegistryEntryCreateCFProperty(svc, CFSTR("Transport"), kCFAllocatorDefault, 0);

        int vid = nano_cfnum_int(cfVid, -1);
        int pid = nano_cfnum_int(cfPid, -1);
        int up  = nano_cfnum_int(cfUP, -1);
        int u   = nano_cfnum_int(cfU, -1);
        bool is_usb_nano = (pid == 0x0B4E && cfTrans && CFGetTypeID(cfTrans) == CFStringGetTypeID() &&
                            CFStringCompare((CFStringRef)cfTrans, CFSTR("USB"), 0) == kCFCompareEqualTo);
        bool is_ble_nano = (pid == 0x0B4F && cfTrans && CFGetTypeID(cfTrans) == CFStringGetTypeID() &&
                            CFStringCompare((CFStringRef)cfTrans, CFSTR("Bluetooth Low Energy"), 0) == kCFCompareEqualTo);

        if (cfVid) CFRelease(cfVid);
        if (cfPid) CFRelease(cfPid);
        if (cfUP)  CFRelease(cfUP);
        if (cfU)   CFRelease(cfU);
        if (cfTrans) CFRelease(cfTrans);

        uint64_t reg_id = 0;
        if (vid == 0x045E && up == 1 && u == 5 && (is_usb_nano || is_ble_nano) &&
            IORegistryEntryGetRegistryEntryID(svc, &reg_id) == KERN_SUCCESS && matched_cnt < 4) {
            matched_svcs[matched_cnt] = svc; // keep retained for nano_open_service
            matched_regs[matched_cnt] = reg_id;
            matched_pids[matched_cnt] = pid;
            matched_cnt++;
        } else {
            IOObjectRelease(svc);
        }
    }
    IOObjectRelease(iter);

    // Cancel any open slot that disappeared from IORegistry
    for (int i = 0; i < 4; i++) {
        if (!g_nano_slots[i]) continue;
        bool still_here = false;
        for (int m = 0; m < matched_cnt; m++) {
            if (g_nano_slots[i]->registry_id == matched_regs[m]) { still_here = true; break; }
        }
        if (!still_here) nano_cancel_slot_idx(i);
    }

    if (matched_cnt == 0) {
        nano_publish_locked_state(NANO_STATUS_ABSENT);
        return;
    }

    IOHIDAccessType access = IOHIDCheckAccess(kIOHIDRequestTypeListenEvent);
    if (access != kIOHIDAccessTypeGranted) {
        for (int i = 0; i < 4; i++) nano_cancel_slot_idx(i);
        for (int m = 0; m < matched_cnt; m++) IOObjectRelease(matched_svcs[m]);
        nano_publish_locked_state(NANO_STATUS_NEEDS_PERMISSION);
        return;
    }

    for (int m = 0; m < matched_cnt; m++) {
        nano_open_service(matched_svcs[m], matched_regs[m], matched_pids[m]);
        IOObjectRelease(matched_svcs[m]);
    }
    nano_publish_locked_state(NANO_STATUS_AWAITING_NEUTRAL);
}

static void nano_set_active(bool active) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        g_nano_queue = dispatch_queue_create("com.challenge96kb.nanohid", DISPATCH_QUEUE_SERIAL);
        g_nano_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, g_nano_queue);
        dispatch_source_set_timer(g_nano_timer, dispatch_time(DISPATCH_TIME_NOW, 0), 500 * NSEC_PER_MSEC, 50 * NSEC_PER_MSEC);
        dispatch_source_set_event_handler(g_nano_timer, ^{ nano_worker_poll(); });
        dispatch_resume(g_nano_timer);
    });

    bool changed = false;
    os_unfair_lock_lock(&g_nano_lock);
    if (g_nano_want_active != active) {
        g_nano_want_active = active;
        changed = true;
        if (!active) {
            memset(&g_nano_pad, 0, sizeof(g_nano_pad));
            g_nano_status = NANO_STATUS_INACTIVE;
            g_nano_transports = 0;
        }
    }
    os_unfair_lock_unlock(&g_nano_lock);

    if (changed) {
        dispatch_async(g_nano_queue, ^{ nano_worker_poll(); });
    }
}

static GamepadState poll_apple_gamepad(uint32_t *prev_held, NanoHIDStatus *out_nano_status, int *out_nano_trans) {
    GamepadState gs = {0};
    for (GCController *c in [GCController controllers]) {
        GCExtendedGamepad *gp = c.extendedGamepad;
        if (gp) {
            gs.connected = true;
            float lx = eng_deadzone(gp.leftThumbstick.xAxis.value, 0.14f);
            float ly = eng_deadzone(gp.leftThumbstick.yAxis.value, 0.14f);
            float rx = eng_deadzone(gp.rightThumbstick.xAxis.value, 0.14f);
            float ry = eng_deadzone(gp.rightThumbstick.yAxis.value, 0.14f);
            if (fabsf(lx) > fabsf(gs.lx)) gs.lx = lx;
            if (fabsf(ly) > fabsf(gs.ly)) gs.ly = ly;
            if (fabsf(rx) > fabsf(gs.rx)) gs.rx = rx;
            if (fabsf(ry) > fabsf(gs.ry)) gs.ry = ry;
            if (gp.leftTrigger.value > gs.lt)  gs.lt = gp.leftTrigger.value;
            if (gp.rightTrigger.value > gs.rt) gs.rt = gp.rightTrigger.value;
            uint32_t h = 0;
            if (gp.buttonA.pressed) h |= GP_BTN_A;
            if (gp.buttonB.pressed) h |= GP_BTN_B;
            if (gp.buttonX.pressed) h |= GP_BTN_X;
            if (gp.buttonY.pressed) h |= GP_BTN_Y;
            if (gp.leftShoulder.pressed)  h |= GP_BTN_LB;
            if (gp.rightShoulder.pressed) h |= GP_BTN_RB;
            if (gp.buttonOptions && gp.buttonOptions.pressed) h |= GP_BTN_VIEW;
            if (gp.buttonMenu.pressed)    h |= GP_BTN_MENU;
            if (gp.leftThumbstickButton && gp.leftThumbstickButton.pressed)   h |= GP_BTN_L3;
            if (gp.rightThumbstickButton && gp.rightThumbstickButton.pressed) h |= GP_BTN_R3;
            if (gp.dpad.up.pressed)    h |= GP_BTN_DPAD_U;
            if (gp.dpad.down.pressed)  h |= GP_BTN_DPAD_D;
            if (gp.dpad.left.pressed)  h |= GP_BTN_DPAD_L;
            if (gp.dpad.right.pressed) h |= GP_BTN_DPAD_R;
            gs.held |= h;
        } else if (c.microGamepad) {
            GCMicroGamepad *mp = c.microGamepad;
            gs.connected = true;
            gs.lx = eng_deadzone(mp.dpad.xAxis.value, 0.14f);
            gs.ly = eng_deadzone(mp.dpad.yAxis.value, 0.14f);
            if (mp.buttonA.pressed) gs.held |= GP_BTN_A;
            if (mp.buttonX.pressed) gs.held |= GP_BTN_X;
            if (mp.buttonMenu.pressed) gs.held |= GP_BTN_MENU;
        }
    }

    NanoHIDStatus nst = NANO_STATUS_INACTIVE;
    int ntr = 0;
    os_unfair_lock_lock(&g_nano_lock);
    GamepadState nano = g_nano_pad;
    nst = g_nano_status;
    ntr = g_nano_transports;
    os_unfair_lock_unlock(&g_nano_lock);

    if (!gs.connected && nano.connected) {
        gs = nano;
    }
    if (out_nano_status) *out_nano_status = gs.connected && nst != NANO_STATUS_READY ? NANO_STATUS_INACTIVE : nst;
    if (out_nano_trans)  *out_nano_trans = ntr;

    gs.pressed = gs.held & ~(*prev_held);
    *prev_held = gs.held;
    return gs;
}

static void save_mtl_texture_png(id<MTLTexture> tex, NSString *path) {
    uint32_t w = (uint32_t)tex.width, h = (uint32_t)tex.height;
    uint32_t row_bytes = w * 4;
    uint8_t *bgra = (uint8_t *)malloc(row_bytes * h);
    [tex getBytes:bgra bytesPerRow:row_bytes fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
    for (uint32_t i = 0; i < w * h; i++) bgra[i * 4 + 3] = 255;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(bgra, w, h, 8, row_bytes, cs,
                                             kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    NSURL *url = [NSURL fileURLWithPath:path];
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL((__bridge CFURLRef)url,
                                                                (__bridge CFStringRef)UTTypePNG.identifier, 1, NULL);
    if (dst && img) {
        CGImageDestinationAddImage(dst, img, NULL);
        CGImageDestinationFinalize(dst);
    }
    if (dst) CFRelease(dst);
    if (img) CGImageRelease(img);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    free(bgra);
}

static int run_headless_captures(NSString *out_dir) {
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    if (!dev) { fprintf(stderr, "No Metal device\n"); return 1; }

    [[NSFileManager defaultManager] createDirectoryAtPath:out_dir withIntermediateDirectories:YES attributes:nil error:nil];

    const AppDesc *desc = app_get_desc();
    GameState *g = (GameState *)calloc(1, app_state_size());
    WorldGeometry geo = {0};
    RenderPacket *pkt = (RenderPacket *)calloc(1, sizeof(RenderPacket));

    app_init(g, true);
    app_build_world(g, &geo);
    printf("Built Procedural World: %u vertices, %u indices (%u tris), %u grass instances\n",
           geo.vert_count, geo.index_count, geo.index_count / 3, geo.grass_count);

    MetalRHI *rhi = rhi_metal_create(dev, &geo, MTLPixelFormatBGRA8Unorm);
    if (!rhi) return 1;

    uint32_t w = 1600, h = 960;
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    id<MTLTexture> off_tex = [dev newTextureWithDescriptor:td];

    EngInput zero_in = {0};
    zero_in.look_axes.z = 1.0f;

    for (int s = 0; s < 5; s++) {
        app_init(g, true);
        app_resize(g, (float)w, (float)h, 0, 0, 0, 0);
        app_set_preset_shot(g, s);

        int steps = desc->shot_steps[s] > 0 ? desc->shot_steps[s] : 6;
        for (int k = 0; k < steps; k++) {
            app_update_and_build_packet(g, 1.0f / 60.0f, &zero_in, pkt);
        }
        rhi_metal_render(rhi, pkt, off_tex, nil);

        NSString *fname = [NSString stringWithUTF8String:desc->shot_names[s]];
        NSString *full = [out_dir stringByAppendingPathComponent:fname];
        save_mtl_texture_png(off_tex, full);
        printf("Saved %s\n", full.UTF8String);
    }

    rhi_metal_destroy(rhi);
    world_free_geo(&geo);
    free(pkt);
    free(g);
    return 0;
}

static int run_headless_card_banner(NSString *out_path, int shot_id) {
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    if (!dev) return 1;
    [[NSFileManager defaultManager] createDirectoryAtPath:[out_path stringByDeletingLastPathComponent]
                              withIntermediateDirectories:YES attributes:nil error:nil];
    const AppDesc *desc = app_get_desc();
    GameState *g = (GameState *)calloc(1, app_state_size());
    WorldGeometry geo = {0};
    RenderPacket *pkt = (RenderPacket *)calloc(1, sizeof(RenderPacket));
    app_init(g, true);
    app_build_world(g, &geo);
    MetalRHI *rhi = rhi_metal_create(dev, &geo, MTLPixelFormatBGRA8Unorm);
    if (!rhi) return 1;

    uint32_t w = 1800, h = 1200; // Exact Apple App Clip Card Header dimensions (3:2 RGB, no HUD!)
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    id<MTLTexture> off_tex = [dev newTextureWithDescriptor:td];

    EngInput zero_in = {0};
    zero_in.look_axes.z = 1.0f;
    app_init(g, true);
    app_resize(g, (float)w, (float)h, 0, 0, 0, 0);
    app_set_preset_shot(g, shot_id);
    int steps = desc->shot_steps[shot_id] > 0 ? desc->shot_steps[shot_id] : 6;
    for (int k = 0; k < steps; k++) {
        app_update_and_build_packet(g, 1.0f / 60.0f, &zero_in, pkt);
    }
    pkt->ui_prim_count = 0; // Strip all HUD text/buttons per Apple App Clip Card HIG!
    rhi_metal_render(rhi, pkt, off_tex, nil);
    save_mtl_texture_png(off_tex, out_path);
    printf("Saved 1800x1200 App Clip Card Banner: %s\n", out_path.UTF8String);

    rhi_metal_destroy(rhi);
    world_free_geo(&geo);
    free(pkt);
    free(g);
    return 0;
}

@interface GameMetalView : NSView {
@public
    CAMetalLayer *metalLayer;
    id<MTLDevice> dev;
    MetalRHI *rhi;
    GameState *game;
    RenderPacket *pkt;
    WorldGeometry geo;
    NSTimer *timer;
    double lastTime;
    NSPoint dragOrigin;
    NSPoint dragPrev;
    bool didDrag;
    bool keysDown[256];
    bool arrowsDown[4]; // 0=Left, 1=Right, 2=Down, 3=Up
    bool shiftHeld;
    uint32_t prevPadHeld;
    NanoHIDStatus lastNanoStatus;
    int lastNanoTrans;
    NSTitlebarAccessoryViewController *nanoTitlebarVC;
    AVAudioEngine *audioEngine;
    AVAudioSourceNode *sourceNode;
}
- (void)clearHeldKeys;
- (void)attachNanoTitlebarBannerToWindow:(NSWindow *)win;
- (void)requestNanoControllerAccess:(id)sender;
- (void)relaunchAppForPermission:(id)sender;
@end

@implementation GameMetalView
- (BOOL)wantsUpdateLayer { return YES; }
- (CALayer *)makeBackingLayer {
    metalLayer = [CAMetalLayer layer];
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    return metalLayer;
}
- (BOOL)acceptsFirstResponder { return YES; }

- (void)attachNanoTitlebarBannerToWindow:(NSWindow *)win {
    NSView *bar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 1360, 34)];
    NSTextField *lbl = [NSTextField labelWithString:@"Xbox Nano Detected — Enable macOS Input Monitoring so Clover Cove can read its HID Gamepad controls:"];
    lbl.font = [NSFont systemFontOfSize:12.5 weight:NSFontWeightMedium];
    lbl.frame = NSMakeRect(14, 7, 630, 18);
    [bar addSubview:lbl];

    NSButton *btnEnable = [NSButton buttonWithTitle:@"Enable Input…" target:self action:@selector(requestNanoControllerAccess:)];
    btnEnable.bezelStyle = NSBezelStyleRounded;
    btnEnable.frame = NSMakeRect(652, 3, 118, 28);
    [bar addSubview:btnEnable];

    NSButton *btnRelaunch = [NSButton buttonWithTitle:@"Relaunch App" target:self action:@selector(relaunchAppForPermission:)];
    btnRelaunch.bezelStyle = NSBezelStyleRounded;
    btnRelaunch.frame = NSMakeRect(776, 3, 108, 28);
    [bar addSubview:btnRelaunch];

    nanoTitlebarVC = [NSTitlebarAccessoryViewController new];
    nanoTitlebarVC.view = bar;
    nanoTitlebarVC.layoutAttribute = NSLayoutAttributeBottom;
    nanoTitlebarVC.hidden = YES;
    [win addTitlebarAccessoryViewController:nanoTitlebarVC];
}

- (void)requestNanoControllerAccess:(id)sender {
    (void)sender;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        IOHIDRequestAccess(kIOHIDRequestTypeListenEvent);
        dispatch_async(dispatch_get_main_queue(), ^{
            NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_ListenEvent"];
            if (url) [[NSWorkspace sharedWorkspace] openURL:url];
        });
    });
}

- (void)relaunchAppForPermission:(id)sender {
    (void)sender;
    NSURL *bundleURL = [[NSBundle mainBundle] bundleURL];
    if (bundleURL) {
        NSWorkspaceOpenConfiguration *cfg = [NSWorkspaceOpenConfiguration configuration];
        cfg.createsNewApplicationInstance = YES;
        [[NSWorkspace sharedWorkspace] openApplicationAtURL:bundleURL configuration:cfg completionHandler:^(NSRunningApplication *app, NSError *err) {
            (void)app; (void)err;
            dispatch_async(dispatch_get_main_queue(), ^{
                [NSApp terminate:nil];
            });
        }];
    }
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.wantsLayer = YES;
        dev = MTLCreateSystemDefaultDevice();
        metalLayer = (CAMetalLayer *)self.layer;
        metalLayer.device = dev;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        lastNanoStatus = (NanoHIDStatus)-1;

        game = (GameState *)calloc(1, app_state_size());
        pkt  = (RenderPacket *)calloc(1, sizeof(RenderPacket));
        audio_init();
        app_init(game, true);
        app_build_world(game, &geo);
        rhi = rhi_metal_create(dev, &geo, MTLPixelFormatBGRA8Unorm);

        audioEngine = [AVAudioEngine new];
        AVAudioFormat *fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:48000.0 channels:2];
        sourceNode = [[AVAudioSourceNode alloc] initWithFormat:fmt renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *ts, AVAudioFrameCount fc, AudioBufferList *abl) {
            (void)isSilence; (void)ts;
            static float interleaved[4096 * 2];
            uint32_t n = fc > 4096 ? 4096 : fc;
            audio_render_frames(interleaved, (int)n, 48000);
            float *outL = (float *)abl->mBuffers[0].mData;
            float *outR = (float *)abl->mBuffers[1].mData;
            for (uint32_t i = 0; i < n; i++) {
                outL[i] = interleaved[i * 2 + 0];
                outR[i] = interleaved[i * 2 + 1];
            }
            return noErr;
        }];
        [audioEngine attachNode:sourceNode];
        [audioEngine connect:sourceNode to:audioEngine.mainMixerNode format:fmt];
        [audioEngine startAndReturnError:nil];

        lastTime = CACurrentMediaTime();
        timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0 target:self selector:@selector(tick) userInfo:nil repeats:YES];
    }
    return self;
}

- (void)clearHeldKeys {
    memset(keysDown, 0, sizeof(keysDown));
    memset(arrowsDown, 0, sizeof(arrowsDown));
    shiftHeld = false;
    prevPadHeld = 0;
    nano_set_active(false);
}

- (EngInput)buildInput {
    EngInput in = {0};
    float mx = (keysDown['d'] ? 1.0f : 0.0f) - (keysDown['a'] ? 1.0f : 0.0f);
    float mz = (keysDown['w'] ? 1.0f : 0.0f) - (keysDown['s'] ? 1.0f : 0.0f);
    float my = ((keysDown['e'] || keysDown['u']) ? 1.0f : 0.0f) - ((keysDown['q'] || keysDown['o']) ? 1.0f : 0.0f);
    float lx = ((keysDown['l'] || arrowsDown[1]) ? 1.0f : 0.0f) - ((keysDown['j'] || arrowsDown[0]) ? 1.0f : 0.0f);
    float ly = ((keysDown['i'] || arrowsDown[3]) ? 1.0f : 0.0f) - ((keysDown['k'] || arrowsDown[2]) ? 1.0f : 0.0f);
    in.move_axes = v3mk(mx, my, mz);
    in.look_axes = v3mk(lx, ly, shiftHeld ? 2.2f : 1.0f);

    BOOL isFg = self.window.isKeyWindow && [NSApp isActive];
    nano_set_active(isFg);
    if (isFg) {
        NanoHIDStatus nst = NANO_STATUS_INACTIVE;
        int ntr = 0;
        in.pad = poll_apple_gamepad(&prevPadHeld, &nst, &ntr);
        if (nst != lastNanoStatus || ntr != lastNanoTrans) {
            lastNanoStatus = nst;
            lastNanoTrans = ntr;
            if (nanoTitlebarVC) {
                nanoTitlebarVC.hidden = (nst != NANO_STATUS_NEEDS_PERMISSION);
            }
            const char *baseTitle = app_get_desc()->window_title;
            NSString *sub = @"";
            if (nst == NANO_STATUS_READY) {
                const char *tr = (ntr == 3) ? "USB+BLE" : (ntr == 1 ? "USB" : "BLE");
                sub = [NSString stringWithFormat:@"  [Xbox Nano %s Ready]", tr];
            } else if (nst == NANO_STATUS_AWAITING_NEUTRAL) {
                sub = @"  [Xbox Nano: Release Sticks to Arm]";
            } else if (nst == NANO_STATUS_NEEDS_PERMISSION) {
                sub = @"  [Xbox Nano: Enable Input Monitoring]";
            }
            self.window.title = [NSString stringWithFormat:@"%s%@", baseTitle, sub];
        }
    } else {
        prevPadHeld = 0;
    }
    return in;
}

- (void)tick {
    CGFloat scale = self.window.backingScaleFactor ?: 2.0;
    CGSize sz = self.bounds.size;
    CGSize drawSz = CGSizeMake(sz.width * scale, sz.height * scale);
    if (drawSz.width < 64 || drawSz.height < 64) return;
    metalLayer.drawableSize = drawSz;
    app_resize(game, (float)drawSz.width, (float)drawSz.height, 0, 0, 0, 0);

    double now = CACurrentMediaTime();
    float dt = (float)(now - lastTime);
    lastTime = now;

    id<CAMetalDrawable> dr = [metalLayer nextDrawable];
    if (!dr) return;
    EngInput in = [self buildInput];
    app_update_and_build_packet(game, dt, &in, pkt);
    rhi_metal_render(rhi, pkt, dr.texture, dr);
}

- (NSPoint)pxPoint:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    CGFloat sc = self.window.backingScaleFactor ?: 2.0;
    return NSMakePoint(p.x * sc, (self.bounds.size.height - p.y) * sc);
}

- (void)mouseMoved:(NSEvent *)e {
    NSPoint p = [self pxPoint:e];
    app_on_hover(game, (float)p.x, (float)p.y);
}
- (void)mouseDown:(NSEvent *)e {
    dragOrigin = dragPrev = [self pxPoint:e];
    didDrag = false;
}
- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self pxPoint:e];
    float dx = (float)(p.x - dragPrev.x), dy = (float)(p.y - dragPrev.y);
    if (hypotf((float)(p.x - dragOrigin.x), (float)(p.y - dragOrigin.y)) > 6.0f) didDrag = true;
    int fingers = (e.modifierFlags & NSEventModifierFlagOption) ? 2 : 1;
    app_on_drag(game, dx * 0.5f, dy * 0.5f, fingers, (float)dragOrigin.x, (float)dragOrigin.y);
    dragPrev = p;
}
- (void)rightMouseDown:(NSEvent *)e {
    dragOrigin = [self pxPoint:e];
}
- (void)rightMouseDragged:(NSEvent *)e {
    CGFloat sc = self.window.backingScaleFactor ?: 2.0;
    app_on_drag(game, (float)(e.deltaX * sc * 0.5f), (float)(e.deltaY * sc * 0.5f), 2, (float)dragOrigin.x, (float)dragOrigin.y);
}
- (void)rightMouseUp:(NSEvent *)e {
    (void)e;
    app_on_drag_end(game);
}
- (void)mouseUp:(NSEvent *)e {
    app_on_drag_end(game);
    if (!didDrag) {
        NSPoint p = [self pxPoint:e];
        app_on_tap(game, (float)p.x, (float)p.y);
    }
}
- (void)scrollWheel:(NSEvent *)e {
    app_on_pinch(game, (float)e.scrollingDeltaY * 0.015f);
}
- (void)magnifyWithEvent:(NSEvent *)e {
    app_on_pinch(game, (float)e.magnification * 2.5f);
}
- (void)flagsChanged:(NSEvent *)e {
    shiftHeld = (e.modifierFlags & NSEventModifierFlagShift) != 0;
}
- (void)keyDown:(NSEvent *)e {
    if (e.modifierFlags & NSEventModifierFlagCommand) { [super keyDown:e]; return; }
    uint16_t kc = e.keyCode;
    if (kc >= 123 && kc <= 126) { arrowsDown[kc - 123] = true; return; }
    NSString *ch = e.charactersIgnoringModifiers.lowercaseString;
    if (ch.length > 0) {
        unichar c = [ch characterAtIndex:0];
        if (c < 256) keysDown[c] = true;
        if (!e.isARepeat && c < 128) {
            app_on_key_press(game, (int)c);
        }
    }
}
- (void)keyUp:(NSEvent *)e {
    uint16_t kc = e.keyCode;
    if (kc >= 123 && kc <= 126) { arrowsDown[kc - 123] = false; return; }
    NSString *ch = e.charactersIgnoringModifiers.lowercaseString;
    if (ch.length > 0) {
        unichar c = [ch characterAtIndex:0];
        if (c < 256) keysDown[c] = false;
    }
}
@end

@interface MacAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (weak, nonatomic) GameMetalView *metalView;
@end
@implementation MacAppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}
- (void)windowDidResignKey:(NSNotification *)notification {
    (void)notification;
    [self.metalView clearHeldKeys];
}
@end

static void setup_macos_menu(void) {
    NSMenu *bar = [NSMenu new];
    NSMenuItem *appItem = [NSMenuItem new];
    [bar addItem:appItem];
    NSMenu *appMenu = [NSMenu new];
    [appMenu addItemWithTitle:@"Enable Xbox Nano Input…" action:@selector(requestNanoControllerAccess:) keyEquivalent:@""];
    [appMenu addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
    [appItem setSubmenu:appMenu];
    [NSApp setMainMenu:bar];
}

int main(int argc, const char **argv) {
    @autoreleasepool {
        if (argc >= 3 && strcmp(argv[1], "--capture") == 0) {
            return run_headless_captures([NSString stringWithUTF8String:argv[2]]);
        }
        if (argc >= 4 && strcmp(argv[1], "--capture-card") == 0) {
            return run_headless_card_banner([NSString stringWithUTF8String:argv[2]], atoi(argv[3]));
        }
        const AppDesc *desc = app_get_desc();
        [NSApplication sharedApplication];
        MacAppDelegate *del = [MacAppDelegate new];
        [NSApp setDelegate:del];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        setup_macos_menu();

        NSRect rect = NSMakeRect(0, 0, 1360, 820);
        NSWindow *win = [[NSWindow alloc] initWithContentRect:rect
                                                    styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                      backing:NSBackingStoreBuffered defer:NO];
        win.title = [NSString stringWithUTF8String:desc->window_title];
        win.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
        win.acceptsMouseMovedEvents = YES;
        win.delegate = del;
        GameMetalView *view = [[GameMetalView alloc] initWithFrame:rect];
        del.metalView = view;
        win.contentView = view;
        [view attachNanoTitlebarBannerToWindow:win];
        [win center];
        [win makeKeyAndOrderFront:nil];
        [win makeFirstResponder:view];
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }
    return 0;
}
