// CYD-Simpsons: cartoon television enclosure for the ESP32-2432S028R (2.8" CYD)
//
// Coordinate system (assembly): X = width (viewer's left to right), Y = depth
// (front face at y=0, rear at y=D), Z = height (table at z=0, legs below it).
//
// Every printable part is a module in its own local coordinates; assembly()
// places them. export(name) re-orients each part for printing. Render one
// part with:  openscad -D 'part="front_shell"' -o front_shell.stl simpsons_tv.scad
//
// All dimensions in mm.

part = "assembly";          // see export() for names
show_components = true;     // ghost the electronics in the assembly view
explode = 0;                // assembly explode distance (0 = assembled)

$fa = 2; $fs = 0.4;
EPS = 0.01;

/* ---------- Colors (print colors, see README) ---------- */
C_FRONT  = "#A98BDA";   // lavender: front shell, rear shell
C_DARK   = "#2E2145";   // dark purple: screen surround
C_LEG    = "#4B2C8F";   // purple: legs, set-top box
C_TEAL   = "#3FB0A0";   // teal: knobs, grill
C_BLACK  = "#1E1E1E";   // antenna
C_PCB    = "#2C6B3F";
C_METAL  = "#B0B0B0";
C_GHOST  = [0.5, 0.5, 0.5, 0.35];

/* ---------- Overall envelope ---------- */
W = 118;            // width
H = 90;             // height (body only, legs extra)
D = 65;             // depth
wall = 2.5;         // side/roof/floor wall thickness
plate_t = 3;        // front plate thickness
split_y = 22;       // front shell / rear shell joint
edge_r = 3;         // radius on the front-to-back edges
fit = 0.2;          // clearance per side for mating parts

/* ---------- Shell joint ---------- */
lip_len = 6;        // lip on the front shell that enters the rear shell
lip_t = 1.2;
tab_w = 10; tab_t = 4; tab_len = 9;
tab_x = [35, 75];               // screw positions, hidden under the set-top box
screw_y = split_y + 5;
pilot_m3 = 2.5; clear_m3 = 3.4; csk_d = 6.4;   // all hardware is M3

/* ---------- Screen surround (dark frame) ---------- */
sur_w = 78; sur_h = 64; sur_r = 6;
sur_x0 = 8; sur_z0 = 14;
sur_face_t = 1.2;               // sits in a recess of the same depth
plug_w = 70; plug_h = 56; plug_r = 5;
sur_depth = 5;                  // total depth of the surround
win_w = 61; win_h = 47; win_r = 7;
win_chamfer = 1.2;
sur_cx = sur_x0 + sur_w/2;      // 47
sur_cz = sur_z0 + sur_h/2;      // 46

/* ---------- CYD (ESP32-2432S028R) ----------
   Mounted with the USB ports on the LEFT and the microSD slot on the TOP edge
   (board rotated 180 degrees from the usual drawing). Firmware uses rotation 3.
   Front of the PCB faces the screen. */
cyd_w = 86.1; cyd_h = 49.9; cyd_t = 1.6;
cyd_hole_d = 3.2; cyd_hole_in_x = 3.83; cyd_hole_in_z = 3.98;
cyd_screen_cx = 43.0;           // visible area centre from the board's left edge
cyd_glass_w = 69; cyd_glass_h = 50;
cyd_glass = 4.5;                // glass top above PCB front face (MEASURE AND ADJUST)
glass_gap = 0.5;                // gap between surround plug and glass
cyd_x0 = sur_cx - cyd_screen_cx;
cyd_z0 = sur_cz - cyd_h/2;
cyd_y  = sur_depth + glass_gap + cyd_glass;   // PCB front face
standoff_d = 6;
standoff_h = cyd_y - plate_t;
cyd_holes = [for (x=[cyd_hole_in_x, cyd_w-cyd_hole_in_x], z=[cyd_hole_in_z, cyd_h-cyd_hole_in_z]) [cyd_x0+x, cyd_z0+z]];
sd_cx = cyd_x0 + 50.6;          // microSD centre (board flipped)
hatch_w = 30; hatch_y0 = 6; hatch_y1 = 19;

