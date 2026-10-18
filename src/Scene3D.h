// Super3D - 3D Scene Designer for Kids
// Copyright (C) 2026 FMSer.CN
// Contact: 3781320752@qq.com
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// Scene3D.h : 场景数据模型（不依赖 MFC，可独立编译测试）
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "CoreDefs.h"
#include "Mat4.h"

class Object3D;

// 图层关键帧：插入时刻该层所有根元件的本地变换快照（Flash 式整层驱动动画）
struct LayerKey {
    int frame;
    std::vector<KeyFrame> snaps;   // 与 objs 一一对应：插入时该层各根元件的变换快照
    std::vector<Object3D*> objs;   // 插入时该层根元件指针（播放/保存时须校验有效性）
    std::wstring script;           // 关键帧脚本（进入该关键帧时执行一次；空白关键帧也可挂）
    LayerKey() : frame(0) {}
};

struct Layer {
    std::wstring name;
    bool visible;
    bool locked;
    std::vector<LayerKey> keys;   // 图层关键帧，按 frame 升序
    Layer() : visible(true), locked(false) {}
};

class Object3D;

class Object3D {
public:
    std::wstring name;        // 名称（脚本用它控制）
    int type;                 // PrimType
    bool visible;
    bool locked;              // 锁定：视口中不可选中/拖动（主要用于防止误拖动），需时间轴解锁
    int layer;                // 所属图层索引（仅根元件有意义；组内子元件跟随其根元件所在图层）
    double r, g, b;           // 颜色 0..1
    double alpha;             // 透明度 0..1（1=不透明，<1=半透明混合渲染）

    double params[4];         // 图元参数（按类型解释）
    int segments;             // 细分数量
    std::vector<Vec3> ctrlPts;   // 曲线(PT_SPLINE)的控制点（局部坐标），其他类型为空
    std::wstring text;        // 3D 文本(PT_TEXT3D)：文字内容
    std::wstring font;        // 3D 文本(PT_TEXT3D)：字体名称（空=系统默认）

    // 网格编辑态（Blender 式点/线/面编辑）：非空=编辑固化的网格，GenerateMesh 直接输出；
    // 空=按参数程序化生成。进入编辑即固化，此后改尺寸/细分参数不再重建（用 ClearEditMesh 回退）。
    std::vector<Vec3> editVerts;   // 焊接后顶点表（局部坐标，无重复顶点）
    std::vector<int>  editTris;    // 三角索引表（长度为 3 的倍数，指向 editVerts）
    // 三角剖分产生的斜线（四边形被分成两个三角时的共享对角线），显示/拾取时跳过，
    // 只保留真正的网格线。烘焙时由 BuildEditMeshFromCore 识别（已排序，便于二分查找）。
    std::vector<std::pair<int, int> > editDiagEdges;
    // 面/线独立着色（编辑态"选中面/线设颜色"）。打包色：0=继承物体色；bit31=自定义 0x80RRGGBB
    std::vector<unsigned int> editTriColor;                       // 每三角一个，长度=三角数，空=全继承
    std::vector<std::pair<int, int> > editEdgeColorKey;           // 自定义色边（a<b，已排序）
    std::vector<unsigned int> editEdgeColorVal;                   // 与 key 对应的打包色

    // 本地变换（作者编辑值）
    double x, y, z;
    double rx, ry, rz;        // 度
    double sx, sy, sz;
    // 显示变换（动画插值/脚本逐帧计算的结果）
    double dx, dy, dz;
    double drx, dry, drz;
    double dsx, dsy, dsz;

    Object3D* parent;
    std::vector<Object3D*> children;
    std::vector<KeyFrame> keys;   // 按 frame 升序
    bool selected;

    Object3D();
    ~Object3D();

    Mat4 LocalMatrix() const;             // 本地 → 父空间
    Mat4 WorldMatrix() const;             // 本地 → 世界
    Mat4 DisplayMatrix() const;           // 用显示值
    Mat4 DisplayWorldMatrix() const;

    void SyncDisplayToLocal();
    void CompensateChildRotations(const Mat4& rotOld);   // 组旋转后补偿直接子件自转（成员只公转、自身角度不变）
    void CopyLocalFromDisplay();
    void CopyLocalToKey(KeyFrame& k) const;
    void CopyKeyToLocal(const KeyFrame& k);

