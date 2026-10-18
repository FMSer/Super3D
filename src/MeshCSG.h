// Super3D - 3D Scene Designer for Kids
// Copyright (C) 2026 FMSer.CN
// Contact: 3781320752@qq.com
// MeshCSG.h : 通用三角网格布尔运算（BSP / CSG），与图元类型完全无关。
//
// 设计（Evan Wallace csg.js 算法的纯 C++ 移植，无任何第三方依赖）：
//   1) 两个物体都先转成闭合三角网格（顶点 + 逐顶点法线），本模块不认识"球/柱/管/截锥"；
//   2) 用各自三角形所在平面建立 BSP 二叉空间分割树，三角形互相求交、沿交线切开，
//      切开处的新顶点用两端法线插值（曲面光滑），并保证两网格在交线上顶点一致；
//   3) 依平面正/反把多边形分类到 inside/outside/coplanar；
//   4) 并集=各自外侧片；差集=A 外侧 + B 内侧(翻转)；交集=各自内侧片；
//   5) 结果仍是闭合三角网格，法线随顶点携带，平面体保持平面、曲面保持平滑。
//
// 输入要求：每个网格为"闭合流形"（朝外法线）。盒/球/柱/锥/截锥/圆环/方管/圆管/布尔结果
//           由 GenerateMesh 输出时都满足；开放薄片（平面/曲线）不参与。
// 非凸体（如方管 = 外壁-内壁的管壳）无需特殊处理：BSP 对任意闭合流形都成立，
// 因此"空心方管差集只挖管壁、保留管腔"这类结果天然正确，不需要任何按图元写的分支。
// 注：类型名统一加 CSG 前缀，避免与 Windows GDI 的 Polygon 等全局符号冲突。
#pragma once
#include "CoreDefs.h"
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <ctime>
#include <cstdio>

namespace mcsg {

static const double kEps = 1e-5;

struct CSGVertex {
    Vec3 pos;
    Vec3 normal;
    CSGVertex() {}
    CSGVertex(const Vec3& p, const Vec3& n) : pos(p), normal(n) {}
    // 沿 this -> other 插值 t；法线插值后归一化，保证曲面切开后仍平滑
    CSGVertex lerp(const CSGVertex& other, double t) const {
        Vec3 p(pos.x + (other.pos.x - pos.x) * t,
               pos.y + (other.pos.y - pos.y) * t,
               pos.z + (other.pos.z - pos.z) * t);
        Vec3 n(normal.x + (other.normal.x - normal.x) * t,
               normal.y + (other.normal.y - normal.y) * t,
               normal.z + (other.normal.z - normal.z) * t);
        double L = std::sqrt(Dot(n, n));
        if (L > 1e-12) n = n * (1.0 / L);
        return CSGVertex(p, n);
    }
};

// 多边形（可含 N 个顶点，与 csg.js 一致）。输入是三角形，但在 BSP 中被平面切开后，
// 一侧可能成为四边形（N=4）甚至更多边的凸多边形；必须在整个 BSP 过程中保留 N 边形，
// 只在最终输出 emitPolys 时才按扇形三角化，否则会丢面/连错“刺”，并使三角数爆炸。
struct CSGPoly {
    std::vector<CSGVertex> v;
    CSGPoly() {}
    CSGPoly(const CSGVertex& a, const CSGVertex& b, const CSGVertex& c) {
        v.push_back(a); v.push_back(b); v.push_back(c);
    }
    explicit CSGPoly(const std::vector<CSGVertex>& verts) : v(verts) {}
    void flip() {
        std::reverse(v.begin(), v.end());
        for (size_t i = 0; i < v.size(); i++) v[i].normal = -v[i].normal;
    }
};

enum CSGTag { CSG_COPLANAR = 0, CSG_FRONT = 1, CSG_BACK = 2, CSG_SPANNING = 3 };

struct CSGPlane {
    Vec3 normal;
    double w;
    CSGPlane() : normal(0, 0, 1), w(0.0) {}
    void flip() { normal = -normal; w = -w; }
    static CSGPlane fromPoints(const Vec3& a, const Vec3& b, const Vec3& c) {
        CSGPlane p;
        p.normal = Normalize(Cross(b - a, c - a));
        p.w = Dot(p.normal, a);
        return p;
    }
    // 把一个（N 顶点）多边形按本平面切分：共面进 copF/copB，正面进 fr，背面进 bk，
    // 跨面在交线处插值分裂；切出的 front/back 片段仍为凸多边形（保留 N 顶点，不三角化）。
    void splitPoly(const CSGPoly& poly,
                   std::vector<CSGPoly>& copF, std::vector<CSGPoly>& copB,
                   std::vector<CSGPoly>& fr, std::vector<CSGPoly>& bk) const {
        size_t n = poly.v.size();
        int polyType = 0;
        std::vector<int> types(n);
        for (size_t i = 0; i < n; i++) {
            double t = Dot(normal, poly.v[i].pos) - w;
            int type = (t < -kEps) ? CSG_BACK : (t > kEps) ? CSG_FRONT : CSG_COPLANAR;
            polyType |= type;
            types[i] = type;
        }
        switch (polyType) {
            case CSG_COPLANAR: {
                if (Dot(normal, poly.v[0].normal) >= 0) copF.push_back(poly);
                else copB.push_back(poly);
                break;
            }
            case CSG_FRONT: fr.push_back(poly); break;
            case CSG_BACK:  bk.push_back(poly); break;
            case CSG_SPANNING: {
                std::vector<CSGVertex> fv, bv;
                for (size_t i = 0; i < n; i++) {
                    size_t j = (i + 1) % n;
                    int ti = types[i], tj = types[j];
                    const CSGVertex& vi = poly.v[i];
                    const CSGVertex& vj = poly.v[j];
                    if (ti != CSG_BACK) fv.push_back(vi);
                    if (ti != CSG_FRONT) bv.push_back(vi);
                    if ((ti | tj) == CSG_SPANNING) {
                        double denom = Dot(normal, vj.pos - vi.pos);
                        double t = (std::fabs(denom) > 1e-12) ? (w - Dot(normal, vi.pos)) / denom : 0.0;
                        if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
                        CSGVertex vx = vi.lerp(vj, t);
                        fv.push_back(vx);
                        bv.push_back(vx);
                    }
                }
                // 切出的凸多边形保持 N 顶点（BSP 全程不三角化，避免数量爆炸与错连），
                // 仅在最终 emitPolys 统一 fan。
                if (fv.size() >= 3) fr.push_back(CSGPoly(fv));
                if (bv.size() >= 3) bk.push_back(CSGPoly(bv));
                break;
            }
        }
    }
};

struct CSGNode {
    CSGPlane plane;
    bool hasPlane;
    std::vector<CSGPoly> polys;
    std::unique_ptr<CSGNode> front;
    std::unique_ptr<CSGNode> back;
    CSGNode() : hasPlane(false) {}
    explicit CSGNode(const std::vector<CSGPoly>& ps) : hasPlane(false) { build(ps); }