/* ---------- Knobs (buttons) ---------- */
knob_x = 102;
knob_z = [63, 45];
knob_d = 14; knob_t = 3; knob_gap = 0.5;
knob_chamfer = 0.8;
groove_w = 2; groove_d = 0.8;
stem_d = 5; stem_hole_d = 5.6;
stem_end_y = 5.4;               // stem tip (assembly y) rests on the switch plunger
keeper_od = 9; keeper_id = 4.8; keeper_t = 2;
sw_h = 5.0;                     // 6x6 tact switch, base to plunger tip
sw_body_h = 3.5; sw_plunger_d = 3.5;
sw_pitch_x = 6.5; sw_pitch_z = 4.5; sw_pin_d = 1.2;
sw_pretravel = 0.1;
carrier_y = stem_end_y + sw_pretravel + sw_h;   // carrier front face
carrier_t = 2;
carrier_x0 = 92; carrier_x1 = 113; carrier_z0 = 35; carrier_z1 = 73;
carrier_boss = [[95.5, 54], [109.5, 54]];

/* ---------- Grill (decorative) ---------- */
grill_x0 = 95; grill_z0 = 16; grill_w = 14; grill_h = 22;
grill_t = 2; grill_recess = 1.2; grill_slots = 4; grill_slot_w = 1.6; grill_slot_d = 0.8;

/* ---------- Legs ---------- */
leg_top = 9; leg_bot = 6; leg_h = 14; leg_splay = 3;
peg = 5; peg_h = 5;
leg_pos = [[14, 10], [W-14, 10], [14, D-10], [W-14, D-10]];

/* ---------- Set-top box ---------- */
box_w = 64; box_d = 30; box_h = 10; box_r = 1.5;
box_x0 = 24; box_y0 = 2;
box_peg_d = 4.8; box_peg_h = 5;
box_pegs = [[10, 8], [54, 8]];  // local xy
ant_peg_d = 4; ant_hole_d = 4.3; ant_peg_h = 5;

/* ---------- Antenna ---------- */
rod_d = 3; rod_len = 42; rod_angle = 28; ball_d = 5; hub_d = 8; hub_h = 4;

/* ---------- Speaker (40 mm, rear firing) ---------- */
spk_d = 40.5; spk_flange_t = 2.7; spk_depth = 18; spk_magnet_d = 22;
spk_cx = 45; spk_cz = 45;
spk_boss_r = 23.6; spk_boss_d = 6;
spk_boss_angles = [90, 210, 330];
spk_notch_angle = 150;          // where the solder tabs sit (between bosses)
clamp_od = 48; clamp_id = 36; clamp_t = 2;
vent_d = 34; vent_slot = 2; vent_pitch = 3.5;

/* ---------- Board pockets (screwless slide-in holders on the back panel) ----------
   Each pocket is a block with a slot; the board slides in from the front with
   its short edge toward the back panel. Boards lie flat (thickness along Z). */
pocket_depth = 8; pocket_wall_h = 3.2;   // slot depth into the pocket, material above/below the slot

/* Amplifier (Adafruit PAM8302): 24.13 x 15.24 x 1.6. Its own holes are only
   2.0 mm, so it is held in a pocket instead of screwed. */
amp_l = 24.13; amp_w = 15.24; amp_t = 1.6;
amp_x = 104; amp_z = 40;        // board centre / mid-plane

/* USB-C breakout (13 x 22, power only), connector faces the rear */
usb_pcb_w = 13; usb_pcb_l = 22; usb_pcb_t = 1.6;
usb_conn_w = 9; usb_conn_h = 3.2; usb_conn_l = 7.5;
usb_x = 104; usb_z = 17;        // board centre / mid-plane
usb_open_w = 9.6; usb_open_h = 4.2;

