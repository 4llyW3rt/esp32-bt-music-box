// Cat-head music box - parametric first-fit enclosure
// Units: mm. This is a fit-check prototype; electronics mounting details remain adjustable.
$fn = 64;
W = 200; H = 150; D = 150;
wall = 3.0;
front_depth = 55;
back_overlap = 4;

// Rounded cat-head outline in X/Z plane. Overall envelope: 200 x 150 mm.
module cat_outline_2d(inset=0) {
    // Scale the silhouette to the requested 200 x 150 mm overall envelope.
    translate([0,-4.255]) translate([100,0]) scale([1.1236,1.06383]) translate([-100,0])
    offset(r=6-inset) offset(delta=-(6-inset))
    polygon(points=[
      [24,4],[15,18],[11,42],[13,68],[25,88],[20,112],
      [27,145],[51,132],[76,145],[124,145],[149,132],[173,145],
      [180,112],[175,88],[187,68],[189,42],[185,18],[176,4]
    ]);
}

// Depth profile in Y/Z. The rear narrows and rounds smoothly toward the back.
module depth_profile_2d(inner=false) {
    if (!inner) polygon(points=[
      [0,0],[0,150],[38,150],[58,148],[78,143],[100,136],
      [122,128],[140,116],[150,105],[150,45],[140,34],
      [122,22],[100,14],[78,7],[58,2],[38,0]
    ]);
    else polygon(points=[
      [3,3],[3,147],[38,147],[58,145],[78,140],[100,133],
      [122,125],[137,113],[147,103],[147,47],[137,37],
      [122,25],[100,17],[78,10],[58,5],[38,3]
    ]);
}

// Extrude X/Z silhouette along Y (front at y=0).
module silhouette_prism(inset=0, y0=0, depth=D) {
    translate([0,y0+depth,0]) rotate([90,0,0])
      linear_extrude(height=depth, convexity=10) cat_outline_2d(inset);
}

// Extrude Y/Z depth profile across X using a coordinate transform.
module depth_prism(inner=false, x0=0, width=W) {
    translate([x0,0,0])
      multmatrix([[0,0,1,0],[1,0,0,0],[0,1,0,0],[0,0,0,1]])
        linear_extrude(height=width, convexity=10) depth_profile_2d(inner);
}

module outer_body() { intersection() { silhouette_prism(0,0,D); depth_prism(false,0,W); } }
module inner_cavity() { intersection() { silhouette_prism(wall,3,D-6); depth_prism(true,0,W); } }

// Clip helper: y-range segment.
module y_clip(ymin,ymax) { translate([-2,ymin,-2]) cube([W+4,ymax-ymin,H+4]); }

// Front face cutouts: screen, nose encoder, and two speaker acoustic windows.
module front_cutouts() {
    // Screen viewing window. PCB recess and mounting are separate features below.
    translate([100-20,-1,43-14]) cube([40,8,28]);
    // Nose/volume encoder shaft opening (provisional 8 mm).
    translate([100,-1,82]) rotate([-90,0,0]) cylinder(d=8.5,h=9);
    // Ear speaker openings: speaker frames mounted vertically, one in each ear.
    for (cx=[48,152]) {
      translate([cx,-1,108]) rotate([-90,0,0])
        linear_extrude(height=8) offset(r=3) offset(delta=-3) square([24,58],center=true);
    }
}

// Mounting holes are drilled along the Y axis.
module y_hole(x,z,dia,depth=12) {
    translate([x,-1,z]) rotate([-90,0,0]) cylinder(d=dia,h=depth);
}

module screen_standoffs() {
    // Board: 62.48 x 38.30; measured hole-center spacing: 55 x 31.
    // Standoffs are on the inner face; holes are provisional M2.5 clearance.
    for (x=[72.5,127.5], z=[27.5,58.5]) {
      translate([x,5,z]) rotate([-90,0,0]) difference() {
        cylinder(d=7,h=5);
        translate([0,0,-0.5]) cylinder(d=2.8,h=6);
      }
    }
}

module speaker_mount_holes() {
    // Speaker frame 70 x 32 x 16; hole-center spacing 65 x 26; 4 mm holes.
    // Rotated vertically in each ear: 26 mm horizontal, 65 mm vertical.
    for (cx=[48,152]) for (dx=[-13,13], dz=[-32.5,32.5])
      y_hole(cx+dx,108+dz,4.2,22);
}

module front_shell() {
    difference() {
      intersection() { outer_body(); y_clip(0,front_depth); }
      // Hollow the shell from the rear, leaving a 3 mm front face.
      intersection() { inner_cavity(); y_clip(3,front_depth-3); }
      front_cutouts();
      speaker_mount_holes();
      // Four shallow fastener access holes near perimeter, provisional.
      for (x=[25,175], z=[22,125]) y_hole(x,z,3.2,15);
    }
    screen_standoffs();
}

module rear_cover() {
    difference() {
      intersection() { outer_body(); y_clip(front_depth-back_overlap,D); }
      // Hollow rear cover, leaving a 3 mm curved rear wall.
      intersection() { inner_cavity(); y_clip(front_depth-back_overlap,D-3); }
      // Rear access / cable opening, provisional 18 x 10 mm near lower center.
      translate([100-9,D-8,12]) cube([18,10,10]);
      // Matching perimeter screw holes.
      for (x=[25,175], z=[22,125]) y_hole(x,z,3.2,D-front_depth+12);
    }
}

// Export one part at a time by changing PART to "front" or "rear".
PART = "front";
if (PART == "front") front_shell();
else if (PART == "rear") rear_cover();
else if (PART == "both") { front_shell(); rear_cover(); }