    void build(const std::vector<CSGPoly>& input) {
        if (input.empty()) return;
        if (!hasPlane) {
            plane = CSGPlane::fromPoints(input[0].v[0].pos, input[0].v[1].pos, input[0].v[2].pos);
            hasPlane = true;
        }
        std::vector<CSGPoly> copF, copB, fr, bk;
        for (size_t i = 0; i < input.size(); i++)
            plane.splitPoly(input[i], copF, copB, fr, bk);
        // 关键（保证递归终止）：共面片无论法向正/反都属于本节点平面，一律存入本节点、
        // 不再向下递归。这样每个节点至少把（相对自身平面共面的）input[0] 留在本层，
        // 传入子树的多边形数严格减少，递归深度 ≤ 多边形数，绝无无限递归/栈溢出。
        polys.insert(polys.end(), copF.begin(), copF.end());
        polys.insert(polys.end(), copB.begin(), copB.end());
        if (!fr.empty()) {
            if (!front) front.reset(new CSGNode());
            front->build(fr);
        }
        if (!bk.empty()) {
            if (!back) back.reset(new CSGNode());
            back->build(bk);
        }
    }

    std::vector<CSGPoly> clipPolygons(const std::vector<CSGPoly>& input) const {
        if (!hasPlane) return input;
        std::vector<CSGPoly> fr, bk;
        for (size_t i = 0; i < input.size(); i++) {
            std::vector<CSGPoly> copF, copB;
            plane.splitPoly(input[i], fr, bk, fr, bk);   // 共面片统一归正面
        }
        if (front) fr = front->clipPolygons(fr);
        if (back) bk = back->clipPolygons(bk);
        else bk.clear();
        std::vector<CSGPoly> out;
        out.reserve(fr.size() + bk.size());
        out.insert(out.end(), fr.begin(), fr.end());
        out.insert(out.end(), bk.begin(), bk.end());
        return out;
    }

