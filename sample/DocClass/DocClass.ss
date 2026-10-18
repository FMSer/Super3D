package {
    import super3d.display.Object3D;
    import super3d.events.Event;
    public class main extends Object3D {
        public function main() {
            stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);
        }
        public function onEnterFrame(e:Event):void {
            trace("ok\n");
            mc.rx++;
            mc.ry++;
            mc.rz++;
            mc.rotationX++;
            mc.s += 0.01;
        }
    }
}
