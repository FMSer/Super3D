package {
	import super3d.Object3D;
	public class main extends Object3D {
		public function main() {
			stage.addEventListener(Event.ENTER_FRAME, onEnterFrame);
		}
		public function onEnterFrame(e:Event):void {
			mc.rx++;
			mc.ry++;
			mc.rz++;
			mc.s+=0.01;
			mc.x=0;

		}
	}
}