    bool HasKeyAt(int frame) const;
    void InsertKey(int frame);            // 用本地值写入/覆盖关键帧
    void InsertKeyFrame(const KeyFrame& k); // 按给定值写入/覆盖关键帧（自动排序去重）
    void RemoveKey(int frame);

    // 生成本地坐标系网格（顶点/法线成对输出）；有编辑网格时直接输出编辑网格
    // coarse=true：为布尔 CSG 输出精简闭合网格（平面/直母线不做高向细分，周向保持光滑），
    // 避免 BSP 树递归过深导致栈溢出、并大幅提速；不影响显示用的精细网格。
    void GenerateMesh(std::vector<Vec3>& verts, std::vector<Vec3>& normals, bool coarse = false) const;
    // 生成基础网格（不含编辑网格覆盖；布尔运算取 A/B 网格与文字/布尔缓存内部使用）
    void GenerateMeshCore(std::vector<Vec3>& verts, std::vector<Vec3>& normals, bool coarse = false) const;
    // ---- Blender 式网格编辑（点/线/面）----
    bool HasEditMesh() const { return !editVerts.empty() && !editTris.empty(); }
    void ClearEditMesh() {
        editVerts.clear(); editTris.clear(); editDiagEdges.clear();
        editTriColor.clear(); editEdgeColorKey.clear(); editEdgeColorVal.clear();
    }
    // 面/线独立色：打包/查询/设置
    static unsigned int PackEditColor(double r, double g, double b);
    static void UnpackEditColor(unsigned int c, double& r, double& g, double& b);
    void SetEditTriColors(const std::vector<int>& tris, unsigned int c);
    void SetEditEdgeColors(const std::vector<std::pair<int, int> >& edges, unsigned int c);
    bool GetEditTriColor(int t, unsigned int& c) const;   // true=该三角有自定义色
    bool GetEditEdgeColor(int a, int b, unsigned int& c) const;  // true=该边有自定义色
    // 判断边 (a,b) 是否为三角剖分斜线
    bool IsDiagEdge(int a, int b) const {
        if (a > b) { int t = a; a = b; b = t; }
        std::pair<int, int> key(a, b);
        return std::binary_search(editDiagEdges.begin(), editDiagEdges.end(), key);
    }
    // 取三角 triIdx 所属"四边形"的所有三角（通常 2 个：共享斜线的一对）；
    // 若该三角不与任何斜线相邻（非四边形剖分），返回自身。面选择/绘制用。
    void GetQuadTris(int triIdx, std::vector<int>& out) const;
    // 该类型是否可进入网格编辑（普通图元，排除曲线/3D文字/布尔/组）
    static bool EditableType(int t);
    // 该类型是否为可参与通用布尔（并/差/交）的闭合实体（GenerateMesh 输出闭合三角网格）
    static bool IsSolidForCSG(int t);
    // 由程序化网格生成编辑网格（空间哈希焊接重复顶点），进入编辑时调用一次
    void BuildEditMeshFromCore();
    // 编辑网格输出（法线按硬/软边重算），供 GenerateMesh 分派
    void EmitEditMesh(std::vector<Vec3>& verts, std::vector<Vec3>& normals) const;
    // 删除指定三角（索引指向 editTris 中的第 i 个三角）
    void DeleteEditTris(const std::vector<int>& tris);
    // 删除指定顶点及其关联三角
    void DeleteEditVerts(const std::vector<int>& verts);
    // Blender 式面挤出（offset=0）：复制选中面为顶盖，沿区域边界生成侧壁。
    // 返回新顶盖顶点下标 newTopVerts、新顶盖三角序号 newTopFaces、选中面平均法线 avgNormal（局部）。
    void ExtrudeEditFaces(const std::vector<int>& tris, std::vector<int>& newTopVerts,
                          std::vector<int>& newTopFaces, Vec3& avgNormal);
    // 布尔运算：把 this(应为 PT_BOOLEAN) 的网格缓存重建为 A op B（op: 0=差集A−B 1=并集A∪B 2=交集A∩B）
    void RebuildBooleanMesh(const Object3D* A, const Object3D* B, int op = 0);
    // 使网格缓存失效（改文字/字体/厚度/字高后调用，下次 GenerateMesh 重建）
    void InvalidateMesh();

