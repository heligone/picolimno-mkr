// Support
// translate([0, 30, -190]) cube([35, 20, 400], center=true);

module Panneau(tr = 5) {
// Plaque sous panneau solaire
    rotate([45,0,0]) difference() {
// façade
        hull() for (x=[-1,1]) for (y=[-1,1]) translate([x*(210/2-10),y*(115/2-10), tr]) cylinder(r=10, h=2, center=true);
// trous des vis            
        for (x=[-1,1]) for (y=[-1,1]) translate([x*195/2,y*100/2,5]) cylinder(r=2, h=3, center=true);
// trou du cable et attache
        translate([195/2-10,0,tr]) scale([1.5,1.0,1.0]) cylinder(r=10, h=3, center=true);
    }
};

module Boite(arrondi = 5, epaisseur = 2) {
    difference() {
        union() {
     // Tube du support
            translate([0,29.35,-11.8]) cube([39,24,59], center=true);

     // Boite
            difference() {
                hull() for (x=[arrondi-105,105-arrondi]) {
                    translate([x,41.35-arrondi,50-arrondi]) sphere(r=arrondi,$fn=36);
                    translate([x,arrondi-50,arrondi-41.35]) sphere(r=arrondi,$fn=36);
                    translate([x,41.35-arrondi,arrondi-41.35]) sphere(r=arrondi,$fn=36);
                }
                hull() for (x=[arrondi-105,105-arrondi]) {
                    translate([x,41.35-arrondi,50-arrondi]) sphere(r=arrondi-epaisseur,$fn=36);
                    translate([x,arrondi-50,arrondi-41.35]) sphere(r=arrondi-epaisseur,$fn=36);
                    translate([x,41.35-arrondi,arrondi-41.35]) sphere(r=arrondi-epaisseur,$fn=36);
                }
            }
            
    // Trous de fermeture (écrous)      
            intersection() {
                 rotate([45,0,0]) for (x=[-97.5,97.5]) for (y=[-50,50]) translate([x,y,-6.0]) cylinder(r=6,h=13,$fn=36,center=true);
                hull() for (x=[arrondi-105,105-arrondi]) for (i=[23,37]) {
                    translate([x,41.35-arrondi,i]) sphere(r=arrondi,$fn=36);
                    translate([x,-i,arrondi-41.35]) sphere(r=arrondi,$fn=36);
                }
            }
    // Épaulement du capteur US
        translate([70,0,-39]) cylinder(r=20,h=3,$fn=60,center=true);
        }

    // Trou de la barre
        translate([0,29.35,-15]) cube([35,20,61],center=true);
    // Trou de la vis de fixation
        translate([0,40.35,0]) rotate([90,0,0]) cylinder(r=2,h=3,center=true,$fn=16);
    // Tranchage de la façade
        translate([0,-7,7]) rotate([45,0,0]) cube([220,130,20],center=true);
    // Percement des trous de fixation de la façade
        rotate([45,0,0]) for (x=[-97.5,97.5]) for (y=[-50,50]) {
    // Trou du filetage
            translate([x,y,-1]) cylinder(r=1.75,h=3,$fn=16,center=true);
    // Trou de passage de l'écrou
            translate([x,y,-7]) cylinder(r=4,h=12,$fn=36,center=true);
        }
    // Trou du capteur US
        translate([70,0,-39.5]) cylinder(r=25/2+.25,h=5,$fn=36,center=true);
        
    // Goutte (rainure sur le dessous)
        hull() {
            translate([98,34.25,-43]) sphere(r=2.5,$fn=16);
            translate([98,-34.25,-43]) sphere(r=2.5,$fn=16);
        }
        hull() {
            translate([98,-34.25,-43]) sphere(r=2.5,$fn=16);
            translate([-98,-34.25,-43]) sphere(r=2.5,$fn=16);
        }
        hull() {
            translate([-98,-34.25,-43]) sphere(r=2.5,$fn=16);
            translate([-98,34.25,-43]) sphere(r=2.5,$fn=16);
        }
        hull() {
            translate([-98,34.25,-43]) sphere(r=2.5,$fn=16);
            translate([98,34.25,-43]) sphere(r=2.5,$fn=16);
        }

    // Épaulement & percements pour AM2302
        translate([0,0,-43]) cube([54,28,5],center=true);
        translate([22,0,-42]) cylinder(r=0.5,h=3,$fn=8);
        translate([-33,0,-42]) cylinder(r=2.5,h=3,$fn=16);
        translate([-33,11,-42]) cylinder(r=0.5,h=3,$fn=8);
        translate([-33,-11,-42]) cylinder(r=0.5,h=3,$fn=8);
    }

    // Tétons d'accrochage de la carte électronique
    difference() {
        translate([-60,7,-7]) rotate([45,0,0]) {
            for (y=[-82.25,82.25])
                for (x=[-62.25,62.25])
                    translate([x/2,y/2,-4]) difference () {
                        cylinder(r=4,h=7,$fn=16,center=true);
                        cylinder(r=0.5,h=8,$fn=16,center=true);
                    }
        }
        translate([0,0,-45]) cube([220,90,10],center=true);
        translate([0,45,0]) rotate([90,0,0]) cube([220,90,10],center=true);
    }
};

// Bouchon de AM2302
module Bouchon() {
    difference() {
        union() {
            hull() for (x=[-1,1]) for (y=[-1,1]) translate([11*x,y,0]) cylinder(r=2.5,h=2,$fn=36);
            union() {
                translate([0,2.5,0]) rotate([90,0,0]) cylinder(r=8.5,h=5,center=true,$fn=104);
                sphere(r=8.5,$fn=52);
            }
        }
        hull() {
            translate([0,9,0]) rotate([90,0,0]) cylinder(r=7.5,h=1,center=true,$fn=36);
            sphere(r=7.5,$fn=36);
        }
        translate([0,-1.75,-5]) cube([28,14.5, 10],center=true);
        for (x=[-1,1]) translate([x*11,0,1]) cylinder(r=1,h=3,$fn=16,center=true);
    }
}



module GrandCote() {
    difference() {
        Boite();
        translate([85,0,0]) cube([100,100,100],center=true);
    };
};

module PetitCote() {
    intersection() {
        Boite();
        translate([85.25,0,0]) cube([100,100,100],center=true);
    };

    difference() {
        translate([35,0,-39]) cube([6,70,3], center=true);
        translate([32,0,-40]) cube([6.5,71,1.5], center=true);
    };

    difference() {
        translate([35,39,0]) cube([6,3,70], center=true);
        translate([32,40,0]) cube([6.5,1.5,71], center=true);
    };
};


PetitCote();
GrandCote();
Bouchon();