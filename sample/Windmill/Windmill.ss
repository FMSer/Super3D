package {
	import super3d.Object3D;
	public class main extends Object3D {
		public function main() {
			trace("Windmill spinning!");
			stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}
		public function onEnterFrame(e:Event):void {
			bladesGroup.rx = frame * 1;
		}
	}
}