    // 图元在自身坐标系中的近似包围盒（不含缩放，显示矩阵会应用缩放），供组拾取用
    static bool PrimitiveLocalBounds(const Object3D* o, Vec3& mn, Vec3& mx);
    // 组（含递归后代）在其自身坐标系中的包围盒（用子元件显示矩阵变换合并）
    bool GroupLocalAABB(Vec3& mn, Vec3& mx) const;
    // 组（含递归后代）在世界坐标系中的包围盒（用子元件 WorldMatrix 变换，与渲染同基准）
    static bool GroupWorldAABB(const Object3D* g, Vec3& mn, Vec3& mx);
    bool HasChildren() const { return !children.empty(); }

    Object3D* CloneDeep(const std::wstring& newName, bool appendSuffix) const;

    static Object3D* CreateByType(int t, const std::wstring& name);
    static const wchar_t* TypeName(int t);
    static bool IsPrimitive(int t) { return t >= PT_BOX && t <= PT_EXTRUDE; }
    bool ChainVisible() const;    // 自己和所有父级都可见（父组隐藏则整组隐藏）

    // 3D 文本网格缓存（Text3D 局部网格，避免每帧 GDI 字形提取）
    mutable std::vector<Vec3> _meshVerts, _meshNorms;
    mutable bool _meshValid;
    mutable int _meshRev;   // 网格内容版本号：_meshVerts 变更时 +1，渲染层显示列表缓存据此重建
    // 3D 文本位图纹理缓存（Renderer 在 GL 线程内生成/更新，ID 用 unsigned 避免 Scene3D 依赖 gl.h）
    mutable unsigned _texID;
    mutable int _texW, _texH;
    mutable bool _texValid;

    // 测量文本渲染位图尺寸（pxH 字号下，紧贴字形像素宽高）；供盒体尺寸与纹理共用
    static bool MeasureTextBitmap(const std::wstring& text, const std::wstring& font,
                                  int pxH, int& wpx, int& hpx);
};

// 渲染层网格缓存失效回调（由 Renderer 注册；对象析构时调用，避免 Scene3D 反向依赖渲染层）
extern void (*g_meshCacheInvalidate)(const Object3D* o);

class Scene3D {
public:
    std::vector<Object3D*> roots;   // 顶层元件
    std::vector<Layer> layers;   // 图层列表，至少1层；顺序=时间轴行序，索引0=最上行=最上层
    int curLayer;                // 当前活动图层索引
    Camera cam;
    int fps;
    int totalFrames;
    bool loopPlay;
    int frame;                       // 当前帧
    std::wstring script;             // 脚本源码
    bool scriptPerFrame;             // 播放时每帧运行脚本
    std::wstring docClass;           // 文档类脚本（整个工程的初始化脚本，Flash 文档类：播放开始时执行一次）
    std::wstring projectPath;

    Scene3D();
    ~Scene3D();
    void Clear();
    void Swap(Scene3D& o);   // 与另一个场景交换内容（用于装载工程，避免双重析构）

    Object3D* AddObject(int t, const std::wstring& name, Object3D* parent);
    void DeleteObject(Object3D* o);
    Object3D* DuplicateObject(Object3D* o);
    bool Ungroup(Object3D* g);            // 打散：把组里的元件移到组外并保留位置
    bool MoveInto(Object3D* o, Object3D* g); // 把已存在的元件移入组（保持世界位置）
    void Reparent(Object3D* o, Object3D* g); // 仅改父级，保持元件全部参数不变（用于转换为元件，g 须与原父同层且无变换）
    bool MoveLayer(Object3D* o, int dir);    // 调整同层父子级顺序（时间轴层级）：-1上移 1下移 2置顶 3置底
    // ---- Flash 式图层 ----
    void EnsureLayers();                        // layers 为空则补一层"图层1"；空名补"图层N"；curLayer 钳制合法
    int  RootLayerOf(const Object3D* o) const;  // 根元件所在图层索引（沿 parent 走到根；索引越界归 0；layers 为空返回 0）
    bool LayerVisible(const Object3D* o) const; // 根所属图层 visible
    bool LayerLocked(const Object3D* o) const;  // 根所属图层 locked
    bool ObjectVisible(const Object3D* o) const;// o 非空 && o->ChainVisible() && LayerVisible(o)
    bool ObjectEditable(const Object3D* o) const;// o 非空 && ObjectVisible(o) && !o->locked && !LayerLocked(o)
    bool SetCurrentLayer(int idx);              // 钳制后设置 curLayer，索引相同或越界返回 false
    bool MoveToLayer(Object3D* o, int layerIdx);// 仅根元件（parent==NULL）可移动；成功返回 true；移动后 SortRootsByLayer
    bool MoveLayerOrder(int layerIdx, int dir); // 图层顺序：-1上移 1下移 2置顶 3置底；成功后 SortRootsByLayer，且"当前图层跟随原图层"
    int  AddLayer();                            // 追加新图层并设为当前，返回其索引；名称"图层N"（N 取不与现有图层重名的最小编号）
    bool DeleteLayer(int idx);                  // 仅当该图层无根元件且 layers.size()>1 时可删除；删除后大于 idx 的根元件 layer 减1，curLayer 钳制
    void SortRootsByLayer();                    // 稳定重排 roots：按图层索引分组（层0的根元件在前…），同层内保持原相对顺序；越界 layer 归 0
    Object3D* FindByName(const std::wstring& n) const;
    void CollectAll(std::vector<Object3D*>& out) const;
    bool ContainsObject(const Object3D* o) const;   // 指针是否仍属于场景（撤销重建后判断野指针用）
    std::wstring AutoName(int t) const;
    int CountType(int t) const;

