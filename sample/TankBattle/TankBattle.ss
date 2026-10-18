package {
    import super3d.display.Object3D;
    import super3d.events.*;
    import super3d.events.Event;
    public class main extends Object3D {
        var score = 0;
        var hp = 100;
        var en1 = 1; var en2 = 1; var en3 = 1;
        var d1 = 1; var d2 = -1; var d3 = 1;
        var ba = 0; var bx = 0; var bz = 0; var bd = 0;
        var ea = 0; var ex = 0; var ez = 0; var ed = 0;
        var cd = 0; var cde = 0; var over = 0;
        var mv = 0; var tr = 0; var fk = 0;
        public function main() {
            stage.addEventListener(KeyboardEvent.KEY_DOWN, onKeyDown);
            stage.addEventListener(KeyboardEvent.KEY_UP, onKeyUp);
        }
        public function onKeyDown(e) {
            if (e == 65 || e == 37) tr = 1;
            if (e == 68 || e == 39) tr = -1;
            if (e == 87 || e == 38) mv = 1;
            if (e == 83 || e == 40) mv = -1;
            if (e == 32) fk = 1;
        }
        public function onKeyUp(e) {
            if (e == 65 || e == 37) { if (tr == 1) tr = 0; }
            if (e == 68 || e == 39) { if (tr == -1) tr = 0; }
            if (e == 87 || e == 38) { if (mv == 1) mv = 0; }
            if (e == 83 || e == 40) { if (mv == -1) mv = 0; }
            if (e == 32) fk = 0;
        }
        public function hitWall(x, z) {
            if (Math.abs(x) < 3.5 && Math.abs(z) < 3.5) return true;
            if (Math.abs(x + 10) < 1.5 && Math.abs(z + 10) < 1.5) return true;
            if (Math.abs(x - 10) < 1.5 && Math.abs(z - 10) < 1.5) return true;
            if (Math.abs(x + 10) < 1.5 && Math.abs(z - 12) < 1.5) return true;
            if (Math.abs(x - 10) < 1.5 && Math.abs(z + 12) < 1.5) return true;
            return false;
        }
        public function playerFire() {
            ba = 1;
            bd = playerTank.ry;
            bx = playerTank.x - sin(bd) * 2.0;
            bz = playerTank.z - cos(bd) * 2.0;
            bullet.visible = true;
            bullet.ry = bd;
        }
        public function onEnterFrame(e:Event) {
            if (over == 1) return;
            // 1) player turn: written by keyDown/keyUp events (A/D or Left/Right)
            if (tr == 1) playerTank.ry = playerTank.ry + 3;
            if (tr == -1) playerTank.ry = playerTank.ry - 3;
            // 2) player move: mv=+-1 along heading (blocked by wall/obstacles, field bounds)
            var sp = 0.15;
            var nx = playerTank.x;
            var nz = playerTank.z;
            if (mv == 1) { nx = playerTank.x - sin(playerTank.ry) * sp; nz = playerTank.z - cos(playerTank.ry) * sp; }
            if (mv == -1) { nx = playerTank.x + sin(playerTank.ry) * sp; nz = playerTank.z + cos(playerTank.ry) * sp; }
            if (!hitWall(nx, nz)) { playerTank.x = nx; playerTank.z = nz; }
            if (playerTank.x > 20) playerTank.x = 20;
            if (playerTank.x < -20) playerTank.x = -20;
            if (playerTank.z > 20) playerTank.z = 20;
            if (playerTank.z < -20) playerTank.z = -20;
            // 3) fire (Space keyDown sets fk, with cooldown; gun fixed to tank heading)
            cd = cd - 1; if (cd < 0) cd = 0;
            if (fk == 1 && ba == 0 && cd == 0) { playerFire(); cd = 14; }
            // 4) player bullet flight + hit test (spherical bullet, hit radius 1.5)
            if (ba == 1) {
                bx = bx - sin(bd) * 0.55;
                bz = bz - cos(bd) * 0.55;
                bullet.x = bx; bullet.z = bz;
                if (en1 == 1 && Math.abs(bx - enemy1.x) < 1.5 && Math.abs(bz - enemy1.z) < 1.5) { en1 = 0; enemy1.visible = false; enemy1Gun.visible = false; ba = 0; bullet.visible = false; score = score + 10; }
                if (en2 == 1 && Math.abs(bx - enemy2.x) < 1.5 && Math.abs(bz - enemy2.z) < 1.5) { en2 = 0; enemy2.visible = false; enemy2Gun.visible = false; ba = 0; bullet.visible = false; score = score + 10; }
                if (en3 == 1 && Math.abs(bx - enemy3.x) < 1.5 && Math.abs(bz - enemy3.z) < 1.5) { en3 = 0; enemy3.visible = false; enemy3Gun.visible = false; ba = 0; bullet.visible = false; score = score + 10; }
                if (hitWall(bx, bz) || Math.abs(bx) > 20 || Math.abs(bz) > 20) { ba = 0; bullet.visible = false; }
            }
            // 5) enemy patrol (only alive ones move)
            if (en1 == 1) { enemy1.x = enemy1.x + 0.07 * d1; if (enemy1.x > 15) d1 = -1; if (enemy1.x < -15) d1 = 1; }
            if (en2 == 1) { enemy2.x = enemy2.x + 0.06 * d2; if (enemy2.x > 15) d2 = -1; if (enemy2.x < -15) d2 = 1; }
            if (en3 == 1) { enemy3.x = enemy3.x + 0.065 * d3; if (enemy3.x > 15) d3 = -1; if (enemy3.x < -15) d3 = 1; }
            // 6) enemy turns toward player (gun fixed to body, fires at player)
            if (en1 == 1) { enemy1.ry = Math.atan2(enemy1.x - playerTank.x, enemy1.z - playerTank.z) * 180 / Math.PI; }
            if (en2 == 1) { enemy2.ry = Math.atan2(enemy2.x - playerTank.x, enemy2.z - playerTank.z) * 180 / Math.PI; }
            if (en3 == 1) { enemy3.ry = Math.atan2(enemy3.x - playerTank.x, enemy3.z - playerTank.z) * 180 / Math.PI; }
            // 7) enemy fire (first alive enemy, with cooldown)
            cde = cde - 1; if (cde < 0) cde = 0;
            if (ea == 0 && cde == 0) {
                if (en1 == 1) { ea = 1; ed = enemy1.ry; ex = enemy1.x - sin(ed) * 2.0; ez = enemy1.z - cos(ed) * 2.0; enemyBullet.visible = true; enemyBullet.ry = ed; cde = 40; }
                else if (en2 == 1) { ea = 1; ed = enemy2.ry; ex = enemy2.x - sin(ed) * 2.0; ez = enemy2.z - cos(ed) * 2.0; enemyBullet.visible = true; enemyBullet.ry = ed; cde = 40; }
                else if (en3 == 1) { ea = 1; ed = enemy3.ry; ex = enemy3.x - sin(ed) * 2.0; ez = enemy3.z - cos(ed) * 2.0; enemyBullet.visible = true; enemyBullet.ry = ed; cde = 40; }
            }
            // 8) enemy bullet flight + hit player (5 HP per hit)
            if (ea == 1) {
                ex = ex - sin(ed) * 0.5;
                ez = ez - cos(ed) * 0.5;
                enemyBullet.x = ex; enemyBullet.z = ez;
                if (Math.abs(ex - playerTank.x) < 1.5 && Math.abs(ez - playerTank.z) < 1.5) { ea = 0; enemyBullet.visible = false; hp = hp - 5; }
                if (hitWall(ex, ez) || Math.abs(ex) > 20 || Math.abs(ez) > 20) { ea = 0; enemyBullet.visible = false; }
            }
            // 9) HUD / win-loss
            if (hp <= 0) { over = 1; hudText = "Game Over! Score=" + score + " Click to restart"; }
            else if (en1 == 0 && en2 == 0 && en3 == 0) { over = 1; hudText = "Victory! All enemies destroyed Score=" + score + " Click to restart"; }
            else { hudText = "Tank Battle  HP=" + hp + "  Score=" + score + "   W/S move A/D turn Space fire"; }
        }
    }
}