/* ================================================================== */
/* Helpers                                                             */
/* ================================================================== */
module rrect(w, h, r) {          // 2D rounded rectangle, corner at origin
    translate([r, r]) offset(r=r) square([w-2*r, h-2*r]);
}
// Box along +Y with a rounded XZ cross-section. Corner at origin.
module rbox_y(w, d, h, r) {
    translate([0, d, 0]) rotate([90, 0, 0]) linear_extrude(d) rrect(w, h, r);
}
// Rounded-rect prism along +Y from y0 to y1, centred at (cx, cz).
module rprism_y(cx, cz, w, h, r, y0, y1) {
    translate([cx - w/2, y1, cz - h/2]) rotate([90, 0, 0]) linear_extrude(y1 - y0) rrect(w, h, r);
}
module cyl_y(d, y0, y1) {         // cylinder along +Y
    translate([0, y0, 0]) rotate([-90, 0, 0]) cylinder(d=d, h=y1 - y0);
}
module csk_hole_z(d, csk, t) {    // countersunk through-hole along Z, head at z=t
    translate([0, 0, -EPS]) cylinder(d=d, h=t + 2*EPS);
    translate([0, 0, t - (csk - d)/2]) cylinder(d1=d, d2=csk + 0.4, h=(csk - d)/2 + 0.2 + EPS);
}

/* ================================================================== */
/* Front shell                                                         */
/* ================================================================== */
module front_shell() {
    difference() {
        union() {
            // plate + walls
            difference() {
                rbox_y(W, split_y, H, edge_r);
                translate([wall, plate_t, wall]) cube([W - 2*wall, split_y, H - 2*wall]);
            }
            // lip into the rear shell
            translate([0, split_y - EPS, 0]) difference() {
                translate([wall + fit, 0, wall + fit]) cube([W - 2*(wall + fit), lip_len, H - 2*(wall + fit)]);
                translate([wall + fit + lip_t, -EPS, wall + fit + lip_t]) cube([W - 2*(wall + fit + lip_t), lip_len + 2*EPS, H - 2*(wall + fit + lip_t)]);
            }
            // screw tabs (roof and floor)
            for (x = tab_x) {
                translate([x - tab_w/2, split_y - 1, wall + fit]) cube([tab_w, tab_len + 1, tab_t]);
                translate([x - tab_w/2, split_y - 1, H - wall - fit - tab_t]) cube([tab_w, tab_len + 1, tab_t]);
            }
            // CYD standoffs
            for (p = cyd_holes) translate([p[0], plate_t - EPS, p[1]]) cyl_y(standoff_d, 0, standoff_h + EPS);
            // button carrier bosses
            for (p = carrier_boss) translate([p[0], plate_t - EPS, p[1]]) cyl_y(standoff_d, 0, carrier_y - plate_t + EPS);
        }
        // screen surround recess and through cut
        rprism_y(sur_cx, sur_cz, sur_w + 2*fit, sur_h + 2*fit, sur_r + fit, -1, sur_face_t);
        rprism_y(sur_cx, sur_cz, plug_w + 2*fit, plug_h + 2*fit, plug_r + fit, -1, plate_t + 1);
        // standoff pilot holes
        for (p = cyd_holes) translate([p[0], 0, p[1]]) cyl_y(pilot_m3, plate_t + 1, plate_t + standoff_h + 1);
        for (p = carrier_boss) translate([p[0], 0, p[1]]) cyl_y(pilot_m3, plate_t + 1, carrier_y + 1);
        // tab pilot holes
        for (x = tab_x) {
            translate([x, screw_y, -1]) cylinder(d=pilot_m3, h=wall + tab_t + 3);
            translate([x, screw_y, H - wall - tab_t - 2]) cylinder(d=pilot_m3, h=wall + tab_t + 3);
        }
        // knob stems
        for (z = knob_z) translate([knob_x, 0, z]) cyl_y(stem_hole_d, -1, plate_t + 1);
        // grill recess
        translate([grill_x0 - fit, -1, grill_z0 - fit]) cube([grill_w + 2*fit, grill_recess + 1, grill_h + 2*fit]);
        // microSD hatch in the roof
        translate([sd_cx - hatch_w/2, hatch_y0, H - wall - 1]) cube([hatch_w, hatch_y1 - hatch_y0, wall + 2]);
        // set-top box peg holes
        for (p = box_pegs) translate([box_x0 + p[0], box_y0 + p[1], H - wall - 1]) cylinder(d=box_peg_d + 2*fit, h=wall + 2);
        // leg peg holes (front pair)
        for (i = [0, 1]) translate([leg_pos[i][0] - (peg + 2*fit)/2, leg_pos[i][1] - (peg + 2*fit)/2, -1]) cube([peg + 2*fit, peg + 2*fit, wall + 2]);
    }
}