    // ---- Flash 式元件编辑层级 ----
    Object3D* editRoot;                  // 当前编辑容器（NULL=编辑场景根层；非空=编辑该组内部）
    Object3D* editMeshObj;               // 当前处于网格编辑态的元件（NULL=无；唯一，进入编辑即固化网格）
    // 当前编辑层级的直接子级（editRoot 为 NULL 时返回 roots）
    void EditChildren(std::vector<Object3D*>& out) const;
    bool InsideGroup() const { return editRoot != NULL; }
    // 该元件是否为当前编辑层级的直接子级（只有它可被拖拽/编辑）
    bool IsDirectEditChild(const Object3D* o) const;

    void SetFrame(int f);                 // 设置当前帧并插值显示变换
    void SyncDisplayToLocalAll();
    void CopyLocalFromDisplayAll();

    // ---- Flash 式图层关键帧（整层驱动） ----
    bool LayerHasKeyAt(int layerIdx, int frame) const;
    int  CurrentIntervalKeyFrame(int layerIdx, int frame) const;           // 索引越界或该帧不存在返回 false
    void InsertLayerKey(int layerIdx, int frame);                // 覆盖或新增：记录该层所有根元件当前本地变换快照
    void InsertEmptyLayerKey(int layerIdx, int frame);           // 覆盖或新增空关键帧（该帧该层无元件）；有元件也强制置空该帧
    void RemoveLayerKey(int layerIdx, int frame);                // 删除该图层该帧关键帧；不存在则忽略
    void MoveLayerKey(int layerIdx, int oldFrame, int newFrame); // 拖拽改帧：保留快照值把关键帧移到 newFrame（排序去重；newFrame 已有则覆盖）
    int  LastLayerKeyFrame() const;                              // 所有图层关键帧最远帧；没有任何图层关键帧时返回 -1
    void UpdateLayerKeySnap(Object3D* o, int frame);
    bool AddObjectToLayerKey(int layerIdx, int frame, Object3D* o);
    void ResampleSpline(Object3D* o, int n);   // 曲线控制点数重采样（沿原曲线路径均匀取 n 个点，保持形状）
    void ResampleProfile(Object3D* o, int n);  // 车削/挤出轮廓点数重采样（折线弧长均分，不平滑；挤出按闭合环）
    void AddNewObjectToCurrentKey(Object3D* o);
    bool RemoveObjectFromLayerKey(int layerIdx, int frame, Object3D* o);
    void RemoveObjectFromAllLayerKeys(Object3D* o);
    bool LayerBlankAt(int layerIdx, int frame) const;            // 该层在帧 frame 是否处于空关键帧区间（该帧该层无元件）；无关键帧=编辑态返回 false
    bool LayerContainsAt(int layerIdx, const Object3D* o, int frame) const;  // 该层在帧 frame 是否显示元件 o（取根判断）：无关键帧=编辑态全部显示；有关键帧则只看当前关键帧内容是否含 o

    // 射线拾取（世界坐标），命中最近可见元件
    bool PickObject(const Vec3& rayOrg, const Vec3& rayDir, Object3D*& hit, double& tmin) const;

    // 让图元“站”在地面上（按类型自动抬高 y）
    static void SetStandingY(Object3D* o);
};

// 工具：射线与三角形求交（Moller-Trumbore），t 为沿 d 的参数
bool RayTriangleHit(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c, double& t);
