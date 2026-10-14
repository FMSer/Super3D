// Script.h : 少儿脚本引擎（ActionScript 3.0 风格语法，不依赖 MFC）
// 语法与 AS3 保持一致，降低学习成本：
//   var speed:Number = 5;                       （类型标注 Number/int/String/Boolean/Array，可省略）
//   function add(a:Number):Number { return a; }   （自定义函数 + return）
//   Math.sin(x)/Math.sqrt(x)/Math.PI ...       （弧度制，与 AS3 一致）
//   trace("hello") / trace(score)              （输出）
//   var arr:Array = [1,2,3]; arr.push(4); arr.length; arr[i]   （数组，引用语义）
//   var b:Ball = new Ball("ball2")             （创建元件，中英文类型名都支持）
//   addEventListener(Event.ENTER_FRAME, onTick);  （每帧事件）
//   if (Key.isDown(Key.LEFT)) ball.x -= speed;   （键盘输入查询）
//   顶层代码只执行一次；定义了 onEnterFrame(e:Event) 后由它逐帧执行。
//   语句以 ; 结尾，块用 {}，注释 // 与块注释 /* */
// 兼容旧写法：中文关键字（定义/如果/否则/循环/当/跳出/真/假/帧）与中文函数
//   （输出/正弦/余弦/正切/开方/绝对值/最小/最大/随机/播放/停止/跳到帧）；
//   未定义函数的旧式脚本仍整段每帧执行（旧行为）。
#pragma once
#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include "Scene3D.h"

// ---------------------------------------------------------------- 值类型
struct Value {
    enum T { V_NUM, V_STR, V_BOOL, V_OBJ, V_ARR, V_NUL };
    T t;
    double num;
    std::wstring str;
    bool b;
    Object3D* obj;
    std::shared_ptr<std::vector<Value> > arr;   // V_ARR：数组（引用语义，与 AS3 一致）
    Value() : t(V_NUL), num(0), b(false), obj(NULL) {}
    static Value MakeNum(double d) { Value v; v.t = V_NUM; v.num = d; return v; }
    static Value MakeStr(const std::wstring& s) { Value v; v.t = V_STR; v.str = s; return v; }
    static Value MakeBool(bool x) { Value v; v.t = V_BOOL; v.b = x; return v; }
    static Value MakeObj(Object3D* o) { Value v; v.t = V_OBJ; v.obj = o; return v; }
    static Value MakeArr() { Value v; v.t = V_ARR; v.arr.reset(new std::vector<Value>()); return v; }
    std::wstring ToString() const;
};

// ---------------------------------------------------------------- 语法树
enum NodeKind {
    NK_NUM, NK_STR, NK_BOOL, NK_NULL, NK_ID, NK_PROP,
    NK_ASSIGN, NK_OP2, NK_NEG, NK_NOT, NK_INC,
    NK_VAR, NK_IF, NK_WHILE, NK_FOR, NK_BLOCK, NK_CALL, NK_BREAK,
    NK_FUNC, NK_RETURN, NK_CONTINUE, NK_MCALL, NK_INDEX, NK_ARRAY, NK_NEW,
    NK_TERNARY,       // cond ? a : b
    NK_DOWHILE,       // do body while(cond)
    NK_SWITCH,        // switch(expr) case...  (a=expr, kids=case 节点, d=default block)
    NK_CASE,          // case 节点：a=值(null=default), b=body block
    NK_TYPEOF,        // typeof x
    NK_THIS,          // this
    NK_DELETE,        // delete x（数组/对象成员）
    NK_FORIN,         // for (var k in obj) body （a=变量名,b=对象,c=body）
    NK_TRY,           // try/catch/finally（a=try块, b=异常变量名, c=catch块, d=finally块）
    NK_THROW          // throw 表达式（a=表达式）
};

struct Node {
    int kind;
    int line;
    int op;            // 运算符 token 编号（NK_OP2 / NK_ASSIGN / NK_INC）
    double num;
    std::wstring str;
    bool bval;         // NK_BOOL：真假；NK_INC：true=前缀 ++i（表达式取新值）
    std::wstring name; // 变量名 / 属性名 / 函数名 / 类型名 / "Math.xxx"
    Node* a, * b, * c, * d;
    std::vector<Node*> kids;

    Node() {
        kind = NK_NUM; line = 0; op = 0; num = 0; bval = false;
        a = b = c = d = NULL;
    }
    ~Node() {
        delete a; delete b; delete c; delete d;
        for (size_t i = 0; i < kids.size(); i++) delete kids[i];
        kids.clear();
    }
};