// Slide-in board pocket on the inside of the back panel (local to rear_shell)
module pocket_block(cx, cz, bw, bt, depth) {
    translate([cx - (bw + 2*fit + 2*pocket_wall_h)/2, depth - wall - pocket_depth, cz - (bt + 0.3)/2 - pocket_wall_h])
        cube([bw + 2*fit + 2*pocket_wall_h, pocket_depth + EPS, bt + 0.3 + 2*pocket_wall_h]);
}
module pocket_slot(cx, cz, bw, bt, depth) {
    translate([cx - (bw + 2*fit)/2, depth - wall - pocket_depth - 1, cz - (bt + 0.3)/2])
        cube([bw + 2*fit, pocket_depth + 1 + EPS, bt + 0.3]);
}

/* ================================================================== */
/* Rear shell                                                          */
/* ================================================================== */
module rear_shell() {
    depth = D - split_y;
    translate([0, split_y, 0]) difference() {
        union() {
            difference() {
                rbox_y(W, depth, H, edge_r);
                translate([wall, -1, wall]) cube([W - 2*wall, depth - wall + 1, H - 2*wall]);
            }
            // speaker bosses
            for (a = spk_boss_angles)
                translate([spk_cx + spk_boss_r*cos(a), depth - wall + EPS, spk_cz + spk_boss_r*sin(a)])
                    cyl_y(spk_boss_d, -(spk_flange_t - 0.3), 0);
            // board pockets (outer blocks)
            pocket_block(usb_x, usb_z, usb_pcb_w, usb_pcb_t, depth);
            pocket_block(amp_x, amp_z, amp_w, amp_t, depth);
        }
        // speaker vents
        intersection() {
            translate([spk_cx, depth - wall - 1, spk_cz]) cyl_y(vent_d, 0, wall + 2);
            for (z = [-vent_d/2 + vent_pitch/2 : vent_pitch : vent_d/2])
                translate([spk_cx - vent_d/2 - 1, depth - wall - 1, spk_cz + z - vent_slot/2]) cube([vent_d + 2, wall + 2, vent_slot]);
        }
        // speaker boss pilots
        for (a = spk_boss_angles)
            translate([spk_cx + spk_boss_r*cos(a), 0, spk_cz + spk_boss_r*sin(a)]) cyl_y(pilot_m3, depth - wall - spk_flange_t - 1, depth - 0.8);
        // board slots and the USB port opening
        pocket_slot(usb_x, usb_z, usb_pcb_w, usb_pcb_t, depth);
        pocket_slot(amp_x, amp_z, amp_w, amp_t, depth);
        translate([usb_x - usb_open_w/2, depth - wall - 1, usb_z + usb_pcb_t/2 + usb_conn_h/2 - usb_open_h/2])
            cube([usb_open_w, wall + 2, usb_open_h]);
        // shell screws (countersunk, roof and floor)
        for (x = tab_x) {
            translate([x, screw_y - split_y, 0]) csk_hole_z(clear_m3, csk_d, wall);
            translate([x, screw_y - split_y, H]) mirror([0, 0, 1]) csk_hole_z(clear_m3, csk_d, wall);
        }
        // leg peg holes (rear pair)
        for (i = [2, 3]) translate([leg_pos[i][0] - (peg + 2*fit)/2, leg_pos[i][1] - split_y - (peg + 2*fit)/2, -1]) cube([peg + 2*fit, peg + 2*fit, wall + 2]);
    }
}

