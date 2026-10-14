package {
	import super3d.Object3D;
	public class main extends Object3D {
		public function main() {
			trace("Ball bouncing!");
			stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}
		public function onEnterFrame(e:Event):void {
			ball.y = 1 + 4 * abs(sin(frame * 8));
			ball.ry = frame * 10;
		}
	}
}
