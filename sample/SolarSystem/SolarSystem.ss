package {
	import super3d.Object3D;
	public class main extends Object3D {
		public function main() {
			trace("Solar system running!");
			stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}
		public function onEnterFrame(e:Event):void {
			mercury.x = 6 * cos(frame * 5);
			mercury.z = 6 * sin(frame * 5);
			earth.x = 10 * cos(frame * 3);
			earth.z = 10 * sin(frame * 3);
			mars.x = 14 * cos(frame * 2);
			mars.z = 14 * sin(frame * 2);
		}
	}
}