/* ================================================================== */
/* Screen surround: local origin at front-face centre, extends +Y      */
/* ================================================================== */
module surround() {
    difference() {
        union() {
            rprism_y(0, 0, sur_w, sur_h, sur_r, 0, sur_face_t);
            rprism_y(0, 0, plug_w, plug_h, plug_r, sur_face_t - EPS, sur_depth);
        }
        // window with a chamfered front edge (fake CRT)
        hull() {
            rprism_y(0, 0, win_w + 2*win_chamfer, win_h + 2*win_chamfer, win_r + win_chamfer, -1, -1 + EPS);
            rprism_y(0, 0, win_w, win_h, win_r, win_chamfer, win_chamfer + EPS);
        }
        rprism_y(0, 0, win_w, win_h, win_r, win_chamfer - EPS, sur_depth + 1);
    }
}

/* ================================================================== */
/* Knob: local origin at the face centre, front face at y=0, +Y is in  */
/* ================================================================== */
module knob() {
    stem_len = knob_gap + stem_end_y;
    difference() {
        union() {
            cyl_y(knob_d - 2*knob_chamfer, 0, EPS);
            hull() {
                cyl_y(knob_d - 2*knob_chamfer, 0, EPS);
                cyl_y(knob_d, knob_chamfer, knob_chamfer + EPS);
            }
            cyl_y(knob_d, knob_chamfer, knob_t);
            cyl_y(stem_d, knob_t - EPS, knob_t + stem_len);
        }
        // indicator groove across the face
        translate([-knob_d/2 - 1, -1, -groove_w/2]) cube([knob_d + 2, groove_d + 1, groove_w]);
    }
}
module knob_keeper() {
    difference() {
        cyl_y(keeper_od, 0, keeper_t);
        cyl_y(keeper_id, -1, keeper_t + 1);
    }
}

/* ================================================================== */
/* Button carrier: local origin (carrier_x0, carrier_z0) at y=0,       */
/* switches mount on the -Y face, screws from +Y                        */
/* ================================================================== */
module button_carrier() {
    difference() {
        translate([carrier_x0, 0, carrier_z0]) cube([carrier_x1 - carrier_x0, carrier_t, carrier_z1 - carrier_z0]);
        for (z = knob_z, sx = [-1, 1], sz = [-1, 1])
            translate([knob_x + sx*sw_pitch_x/2, 0, z + sz*sw_pitch_z/2]) cyl_y(sw_pin_d, -1, carrier_t + 1);
        for (p = carrier_boss) translate([p[0], 0, p[1]]) cyl_y(clear_m3, -1, carrier_t + 1);
    }
}

/* ================================================================== */
/* Grill: local origin at front-left-bottom, front face at y=0         */
/* ================================================================== */
module grill() {
    pitch = grill_w / grill_slots;
    difference() {
        cube([grill_w, grill_t, grill_h]);
        for (i = [0 : grill_slots - 1])
            translate([pitch*(i + 0.5) - grill_slot_w/2, -1, 2]) cube([grill_slot_w, grill_slot_d + 1, grill_h - 4]);
    }
}

/* ================================================================== */
/* Leg: local origin at the floor contact point, peg goes +Z,          */
/* body hangs -Z, splays toward +X (mirror for the other side)         */
/* ================================================================== */
module leg() {
    translate([-peg/2, -peg/2, -EPS]) cube([peg, peg, peg_h + EPS]);
    hull() {
        translate([-leg_top/2, -leg_top/2, -EPS]) cube([leg_top, leg_top, EPS]);
        translate([leg_splay - leg_bot/2, -leg_bot/2, -leg_h]) cube([leg_bot, leg_bot, EPS]);
    }
}

/* ================================================================== */
/* Set-top box: local origin at bottom-front-left corner               */
/* ================================================================== */
module set_top_box() {
    difference() {
        linear_extrude(box_h) rrect(box_w, box_d, box_r);
        // "display" window on the front face
        translate([5, -1, 3]) cube([20, 1.8, 4]);
        // two dial dots
        for (x = [46, 55]) translate([x, -1, 5]) cyl_y(4, 0, 1.8);
        // antenna socket
        translate([box_w/2, box_d/2, box_h - ant_peg_h - 0.5]) cylinder(d=ant_hole_d, h=ant_peg_h + 1);
    }
    for (p = box_pegs) translate([p[0], p[1], -box_peg_h + EPS]) cylinder(d=box_peg_d, h=box_peg_h);
}