// ---------------------------------------------------------------- 词法 token
enum TokId {
    T_EOF = 0,
    T_NUM, T_STR, T_ID,
    T_KW_VAR, T_KW_IF, T_KW_ELSE, T_KW_FOR, T_KW_WHILE, T_KW_BREAK,
    T_KW_TRUE, T_KW_FALSE,
    T_KW_FUNC, T_KW_RETURN, T_KW_CONTINUE, T_KW_NEW, T_KW_NULL, T_KW_IMPORT,
    T_KW_DO, T_KW_SWITCH, T_KW_CASE, T_KW_DEFAULT, T_KW_TYPEOF, T_KW_THIS,
    T_KW_TRY, T_KW_CATCH, T_KW_FINALLY, T_KW_THROW, T_KW_IN, T_KW_DELETE,
    T_KW_IS, T_KW_AS, T_KW_INSTANCEOF, T_KW_ARGUMENTS,
    T_KW_PACKAGE, T_KW_CLASS, T_KW_EXTENDS, T_KW_IMPLEMENTS,
    T_KW_PUBLIC, T_KW_PRIVATE, T_KW_PROTECTED, T_KW_STATIC, T_KW_INTERNAL,
    T_KW_FINAL, T_KW_OVERRIDE, T_KW_DYNAMIC, T_KW_CONST, T_KW_INTERFACE,
    T_KW_NAMESPACE, T_KW_SUPER, T_ELLIPSIS,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_ASSIGN, T_PLUSEQ, T_MINUSEQ, T_STAREQ, T_SLASHEQ,
    T_INC, T_DEC,                       // ++ --
    T_EQ, T_NE, T_SEQ, T_SNE, T_LT, T_LE, T_GT, T_GE,   // == != === !== < <= > >=
    T_AND, T_OR, T_NOT,
    T_LP, T_RP, T_LB, T_RB, T_LBRACKET, T_RBRACKET,
    T_COMMA, T_SEMI, T_DOT, T_COLON, T_QUESTION
};

struct Tok {
    int t;
    int line;
    double num;
    std::wstring str;
    Tok() : t(T_EOF), line(0), num(0) {}
};

// 播放控制回调：cmd 1=播放 2=停止 3=跳到帧
typedef void (*PlaybackHook)(int cmd, double arg, void* user);

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    void SetText(const std::wstring& text);
    const std::wstring& GetText() const { return m_text; }
    bool EnsureParsed(std::wstring& err);
    void RunOnce(Scene3D* scene);                // 立即运行一次（改本地值）
    void RunFrame(Scene3D* scene, int frame);    // 播放时逐帧运行（改显示值）
    void RunKeyFrame(Scene3D* scene, int frame); // 关键帧脚本：进入关键帧时执行一次（改显示值，Flash 帧动作语义）
    const std::wstring& GetLog() const { return m_log; }
    bool LastRunError() const { return m_error; }
    void SetPlaybackHook(PlaybackHook hook, void* user) { m_hook = hook; m_hookUser = user; }

    // 键盘输入（播放期间由主窗口喂入当前按下状态）
    void SetKeyDown(int code) { m_keysDown.insert(code); }
    void SetKeyUp(int code) { m_keysDown.erase(code); }
    void ClearKeys() { m_keysDown.clear(); }
    bool IsKeyDown(int code) const { return m_keysDown.find(code) != m_keysDown.end(); }

    // AS3 舞台事件：stage.addEventListener("keyDown"/"keyUp", 函数名) 注册，
    // 由播放窗口在键盘边沿（按下/抬起瞬间）调用 FireStageEvent 分发。
    void FireStageEvent(const std::wstring& type, int code);

private:
    std::wstring m_text;
    Node* m_prog;           // NULL = 未解析或解析失败
    std::wstring m_log;
    PlaybackHook m_hook;
    void* m_hookUser;

    // 解析期派生信息
    std::map<std::wstring, Node*> m_funcs;   // 函数名 → NK_FUNC 节点
    bool m_hasFuncs;                         // 脚本里定义了函数
    bool m_hasFrameHandlers;                 // 定义了 onEnterFrame 或 addEventListener
    bool m_globalsDone;                      // AS3 风格：顶层代码是否已执行过

    // 运行期状态
    Scene3D* m_scene;
    bool m_frameMode;
    int m_curFrame;
    bool m_error;
    bool m_breakFlag;
    bool m_continueFlag;
    bool m_returnFlag;
    Value m_returnVal;
    std::vector<std::map<std::wstring, Value> > m_scopes;  // 作用域栈，[0]=全局
    std::vector<std::wstring> m_frameFns;    // addEventListener 注册的每帧函数
    std::map<std::wstring, std::vector<std::wstring> > m_stageFns;  // stage 事件(如 keyDown) → 回调函数
    int m_keyCode;                          // 最近一次 stage 键盘事件的键码（stage.keyCode）
    std::set<int> m_keysDown;               // 当前按下的键码
    long m_stepCount;
    int m_callDepth;
    Value m_throwVal;                       // throw 表达式的值（供 catch 捕获）
    bool m_throwFlag;                       // 当前错误是否由 throw 引发

    void ResetRun(Scene3D* scene, bool frameMode, int frame);   // 全量重置
    void ResetFrameTemp(Scene3D* scene, int frame);             // 逐帧模式：保留全局变量
    void ScanAST();                                             // 统计函数/每帧回调
    void ScanNode(Node* n);
    Value* FindVar(const std::wstring& name);                   // 由内向外查作用域
    void RunNode(Node* n);
    Value Eval(Node* n);
    void AssignNode(Node* lv, const Value& v, int op);   // op：赋值运算符（T_ASSIGN/T_PLUSEQ...）
    Value CallFunc(const std::wstring& fn, std::vector<Value>& args, int line);  // 内置函数
    Value CallUser(const std::wstring& fn, std::vector<Value>& args, int line);  // 用户函数
    Value CallUserNode(Node* f, std::vector<Value>& args, int line);
    Value GetPropValue(Object3D* o, const std::wstring& p, int line);
    void SetPropValue(Object3D* o, const std::wstring& p, const Value& v, int line);
    void Err(int line, const std::wstring& msg);
};
