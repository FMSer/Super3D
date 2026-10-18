package {
    import super3d.display.Object3D;
    import super3d.events.Event;

    public class main extends Object3D {
        private var tick:Number = 0;

        public function main() {
            S1Flame.sx = 0.01; S1Flame.sy = 0.01; S1Flame.sz = 0.01;
            S2Flame.sx = 0.01; S2Flame.sy = 0.01; S2Flame.sz = 0.01;
            Smoke.sx = 0.15; Smoke.sy = 0.15; Smoke.sz = 0.15;
            SatPanelL.rz = 90; SatPanelR.rz = -90;
            Fairing.x = 0; Fairing.y = 0; Fairing.rz = 0;
            Booster.x = 0; Booster.y = 0.5; Booster.rz = 0;
            Stage2.x = 0; Stage2.y = 4.1; Stage2.rz = 0;
        }

        public function onEnterFrame(e:Event):void {
            tick = tick + 1;
            if (tick > 320) tick = 0;
            if (frame >= 90) gotoFrame(0);
            var t1 = tick;

            var dy = 0;
            if (t1 > 30 && t1 <= 150) dy = 0.00208 * (t1 - 30) * (t1 - 30);
            if (t1 <= 150) {
                Booster.y = 0.5 + dy;
                Stage2.y = 4.1 + dy;
                Booster.x = dy * 0.05;
                Stage2.x = dy * 0.05;
                var tilt = 0;
                if (t1 > 60) tilt = 14 * ((t1 - 60) / 90);
                Booster.rz = tilt;
                Stage2.rz = tilt;
            }

            if (t1 > 150) {
                var u = t1 - 150;
                Stage2.y = 34.1 + 0.5 * u + 0.002 * u * u;
                Stage2.x = 1.5 + u * 0.045;
                Stage2.rz = 14 + u * 0.06;
                var by = 30.5 - 0.004 * u * u;
                if (by < 0.8) by = 0.8;
                Booster.y = by;
                Booster.x = 1.5 + u * 0.05;
                if (Booster.x > 8) Booster.x = 8;
                Booster.rz = 14 + u * 3.2;
            }

            if (t1 < 30 || t1 > 152) {
                S1Flame.sx = 0.01; S1Flame.sy = 0.01; S1Flame.sz = 0.01;
            } else {
                var f1 = 0.8 + 0.4 * random();
                S1Flame.sx = f1;
                S1Flame.sy = f1 * (1 + 0.06 * sin(t1 * 0.9));
                S1Flame.sz = f1;
            }

            if (t1 < 155) {
                S2Flame.sx = 0.01; S2Flame.sy = 0.01; S2Flame.sz = 0.01;
            } else {
                var f2 = 0.8 + 0.35 * random();
                S2Flame.sx = f2; S2Flame.sy = f2; S2Flame.sz = f2;
            }

            var sm = 0.15;
            if (t1 >= 27) sm = 0.15 + 0.022 * (t1 - 27);
            if (sm > 3.4) sm = 3.4;
            Smoke.sx = sm; Smoke.sy = sm * 0.6; Smoke.sz = sm;

            if (t1 <= 200) {
                Fairing.x = 0; Fairing.y = 0; Fairing.rz = 0;
            } else {
                var v = t1 - 200;
                Fairing.x = v * 0.06;
                Fairing.y = v * 0.085;
                Fairing.rz = v * 2;
                if (Fairing.rz > 50) Fairing.rz = 50;
            }

            var pl = 90 * (1 - (t1 - 240) / 30);
            if (t1 < 240) pl = 90;
            if (t1 > 270) pl = 0;
            SatPanelL.rz = pl;
            SatPanelR.rz = -pl;
        }
    }
}