/* ================================================================== */
/* Antenna: local origin at hub base, V opens in the XZ plane          */
/* ================================================================== */
module antenna() {
    intersection() {
        union() {
            translate([0, 0, -ant_peg_h]) cylinder(d=ant_peg_d, h=ant_peg_h + EPS);
            cylinder(d=hub_d, h=hub_h);
            for (s = [-1, 1]) translate([0, 0, hub_h - 1]) rotate([0, s*rod_angle, 0]) {
                cylinder(d=rod_d, h=rod_len);
                translate([0, 0, rod_len]) sphere(d=ball_d);
            }
        }
        // flat back so the part prints lying down
        translate([-100, -rod_d/2, -50]) cube([200, 100, 200]);
    }
}

/* ================================================================== */
/* Speaker clamp: local ring in the XY plane, z 0..clamp_t             */
/* ================================================================== */
module speaker_clamp() {
    difference() {
        union() {
            cylinder(d=clamp_od, h=clamp_t);
            for (a = spk_boss_angles) translate([spk_boss_r*cos(a), spk_boss_r*sin(a), 0]) cylinder(d=spk_boss_d + 2, h=clamp_t);
        }
        translate([0, 0, -1]) cylinder(d=clamp_id, h=clamp_t + 2);
        for (a = spk_boss_angles) translate([spk_boss_r*cos(a), spk_boss_r*sin(a), -1]) cylinder(d=clear_m3, h=clamp_t + 2);
        // notch for the solder tabs
        rotate([0, 0, spk_notch_angle]) translate([clamp_id/2 - 1, -6, -1]) cube([clamp_od, 12, clamp_t + 2]);
    }
}

/* ================================================================== */
/* Ghost components for the assembly view                              */
/* ================================================================== */
module ghost_components() {
    // CYD: PCB, glass, rear header, USB ports (left edge), microSD (top edge)
    color(C_PCB, 0.6) translate([cyd_x0, cyd_y, cyd_z0]) cube([cyd_w, cyd_t, cyd_h]);
    color(C_GHOST) translate([cyd_x0 + cyd_screen_cx - cyd_glass_w/2, cyd_y - cyd_glass, cyd_z0]) cube([cyd_glass_w, cyd_glass, cyd_glass_h]);
    color(C_GHOST) translate([cyd_x0 + 8, cyd_y + cyd_t, cyd_z0 + 30]) cube([10, 8.5, 2.5]);          // 4-pin header
    color(C_GHOST) translate([cyd_x0 - 1.5, cyd_y + cyd_t, cyd_z0 + 12]) cube([9, 3.2, 9]);           // USB-C
    color(C_GHOST) translate([cyd_x0 - 1.5, cyd_y + cyd_t, cyd_z0 + 28]) cube([8, 2.8, 8]);           // micro USB
    color(C_GHOST) translate([sd_cx - 6, cyd_y + cyd_t, cyd_z0 + cyd_h - 12]) cube([12, 2.2, 15]);    // microSD + card
    // tact switches on the carrier
    for (z = knob_z) color(C_GHOST) {
        translate([knob_x - 3, carrier_y - sw_body_h, z - 3]) cube([6, sw_body_h, 6]);
        translate([knob_x, 0, z]) cyl_y(sw_plunger_d, carrier_y - sw_h, carrier_y - sw_body_h);
    }
    // speaker
    color(C_METAL, 0.6) translate([spk_cx, D - wall, spk_cz]) {
        cyl_y(spk_d, -spk_flange_t, 0);
        cyl_y(spk_magnet_d, -spk_depth, -spk_flange_t);
        cyl_y(32, -8, -spk_flange_t);
    }
    // amp
    color("#7B2D8E", 0.6) translate([amp_x - amp_w/2, D - wall - amp_l, amp_z - amp_t/2]) cube([amp_w, amp_l, amp_t]);
    // USB breakout
    color("#C0392B", 0.6) translate([usb_x - usb_pcb_w/2, D - wall - usb_pcb_l, usb_z - usb_pcb_t/2]) cube([usb_pcb_w, usb_pcb_l, usb_pcb_t]);
    color(C_METAL, 0.6) translate([usb_x - usb_conn_w/2, D - wall - usb_conn_l + 1, usb_z + usb_pcb_t/2]) cube([usb_conn_w, usb_conn_l, usb_conn_h]);
}