    void clipTo(const CSGNode* other) {
        if (!other) return;
        polys = other->clipPolygons(polys);
        if (front) front->clipTo(other);
        if (back) back->clipTo(other);
    }

    void invert() {
        if (!hasPlane) return;
        plane.flip();
        for (size_t i = 0; i < polys.size(); i++) polys[i].flip();
        std::swap(front, back);
        if (front) front->invert();
        if (back) back->invert();
    }

    std::vector<CSGPoly> allPolygons() const {
        std::vector<CSGPoly> out = polys;
        if (front) {
            std::vector<CSGPoly> f = front->allPolygons();
            out.insert(out.end(), f.begin(), f.end());
        }
        if (back) {
            std::vector<CSGPoly> b = back->allPolygons();
            out.insert(out.end(), b.begin(), b.end());
        }
        return out;
    }
};

// 由三角网格（位置 av、法线 an，每 3 个一组）构造多边形。
// 关键鲁棒性：各图元 GenerateMesh 的顶点绕序未必与"法线数组"一致（部分面片几何叉积
// 朝内），而 BSP 用几何叉积确定平面/内外。这里以逐顶点法线（权威外法线）为准，
// 若某三角的几何叉积与平均外法线反向，则交换两个顶点，保证绕序朝外。退化三角丢弃。
static std::vector<CSGPoly> makePolys(const std::vector<Vec3>& av, const std::vector<Vec3>& an) {
    std::vector<CSGPoly> ps;
    size_t n = av.size() / 3;
    ps.reserve(n);
    for (size_t i = 0; i + 2 < av.size(); i += 3) {
        Vec3 p0 = av[i],     p1 = av[i + 1],     p2 = av[i + 2];
        Vec3 q0 = an[i],     q1 = an[i + 1],     q2 = an[i + 2];
        Vec3 geom = Cross(p1 - p0, p2 - p0);
        Vec3 refn = q0 + q1 + q2;
        if (Length(geom) > kEps && Length(refn) > kEps && Dot(geom, refn) < 0.0) {
            std::swap(p1, p2);
            std::swap(q1, q2);
        }
        if (Length(geom) <= kEps) continue;   // 退化（三点共线/重合）三角丢弃
        ps.push_back(CSGPoly(CSGVertex(p0, q0),
                             CSGVertex(p1, q1),
                             CSGVertex(p2, q2)));
    }
    return ps;
}

static void emitPolys(const std::vector<CSGPoly>& ps, std::vector<Vec3>& ov, std::vector<Vec3>& on) {
    // 最终输出：每个凸 N 边形按扇形（v[0] 为顶）三角化为 (N-2) 个三角形，绕序沿用多边形。
    for (size_t i = 0; i < ps.size(); i++) {
        const CSGPoly& p = ps[i];
        for (size_t k = 1; k + 1 < p.v.size(); k++) {
            const CSGVertex& a = p.v[0];
            const CSGVertex& b = p.v[k];
            const CSGVertex& c = p.v[k + 1];
            ov.push_back(a.pos); on.push_back(a.normal);
            ov.push_back(b.pos); on.push_back(b.normal);
            ov.push_back(c.pos); on.push_back(c.normal);
        }
    }
}

// 后处理：消除 T 型连接（conforming），使结果成为严格流形（每条边恰好被两个三角共享）。
// csg.js 用无限平面切多边形：A 面与 B 面的交线是同一条空间线段，但 B 面矩形自带的对角线
// （以及曲面三角边）会在交线上产生"落在 A 面整段边内部"的中间顶点（典型 0.2 处）。
// 几何上两段与整段完全重合（无洞），但拓扑不闭合，渲染为细缝、曲面交线还会不均匀。
// 这里逐个三角形：把落在其三条边上的其它顶点作为内部点插入（法线沿当前面边插值，
// 不引用邻面法线），得到一个"三角形轮廓含共线点"的凸多边形，再用耳切法（跳过共线/反向耳）
// 重建，使该三角形在这些内部点处分裂，与邻面片的边完全对齐。
static void ConformMesh(std::vector<Vec3>& v, std::vector<Vec3>& n) {
    int T0 = (int)v.size() / 3;
    if (T0 < 1) return;
    const double WELD = 1e-3;     // 视为共线/同点的容差（模型单位尺度 ~1..10）
    struct PN { Vec3 p, nn; };

    // --- 空间均匀网格哈希，把"每条边遍历全部顶点"的 O(n²) 降到近 O(n) ---
    Vec3 lo = v[0], hi = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i].x < lo.x) lo.x = v[i].x; if (v[i].x > hi.x) hi.x = v[i].x;
        if (v[i].y < lo.y) lo.y = v[i].y; if (v[i].y > hi.y) hi.y = v[i].y;
        if (v[i].z < lo.z) lo.z = v[i].z; if (v[i].z > hi.z) hi.z = v[i].z;
    }
    double diag = Length(hi - lo);
    double h = diag / 200.0;          // 格宽 ~ 点间距量级
    if (h < WELD * 10.0) h = WELD * 10.0;
    if (!(h > 0.0) || h > 1e9) h = 1.0;
    struct CKey { int x, y, z;
        bool operator==(const CKey& o) const { return x==o.x&&y==o.y&&z==o.z; } };
    struct CKHash { size_t operator()(const CKey& k) const {
        unsigned long long ux=(unsigned long long)(k.x+(1<<20)),
                           uy=(unsigned long long)(k.y+(1<<20)),
                           uz=(unsigned long long)(k.z+(1<<20));
        return (size_t)((ux*73856093u) ^ (uy*19349663u) ^ (uz*83492791u)); } };
    auto cellOf = [&](const Vec3& p) -> CKey {
        return { (int)std::floor(p.x/h), (int)std::floor(p.y/h), (int)std::floor(p.z/h) }; };
    std::unordered_map<CKey, std::vector<int>, CKHash> grid;
    grid.reserve(v.size() * 2);
    for (size_t i = 0; i < v.size(); ++i)
        grid[cellOf(v[i])].push_back((int)i);

    // 收集落在边 p0→p1 内部（参数 t∈(0,1)、离边≤WELD）且位于当前三角形平面内（到平面
    // 距离≤WELD）的顶点，按 t 升序。关键：曲面交线两侧三角形不共面，对侧曲面顶点虽离边
    // 很近，但不在当前三角形平面内，若当作本面边界点会 fan 出"跨表面错误桥"，破坏流形。
    // 故只接受共面点（平面交线严格 T 型）；曲面交线的微小拉链保持 csg.js 原样（高细分减小）。
    auto edgePoints = [&](const PN& p0, const PN& p1, const Vec3& pn, double pw,
                          std::vector<PN>& out) {
        Vec3 ab = p1.p - p0.p; double L2 = Dot(ab, ab);
        if (L2 < 1e-14) return;
        double L = std::sqrt(L2);
        std::unordered_map<int, double> sel;
        const double kPlanarTol = 1e-4; // 共面阈值：真同平面 T 点距离<1e-5；曲面近共面点>1e-4 被排除
        auto inPlane = [&](const Vec3& q){ return std::fabs(Dot(pn, q) - pw) <= kPlanarTol; };
#ifdef MCSG_FULL_SEARCH
        for (int qi = 0; qi < (int)v.size(); ++qi) {
            const Vec3& q = v[qi];
            double t = Dot(q - p0.p, ab) / L2;
            if (t <= WELD || t >= 1.0 - WELD) continue;
            Vec3 proj = p0.p + ab * t;
            if (Length(proj - q) > WELD) continue;
            if (!inPlane(q)) continue;
            auto e = sel.find(qi);
            if (e == sel.end() || t < e->second) sel[qi] = t;
        }
#else
        int nsteps = (int)std::ceil(L / h);
        if (nsteps < 1) nsteps = 1;
        for (int s = 0; s <= nsteps; ++s) {
            Vec3 ctr = p0.p + ab * ((double)s / nsteps);
            CKey ck = cellOf(ctr);
            for (int gx = -2; gx <= 2; ++gx)
            for (int gy = -2; gy <= 2; ++gy)
            for (int gz = -2; gz <= 2; ++gz) {
                CKey ak = { ck.x+gx, ck.y+gy, ck.z+gz };
                auto it = grid.find(ak);
                if (it == grid.end()) continue;
                for (int qi : it->second) {
                    const Vec3& q = v[qi];
                    double t = Dot(q - p0.p, ab) / L2;
                    if (t <= WELD || t >= 1.0 - WELD) continue;
                    Vec3 proj = p0.p + ab * t;
                    if (Length(proj - q) > WELD) continue;
                    if (!inPlane(q)) continue;
                    auto e = sel.find(qi);
                    if (e == sel.end() || t < e->second) sel[qi] = t;
                }
            }
        }
#endif
        if (sel.empty()) return;
        std::vector<std::pair<double,int> > lst;
        lst.reserve(sel.size());
        for (auto& kv : sel) lst.push_back(std::make_pair(kv.second, kv.first));
        std::sort(lst.begin(), lst.end());
        // 非索引网格在交线处存在大量"几何相同、index 不同"的点副本。按 t 升序后，
        // 相邻点几何距离 < WELD 的视为同一位置，只保留一个，否则 fan 会出退化三角形。
        for (auto& pr : lst) {
            double t = pr.first;
            Vec3 q = v[pr.second];
            if (!out.empty() && Length(q - out.back().p) <= WELD) continue;
            out.push_back({ q, p0.nn + (p1.nn - p0.nn) * t });
        }
    };

    // 一次性处理：把每个三角形的三条边分别插入内部点，得到沿边界走的凸多边形。
    //  - 无内部点（Np==3）：原样输出。
    //  - 有内部点：在三角形内部取中心点 O，对边界序列做中心 fan（O,p_i,p_{i+1}）。
    // 中心 fan 绝对正确、无交叉、不递归、严格收敛，且三条边上的共线点都成为真实边界段，
    // 相邻三角形在共享边处自动细分为相同边界段（严格流形）。仅交线附近少量三角形被细分，
    // 其余三角形保持原样（不会整体加密）。
    std::vector<PN> out;
    out.reserve(v.size() * 2);
    for (int i = 0; i < T0; ++i) {
        PN A = { v[i*3],   n[i*3]   };
        PN B = { v[i*3+1], n[i*3+1] };
        PN C = { v[i*3+2], n[i*3+2] };
        // 当前三角形所在平面（几何叉积），用于判定候选点是否共面
        Vec3 pn = Normalize(Cross(B.p - A.p, C.p - A.p));
        double pw = Dot(pn, A.p);
        std::vector<PN> P;
        P.reserve(12);
        P.push_back(A);
        edgePoints(A, B, pn, pw, P);
        P.push_back(B);
        edgePoints(B, C, pn, pw, P);
        P.push_back(C);
        edgePoints(C, A, pn, pw, P);
        int Np = (int)P.size();
        if (Np < 3) continue;
        if (Np == 3) {
            out.push_back(A); out.push_back(B); out.push_back(C);
        } else {
            PN O = { (A.p + B.p + C.p) * (1.0 / 3.0),
                     Normalize(A.nn + B.nn + C.nn) };
            for (int k = 0; k < Np; ++k)
                out.push_back(O), out.push_back(P[k]), out.push_back(P[(k + 1) % Np]);
        }
    }
    v.clear(); n.clear();
    v.reserve(out.size()); n.reserve(out.size());
    for (size_t i = 0; i < out.size(); ++i) {
        v.push_back(out[i].p); n.push_back(out[i].nn);
    }
}

