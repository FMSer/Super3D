package {
    import super3d.display.Object3D;
    import super3d.events.Event;
    public class main extends Object3D {
        var f:int = 0;
        public function main() {
            stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);
        }
        public function lerp(a:Number, b:Number, t:Number):Number {
            return a + (b - a) * t;
        }
        public function smooth(t:Number):Number {
            if (t < 0) { t = 0; }
            if (t > 1) { t = 1; }
            return t * t * (3 - 2 * t);
        }
        public function onEnterFrame(e:Event):void {
            f++;
            if (f > 120) { f = 1; }
            if (f <= 8) {
                hopper.x = 0;
                hopper.z = 0;
                hopper.ry = 0;
                hopper.rx = 0;
                hopper.s = 1;
                hopper.y = 0.02 * Math.sin(f * 0.5);
            } else if (f <= 52) {
                var tc:Number = smooth((f - 9) / 43);
                hopper.x = lerp(0, -6, tc);
                hopper.z = lerp(0, 6, tc);
                hopper.ry = lerp(0, 45, smooth((f - 9) / 15));
                hopper.y = Math.abs(Math.sin(f * 0.9)) * 0.08;
            } else if (f <= 88) {
                hopper.x = -6;
                hopper.z = 6;
                hopper.ry = 45;
                hopper.y = 0;
                hopper.rx = 2.5 * Math.sin((f - 53) * 0.3);
            } else if (f <= 96) {
                var tq:Number = smooth((f - 89) / 7);
                hopper.rx = lerp(hopper.rx, 0, tq);
                hopper.y = lerp(0, -0.18, tq);
                hopper.s = lerp(1, 0.9, tq);
                hopper.ry = lerp(45, 222.6, tq);
            } else if (f <= 110) {
                var tj:Number = (f - 97) / 13;
                hopper.x = lerp(-6, 6, tj);
                hopper.z = lerp(6, -5, tj);
                hopper.y = -0.18 + 0.18 * tj + 13.6 * tj * (1 - tj);
                hopper.rx = -10 * Math.sin(3.14159 * tj);
                var ts2:Number = tj * 2;
                if (ts2 > 1) { ts2 = 1; }
                hopper.s = lerp(0.9, 1, ts2);
            } else if (f <= 116) {
                hopper.x = 6;
                hopper.z = -5;
                hopper.y = 0;
                hopper.rx = 0;
                hopper.ry = 222.6;
                if (f <= 112) {
                    hopper.s = 0.92;
                } else {
                    hopper.s = lerp(0.92, 1, (f - 112) / 4);
                }
            } else {
                hopper.x = 6;
                hopper.z = -5;
                hopper.y = 0.02 * Math.sin(f * 0.5);
                hopper.rx = 0;
                hopper.ry = 222.6;
                hopper.s = 1;
            }
        }
    }
}