/* ================================================================== */
/* Assembly                                                            */
/* ================================================================== */
module assembly() {
    e = explode;
    color(C_FRONT) translate([0, -e, 0]) front_shell();
    color(C_FRONT) translate([0, e, 0]) rear_shell();
    color(C_DARK) translate([sur_cx, -2*e, sur_cz]) surround();
    for (z = knob_z) {
        color(C_TEAL) translate([knob_x, -(knob_gap + knob_t) - 3*e, z]) knob();
        color(C_TEAL) translate([knob_x, plate_t + 0.3 - e, z]) knob_keeper();
    }
    color(C_TEAL) translate([0, carrier_y - 0.5*e, 0]) button_carrier();
    color(C_TEAL) translate([grill_x0, grill_recess - grill_t - 3*e, grill_z0]) grill();
    for (i = [0 : 3])
        color(C_LEG) translate([leg_pos[i][0], leg_pos[i][1], -e]) mirror([leg_pos[i][0] > W/2 ? 0 : 1, 0, 0]) leg();
    color(C_LEG) translate([box_x0, box_y0, H + e]) set_top_box();
    color(C_BLACK) translate([box_x0 + box_w/2, box_y0 + box_d/2, H + box_h + 2*e]) antenna();
    color(C_TEAL) translate([spk_cx, D - wall - spk_flange_t + e, spk_cz]) rotate([90, 0, 0]) speaker_clamp();
    if (show_components) ghost_components();
}

/* ================================================================== */
/* Export orientation (each part flat on the bed)                      */
/* ================================================================== */
module export(name) {
    if (name == "front_shell")     rotate([90, 0, 0]) front_shell();                         // face down
    else if (name == "rear_shell") translate([0, 0, D]) rotate([-90, 0, 0]) rear_shell();     // back panel down
    else if (name == "surround")   rotate([90, 0, 0]) surround();                            // face down
    else if (name == "knob")       rotate([90, 0, 0]) knob();                                // face down, stem up
    else if (name == "knob_keeper") rotate([90, 0, 0]) knob_keeper();
    else if (name == "button_carrier") translate([0, 0, carrier_t]) rotate([-90, 0, 0]) button_carrier();
    else if (name == "grill")      translate([0, 0, grill_t]) rotate([-90, 0, 0]) grill();   // slots up
    else if (name == "leg_r")      rotate([90, 0, 0]) leg();                                 // lying on its side
    else if (name == "leg_l")      rotate([90, 0, 0]) mirror([1, 0, 0]) leg();
    else if (name == "set_top_box") translate([0, 0, box_h]) rotate([180, 0, 0]) set_top_box(); // upside down, pegs up
    else if (name == "antenna")    translate([0, 0, rod_d/2]) rotate([90, 0, 0]) antenna();  // flat back down
    else if (name == "speaker_clamp") speaker_clamp();
    else if (name == "plate")      plate();
    else assembly();
}

// Every part in print orientation, laid out for a look (not a print plate)
module plate() {
    color(C_FRONT) export("front_shell");
    color(C_FRONT) translate([W + 15, 0, 0]) export("rear_shell");
    color(C_DARK)  translate([sur_w/2, -30 - sur_h/2 - 20, 0]) export("surround");
    color(C_TEAL)  translate([100, -40, 0]) export("knob");
    color(C_TEAL)  translate([120, -40, 0]) export("knob_keeper");
    color(C_TEAL)  translate([40, -70, 0]) export("button_carrier");
    color(C_TEAL)  translate([140, -50, 0]) export("grill");
    color(C_LEG)   translate([170, -50, 0]) export("leg_l");
    color(C_LEG)   translate([190, -50, 0]) export("leg_r");
    color(C_LEG)   translate([210, -80, 0]) export("set_top_box");
    color(C_BLACK) translate([120, -100, 0]) export("antenna");
    color(C_TEAL)  translate([140, -140, 0]) export("speaker_clamp");
}

export(part);