// 通用布尔：op=0 差集(A-B)、1 并集(A∪B)、2 交集(A∩B)。网格须为同一坐标系下的闭合流形。
inline bool Evaluate(const std::vector<Vec3>& av, const std::vector<Vec3>& an,
                     const std::vector<Vec3>& bv, const std::vector<Vec3>& bn,
                     int op, std::vector<Vec3>& ov, std::vector<Vec3>& on) {
    if (av.size() < 3 || bv.size() < 3) return false;
    std::vector<CSGPoly> pa = makePolys(av, an);
    std::vector<CSGPoly> pb = makePolys(bv, bn);
    if (pa.empty() || pb.empty()) return false;

    CSGNode A(pa);
    CSGNode B(pb);
    std::vector<CSGPoly> result;
    if (op == 1) {            // 并集 A∪B
        A.clipTo(&B);
        B.clipTo(&A);
        B.invert();
        B.clipTo(&A);
        B.invert();
        A.build(B.allPolygons());
        result = A.allPolygons();
    } else if (op == 2) {     // 交集 A∩B
        A.invert();
        B.clipTo(&A);
        B.invert();
        A.clipTo(&B);
        B.clipTo(&A);
        A.build(B.allPolygons());
        A.invert();
        result = A.allPolygons();
    } else {                  // 差集 A-B
        A.invert();
        A.clipTo(&B);
        B.clipTo(&A);
        B.invert();
        B.clipTo(&A);
        B.invert();
        A.build(B.allPolygons());
        A.invert();
        result = A.allPolygons();
    }
    if (result.empty()) return false;
    ov.clear(); on.clear();
    emitPolys(result, ov, on);
#ifndef MCSG_NO_CONFORM
    ConformMesh(ov, on);       // 消除交线 T 型连接，使输出严格流形
#endif
    return ov.size() >= 3;
}

} // namespace mcsg
