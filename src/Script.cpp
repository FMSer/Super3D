// Script.cpp : 少儿脚本引擎实现（词法/语法/解释执行，ActionScript 3.0 风格语法）
#include "Script.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

// ---------------------------------------------------------------- 值
std::wstring Value::ToString() const {
    wchar_t buf[64];
    switch (t) {
        case V_NUM:
            swprintf(buf, 64, L"%.6g", num);
            return std::wstring(buf);
        case V_STR: return str;
        case V_BOOL: return b ? L"真" : L"假";
        case V_OBJ: return obj ? obj->name : L"元件";
        case V_ARR: {
            std::wstring r = L"[";
            if (arr) {
                for (size_t i = 0; i < arr->size(); i++) {
                    if (i) r += L", ";
                    r += (*arr)[i].ToString();
                }
            }
            r += L"]";
            return r;
        }
        default: return L"空";
    }
}

static bool IsTrue(const Value& v) {
    switch (v.t) {
        case Value::V_NUM: return v.num != 0.0;
        case Value::V_BOOL: return v.b;
        case Value::V_OBJ: return v.obj != NULL;
        case Value::V_ARR: return v.arr != NULL;
        default: return false;
    }
}

// 宽松查找元件：先精确匹配名字；找不到时按“名字开头相同”匹配第一个；
// 再不行按类型名匹配（如写“球”也能控制“球1”）。方便小学生书写脚本。
static Object3D* FindObj(Scene3D* s, const std::wstring& name) {
    if (!s) return NULL;
    Object3D* o = s->FindByName(name);
    if (o) return o;
    std::vector<Object3D*> all;
    s->CollectAll(all);
    for (size_t i = 0; i < all.size(); i++) {
        const std::wstring& n = all[i]->name;
        if (n.size() >= name.size() && n.compare(0, name.size(), name) == 0)
            return all[i];
    }
    for (size_t i = 0; i < all.size(); i++) {
        if (Object3D::TypeName(all[i]->type) == name)
            return all[i];
    }
    return NULL;
}

// new 类型名 → 图元类型编号：支持中文名（球/立方体/...）与英文别名（Ball/Sphere/Box/...）
static int ResolvePrimType(const std::wstring& n) {
    for (int t = PT_BOX; t <= PT_BOOLEAN; t++) {
        if (Object3D::TypeName(t) == n) return t;
    }
    static const struct { const wchar_t* en; int t; } en[] = {
        { L"Box", PT_BOX },       { L"Cube", PT_BOX },
        { L"Sphere", PT_SPHERE }, { L"Ball", PT_SPHERE },
        { L"Cylinder", PT_CYLINDER }, { L"Cone", PT_CONE }, { L"Torus", PT_TORUS },
        { L"Plane", PT_PLANE },
        { L"Group", PT_NONE },    { L"Object3D", PT_NONE },
        { L"Spline", PT_SPLINE }, { L"Curve", PT_SPLINE },
        { L"Leaf", PT_LEAF },     { L"Text3D", PT_TEXT3D },
        { L"Tube", PT_TUBE },     { L"RTube", PT_RTUBE },
        { L"Boolean", PT_BOOLEAN },
    };
    for (int i = 0; i < (int)(sizeof(en) / sizeof(en[0])); i++)
        if (n == en[i].en) return en[i].t;
    return -1;
}

// ---------------------------------------------------------------- 词法
static bool IsIdStart(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
           ch == L'_' || (ch >= 0x4e00 && ch <= 0x9fff);
}
static bool IsIdPart(wchar_t ch) {
    return IsIdStart(ch) || (ch >= L'0' && ch <= L'9');
}

static std::map<std::wstring, int> BuildKwMap() {
    std::map<std::wstring, int> m;
    m[L"var"] = T_KW_VAR;      m[L"定义"] = T_KW_VAR;
    m[L"if"] = T_KW_IF;        m[L"如果"] = T_KW_IF;
    m[L"else"] = T_KW_ELSE;    m[L"否则"] = T_KW_ELSE;
    m[L"for"] = T_KW_FOR;      m[L"循环"] = T_KW_FOR;
    m[L"while"] = T_KW_WHILE;  m[L"当"] = T_KW_WHILE;
    m[L"do"] = T_KW_DO;        m[L"做"] = T_KW_DO;
    m[L"switch"] = T_KW_SWITCH; m[L"开关"] = T_KW_SWITCH;
    m[L"case"] = T_KW_CASE;    m[L"情况"] = T_KW_CASE;
    m[L"default"] = T_KW_DEFAULT; m[L"默认"] = T_KW_DEFAULT;
    m[L"typeof"] = T_KW_TYPEOF; m[L"类型"] = T_KW_TYPEOF;
    m[L"this"] = T_KW_THIS;    m[L"这"] = T_KW_THIS;
    m[L"try"] = T_KW_TRY;      m[L"尝试"] = T_KW_TRY;
    m[L"catch"] = T_KW_CATCH;  m[L"捕捉"] = T_KW_CATCH;
    m[L"finally"] = T_KW_FINALLY; m[L"最后"] = T_KW_FINALLY;
    m[L"throw"] = T_KW_THROW;  m[L"抛出"] = T_KW_THROW;
    m[L"in"] = T_KW_IN;        m[L"在"] = T_KW_IN;
    m[L"delete"] = T_KW_DELETE; m[L"删除"] = T_KW_DELETE;
    m[L"is"] = T_KW_IS;        m[L"是"] = T_KW_IS;
    m[L"as"] = T_KW_AS;        m[L"作为"] = T_KW_AS;
    m[L"instanceof"] = T_KW_INSTANCEOF; m[L"实例"] = T_KW_INSTANCEOF;
    m[L"arguments"] = T_KW_ARGUMENTS;   m[L"参数"] = T_KW_ARGUMENTS;
    // AS3 文档类语法：package / class / 访问修饰符 / 常量 / 继承 / super
    m[L"package"] = T_KW_PACKAGE;   m[L"包"] = T_KW_PACKAGE;
    m[L"class"] = T_KW_CLASS;       m[L"类"] = T_KW_CLASS;
    m[L"extends"] = T_KW_EXTENDS;   m[L"继承"] = T_KW_EXTENDS;
    m[L"implements"] = T_KW_IMPLEMENTS; m[L"实现"] = T_KW_IMPLEMENTS;
    m[L"public"] = T_KW_PUBLIC;     m[L"公有"] = T_KW_PUBLIC;
    m[L"private"] = T_KW_PRIVATE;   m[L"私有"] = T_KW_PRIVATE;
    m[L"protected"] = T_KW_PROTECTED; m[L"保护"] = T_KW_PROTECTED;
    m[L"static"] = T_KW_STATIC;     m[L"静态"] = T_KW_STATIC;
    m[L"internal"] = T_KW_INTERNAL;
    m[L"final"] = T_KW_FINAL;
    m[L"override"] = T_KW_OVERRIDE; m[L"重写"] = T_KW_OVERRIDE;
    m[L"dynamic"] = T_KW_DYNAMIC;
    m[L"const"] = T_KW_CONST;       m[L"常量"] = T_KW_CONST;
    m[L"interface"] = T_KW_INTERFACE; m[L"接口"] = T_KW_INTERFACE;
    m[L"namespace"] = T_KW_NAMESPACE; m[L"命名空间"] = T_KW_NAMESPACE;
    m[L"super"] = T_KW_SUPER;       m[L"父类"] = T_KW_SUPER;
    m[L"break"] = T_KW_BREAK;  m[L"跳出"] = T_KW_BREAK;
    m[L"continue"] = T_KW_CONTINUE; m[L"继续"] = T_KW_CONTINUE;
    m[L"function"] = T_KW_FUNC; m[L"函数"] = T_KW_FUNC;
    m[L"return"] = T_KW_RETURN; m[L"返回"] = T_KW_RETURN;
    m[L"new"] = T_KW_NEW;      m[L"新建"] = T_KW_NEW;
    m[L"null"] = T_KW_NULL;    m[L"空"] = T_KW_NULL;
    m[L"import"] = T_KW_IMPORT; m[L"导入"] = T_KW_IMPORT;
    m[L"true"] = T_KW_TRUE;    m[L"真"] = T_KW_TRUE;
    m[L"false"] = T_KW_FALSE;  m[L"假"] = T_KW_FALSE;
    return m;
}

static const std::map<std::wstring, int>& KwMap() {
    static std::map<std::wstring, int> m = BuildKwMap();
    return m;
}

// 词法分析，返回是否成功；错误写入 err
static bool Lex(const std::wstring& src, std::vector<Tok>& toks, std::wstring& err) {
    toks.clear();
    size_t i = 0;
    int line = 1;
    const size_t n = src.size();
    while (i < n) {
        wchar_t ch = src[i];
        if (ch == L'\r' || ch == L'\n') {
            if (ch == L'\n') line++;
            i++;
            continue;
        }
        if (ch == L' ' || ch == L'\t') { i++; continue; }
        // 注释
        if (ch == L'/' && i + 1 < n && src[i + 1] == L'/') {
            while (i < n && src[i] != L'\n') i++;
            continue;
        }
        if (ch == L'/' && i + 1 < n && src[i + 1] == L'*') {
            i += 2;
            while (i + 1 < n && !(src[i] == L'*' && src[i + 1] == L'/')) {
                if (src[i] == L'\n') line++;
                i++;
            }
            if (i + 1 >= n) { wchar_t b[128]; swprintf(b, 128, L"第%d行：注释没有结束", line); err = b; return false; }
            i += 2;
            continue;
        }
        // 数字
        if ((ch >= L'0' && ch <= L'9') ||
            (ch == L'.' && i + 1 < n && src[i + 1] >= L'0' && src[i + 1] <= L'9')) {
            size_t start = i;
            bool dot = false;
            while (i < n) {
                if (src[i] >= L'0' && src[i] <= L'9') { i++; }
                else if (src[i] == L'.' && !dot && i + 1 < n && src[i + 1] >= L'0' && src[i + 1] <= L'9') { dot = true; i++; }
                else break;
            }
            Tok tk; tk.t = T_NUM; tk.line = line;
            tk.str = src.substr(start, i - start);
            tk.num = wcstod(tk.str.c_str(), NULL);
            toks.push_back(tk);
            continue;
        }
        // 字符串（双引号或单引号，与 AS3 一致）
        if (ch == L'"' || ch == L'\'') {
            wchar_t quote = ch;
            i++;
            std::wstring s;
            bool closed = false;
            while (i < n) {
                wchar_t c = src[i];
                if (c == L'\\' && i + 1 < n) {
                    wchar_t e = src[i + 1];
                    if (e == L'n') { s += L'\n'; i += 2; continue; }
                    if (e == L't') { s += L'\t'; i += 2; continue; }
                    if (e == L'"') { s += L'"'; i += 2; continue; }
                    if (e == L'\'') { s += L'\''; i += 2; continue; }
                    if (e == L'\\') { s += L'\\'; i += 2; continue; }
                    s += c; i++;
                    continue;
                }
                if (c == quote) { closed = true; i++; break; }
                if (c == L'\n') line++;
                s += c;
                i++;
            }
            if (!closed) { wchar_t b[128]; swprintf(b, 128, L"第%d行：字符串没有结束", line); err = b; return false; }
            Tok tk; tk.t = T_STR; tk.line = line; tk.str = s;
            toks.push_back(tk);
            continue;
        }
        // 标识符 / 关键字
        if (IsIdStart(ch)) {
            size_t start = i;
            while (i < n && IsIdPart(src[i])) i++;
            std::wstring word = src.substr(start, i - start);
            Tok tk; tk.t = T_ID; tk.line = line; tk.str = word;
            std::map<std::wstring, int>::const_iterator it = KwMap().find(word);
            if (it != KwMap().end()) tk.t = it->second;
            toks.push_back(tk);
            continue;
        }
        // 运算符（先长后短）
        struct Op2 { const wchar_t* s; int t; };
        static const Op2 ops2[] = {
            { L"===", T_SEQ }, { L"!==", T_SNE },
            { L"++", T_INC }, { L"--", T_DEC },
            { L"+=", T_PLUSEQ }, { L"-=", T_MINUSEQ }, { L"*=", T_STAREQ }, { L"/=", T_SLASHEQ },
            { L"==", T_EQ }, { L"!=", T_NE }, { L"<=", T_LE }, { L">=", T_GE },
            { L"&&", T_AND }, { L"||", T_OR }
        };
        bool matched = false;
        for (int k = 0; k < 14; k++) {
            const wchar_t* s = ops2[k].s;
            size_t len = wcslen(s);
            if (i + len <= n && wcsncmp(src.c_str() + i, s, len) == 0) {
                Tok tk; tk.t = ops2[k].t; tk.line = line;
                toks.push_back(tk);
                i += len;
                matched = true;
                break;
            }
        }
        if (matched) continue;
        // AS3 剩余参数 ...arg
        if (ch == L'.' && i + 2 < n && src[i + 1] == L'.' && src[i + 2] == L'.') {
            Tok tk; tk.t = T_ELLIPSIS; tk.line = line;
            toks.push_back(tk);
            i += 3;
            continue;
        }
        int single = -1;
        switch (ch) {
            case L'+': single = T_PLUS; break;
            case L'-': single = T_MINUS; break;
            case L'*': single = T_STAR; break;
            case L'/': single = T_SLASH; break;
            case L'%': single = T_PERCENT; break;
            case L'=': single = T_ASSIGN; break;
            case L'<': single = T_LT; break;
            case L'>': single = T_GT; break;
            case L'!': single = T_NOT; break;
            case L'(': single = T_LP; break;
            case L')': single = T_RP; break;
            case L'{': single = T_LB; break;
            case L'}': single = T_RB; break;
            case L'[': single = T_LBRACKET; break;
            case L']': single = T_RBRACKET; break;
            case L',': single = T_COMMA; break;
            case L';': single = T_SEMI; break;
            case L'.': single = T_DOT; break;
            case L':': single = T_COLON; break;
            case L'?': single = T_QUESTION; break;
        }
        if (single >= 0) {
            Tok tk; tk.t = single; tk.line = line;
            toks.push_back(tk);
            i++;
            continue;
        }
        wchar_t b[128];
        swprintf(b, 128, L"第%d行：无法识别的字符 '%c'", line, ch);
        err = b;
        return false;
    }
    Tok eofTok; eofTok.t = T_EOF; eofTok.line = line;
    toks.push_back(eofTok);
    return true;
}

// ---------------------------------------------------------------- 语法分析
class Parser {
public:
    Parser(const std::vector<Tok>& toks) : m_toks(toks), m_i(0), m_err(false), m_lastOp(0) {}

    Node* ParseProgram() {
        Node* n = new Node();
        n->kind = NK_BLOCK;
        while (!Is(T_EOF) && !m_err) {
            Node* s = ParseStatement();
            if (s) n->kids.push_back(s);
        }
        if (m_err) { delete n; return NULL; }
        return n;
    }

    bool GetError(std::wstring& msg) const { msg = m_errMsg; return m_err; }

private:
    const std::vector<Tok>& m_toks;
    size_t m_i;
    bool m_err;
    std::wstring m_errMsg;
    int m_lastOp;

    const Tok& Cur() const { return m_toks[m_i]; }
    void Next() { if (m_i + 1 < m_toks.size()) m_i++; }
    bool Is(int t) const { return Cur().t == t; }
    bool Match(int t) { if (Is(t)) { Next(); return true; } return false; }
    void ErrHere(const std::wstring& m) {
        if (m_err) return;
        m_err = true;
        wchar_t b[256];
        swprintf(b, 256, L"第%d行：%s", Cur().line, m.c_str());
        m_errMsg = b;
    }
    bool Expect(int t, const std::wstring& m) {
        if (Is(t)) { Next(); return true; }
        ErrHere(m);
        return false;
    }

    Node* New(int kind, int line) {
        Node* n = new Node();
        n->kind = kind;
        n->line = line;
        return n;
    }

    Node* ParseStatement() {
        int line = Cur().line;
        if (Is(T_SEMI)) { Next(); return New(NK_BLOCK, line); }   // 空语句
        if (Is(T_KW_IMPORT)) {                                    // AS3 的 import：接受但忽略
            Next();
            while (!Is(T_SEMI) && !Is(T_EOF)) Next();
            Match(T_SEMI);
            return New(NK_BLOCK, line);
        }
        // AS3 访问修饰符 / 其他类修饰符：跳过后解析真正的语句（function/var/const/class）
        if (Is(T_KW_PUBLIC) || Is(T_KW_PRIVATE) || Is(T_KW_PROTECTED) ||
            Is(T_KW_STATIC) || Is(T_KW_INTERNAL) || Is(T_KW_FINAL) ||
            Is(T_KW_OVERRIDE) || Is(T_KW_DYNAMIC)) {
            Next();
            return ParseStatement();
        }
        // super() 构造调用（AS3 文档类）：接受但忽略
        if (Is(T_KW_SUPER)) {
            Next();
            while (!Is(T_SEMI) && !Is(T_EOF) && !Is(T_RB)) Next();
            Match(T_SEMI);
            return New(NK_BLOCK, line);
        }
        // package [包名] { ... }：包块，内部按普通语句解析（import/class/顶层语句）
        if (Is(T_KW_PACKAGE)) {
            Next();
            while (Is(T_ID) || Is(T_DOT)) Next();                 // 可选的包名（含点号）
            Node* blk = New(NK_BLOCK, line);
            if (!Expect(T_LB, L"package 缺少 '{'")) return NULL;
            while (!Is(T_RB) && !Is(T_EOF) && !m_err) {
                Node* s = ParseStatement();
                if (s) blk->kids.push_back(s);
            }
            Match(T_RB);
            return blk;
        }
        // class 类名 [extends 基类] [implements 接口] { ... }：类定义
        if (Is(T_KW_CLASS)) {
            Next();
            if (!Expect(T_ID, L"类名")) return NULL;
            std::wstring clsName = m_toks[m_i - 1].str;
            if (Match(T_KW_EXTENDS)) {                            // extends 基类（可带点号链）
                if (!Expect(T_ID, L"基类名")) return NULL;
                while (Match(T_DOT)) { if (!Expect(T_ID, L"基类名")) return NULL; }
            }
            if (Match(T_KW_IMPLEMENTS)) {                         // implements 接口列表
                do { if (!Expect(T_ID, L"接口名")) return NULL; } while (Match(T_COMMA));
            }
            Node* blk = New(NK_BLOCK, line);
            if (!Expect(T_LB, L"class 缺少 '{'")) return NULL;
            while (!Is(T_RB) && !Is(T_EOF) && !m_err) {
                Node* s = ParseStatement();
                if (s) {
                    // 构造函数：与类同名的方法 → bval 标记，顶层执行一次（Flash 文档类构造）
                    if (s->kind == NK_FUNC && s->name == clsName) s->bval = true;
                    blk->kids.push_back(s);
                }
            }
            Match(T_RB);
            return blk;
        }
        if (Is(T_KW_FUNC)) {                                      // function 名(参数:类型):返回类型 { }
            Next();
            if (!Expect(T_ID, L"函数名")) return NULL;
            Node* n = New(NK_FUNC, line);
            n->name = m_toks[m_i - 1].str;
            if (!Expect(T_LP, L"缺少 '('")) { delete n; return NULL; }
            if (!Is(T_RP)) {
                for (;;) {
                    if (Match(T_ELLIPSIS)) {                      // AS3 剩余参数 ...args：当普通参数（值为数组语义由调用方填充，此处简化）
                        if (!Expect(T_ID, L"参数名")) { delete n; return NULL; }
                        Node* p = New(NK_ID, line);
                        p->name = m_toks[m_i - 1].str;
                        n->kids.push_back(p);
                    } else {
                        if (!Expect(T_ID, L"参数名")) { delete n; return NULL; }
                        Node* p = New(NK_ID, line);
                        p->name = m_toks[m_i - 1].str;
                        n->kids.push_back(p);
                        if (Match(T_COLON)) {                     // 参数类型标注（接受并忽略）
                            if (!Expect(T_ID, L"参数类型")) { delete n; return NULL; }
                        }
                    }
                    if (!Match(T_COMMA)) break;
                }
            }
            if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            if (Match(T_COLON)) {                                 // 返回类型标注（接受并忽略）
                if (!Expect(T_ID, L"返回类型")) { delete n; return NULL; }
            }
            n->a = ParseStatement();
            if (!n->a) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_RETURN)) {
            Next();
            Node* n = New(NK_RETURN, line);
            if (!Is(T_SEMI) && !Is(T_RB) && !Is(T_EOF)) {
                n->a = ParseExpr();
                if (!n->a) { delete n; return NULL; }
            }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_VAR) || Is(T_KW_CONST)) {                    // var/const 声明（const 同 var 处理）
            Next();
            Node* blk = New(NK_BLOCK, line);                      // 多变量声明：var a:int = 1, b = 2;
            for (;;) {
                if (!Expect(T_ID, L"变量名")) { delete blk; return NULL; }
                Node* n = New(NK_VAR, line);
                n->name = m_toks[m_i - 1].str;
                if (Match(T_COLON)) {                             // 类型标注（接受并忽略）
                    if (!Expect(T_ID, L"变量类型")) { delete blk; return NULL; }
                }
                if (Match(T_ASSIGN)) {
                    n->a = ParseExpr();
                    if (!n->a) { delete blk; return NULL; }
                }
                blk->kids.push_back(n);
                if (!Match(T_COMMA)) break;
            }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete blk; return NULL; }
            return blk;
        }
        if (Is(T_KW_IF)) {
            Next();
            Node* n = New(NK_IF, line);
            bool hasParen = Match(T_LP);
            n->a = ParseExpr();
            if (!n->a) { delete n; return NULL; }
            if (hasParen) {
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            }
            n->b = ParseStatement();
            if (!n->b) { delete n; return NULL; }
            if (Match(T_KW_ELSE)) {
                n->c = ParseStatement();
                if (!n->c) { delete n; return NULL; }
            }
            return n;
        }
        if (Is(T_KW_WHILE)) {
            Next();
            Node* n = New(NK_WHILE, line);
            bool hasParen = Match(T_LP);
            n->a = ParseExpr();
            if (!n->a) { delete n; return NULL; }
            if (hasParen) {
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            }
            n->b = ParseStatement();
            if (!n->b) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_DO)) {                        // do { ... } while (条件);
            Next();
            Node* n = New(NK_DOWHILE, line);
            n->a = ParseStatement();
            if (!n->a) { delete n; return NULL; }
            if (!Expect(T_KW_WHILE, L"缺少 while")) { delete n; return NULL; }
            bool hasParen2 = Match(T_LP);
            n->b = ParseExpr();
            if (!n->b) { delete n; return NULL; }
            if (hasParen2) {
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_SWITCH)) {                    // switch (x) { case 1: ... break; default: ... }
            Next();
            Node* n = New(NK_SWITCH, line);
            bool hasParen3 = Match(T_LP);
            n->a = ParseExpr();
            if (!n->a) { delete n; return NULL; }
            if (hasParen3) {
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            }
            if (!Expect(T_LB, L"缺少 '{'")) { delete n; return NULL; }
            Node* def = NULL;
            while (!Is(T_RB) && !Is(T_EOF) && !m_err) {
                if (Is(T_KW_CASE) || Is(T_KW_DEFAULT)) {
                    bool isDef = Is(T_KW_DEFAULT);
                    Next();
                    Node* cs = New(NK_CASE, line);
                    if (isDef) cs->a = NULL;
                    else {
                        cs->a = ParseExpr();
                        if (!cs->a) { delete cs; delete n; return NULL; }
                    }
                    if (!Expect(T_COLON, L"缺少 ':'")) { delete cs; delete n; return NULL; }
                    Node* blk = New(NK_BLOCK, line);
                    while (!Is(T_KW_CASE) && !Is(T_KW_DEFAULT) && !Is(T_RB) && !Is(T_EOF) && !m_err) {
                        Node* st = ParseStatement();
                        if (st) blk->kids.push_back(st);
                    }
                    cs->b = blk;
                    if (isDef) { if (def) delete def; def = cs; }
                    else n->kids.push_back(cs);
                } else {
                    ErrHere(L"switch 里需要 case 或 default");
                    delete n;
                    return NULL;
                }
            }
            if (!Expect(T_RB, L"缺少 '}'")) { delete n; return NULL; }
            n->d = def;
            return n;
        }
        if (Is(T_KW_TRY)) {                       // try { } catch (e) { } finally { }
            Next();
            Node* n = New(NK_TRY, line);
            n->a = ParseStatement();
            if (!n->a) { delete n; return NULL; }
            if (Is(T_KW_CATCH)) {
                Next();
                if (!Expect(T_LP, L"缺少 '('")) { delete n; return NULL; }
                if (!Expect(T_ID, L"异常变量名")) { delete n; return NULL; }
                n->b = New(NK_ID, line);
                n->b->name = m_toks[m_i - 1].str;
                if (Match(T_COLON)) {
                    if (!Expect(T_ID, L"异常类型")) { delete n; return NULL; }
                }
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
                n->c = ParseStatement();
                if (!n->c) { delete n; return NULL; }
            }
            if (Is(T_KW_FINALLY)) {
                Next();
                n->d = ParseStatement();
                if (!n->d) { delete n; return NULL; }
            }
            if (!n->c && !n->d) { ErrHere(L"try 后需要 catch 或 finally"); delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_THROW)) {                     // throw "错误信息";
            Next();
            Node* n = New(NK_THROW, line);
            n->a = ParseExpr();
            if (!n->a) { delete n; return NULL; }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_FOR)) {
            Next();
            // for-in 探测：for (var k in obj) 或 for (k in obj)
            size_t saveFor = m_i;
            bool fp = Match(T_LP);
            bool isForIn = false;
            std::wstring itVar;
            if (Match(T_KW_VAR)) {
                if (Is(T_ID)) { itVar = Cur().str; Next(); if (Match(T_KW_IN)) isForIn = true; }
            } else if (Is(T_ID)) {
                itVar = Cur().str; Next(); if (Match(T_KW_IN)) isForIn = true;
            }
            if (isForIn) {
                Node* n = New(NK_FORIN, line);
                n->name = itVar;
                n->b = ParseExpr();
                if (!n->b) { delete n; return NULL; }
                if (fp) {
                    if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
                }
                n->c = ParseStatement();
                if (!n->c) { delete n; return NULL; }
                return n;
            }
            m_i = saveFor;   // 回退：普通 for
            Node* n = New(NK_FOR, line);
            bool hasParen = Match(T_LP);
            // init
            if (!Is(T_SEMI)) {
                if (Is(T_KW_VAR)) {
                    Next();
                    if (!Expect(T_ID, L"变量名")) { delete n; return NULL; }
                    n->a = New(NK_VAR, line);
                    n->a->name = m_toks[m_i - 1].str;
                    if (Match(T_COLON)) {                         // for (var i:int = 0; ...)
                        if (!Expect(T_ID, L"变量类型")) { delete n; return NULL; }
                    }
                    if (Match(T_ASSIGN)) {
                        n->a->a = ParseExpr();
                        if (!n->a->a) { delete n; return NULL; }
                    }
                } else {
                    Node* e = ParseExpr();
                    if (!e) { delete n; return NULL; }
                    if (e->kind == NK_ID || e->kind == NK_PROP) {
                        if (MatchAssignOp()) {
                            Node* an = New(NK_ASSIGN, line);
                            an->op = m_lastOp;
                            an->a = e;
                            an->b = ParseExpr();
                            if (!an->b) { delete an; delete n; return NULL; }
                            n->a = an;
                        } else { delete e; }
                    } else { delete e; }
                }
            }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            if (!Is(T_SEMI)) {
                n->b = ParseExpr();
                if (!n->b) { delete n; return NULL; }
            }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            if (hasParen ? !Is(T_RP) : !Is(T_LB)) {
                Node* e = ParseExpr();
                if (!e) { delete n; return NULL; }
                if (e->kind == NK_ID || e->kind == NK_PROP) {
                    if (MatchAssignOp()) {
                        Node* an = New(NK_ASSIGN, line);
                        an->op = m_lastOp;
                        an->a = e;
                        an->b = ParseExpr();
                        if (!an->b) { delete an; delete n; return NULL; }
                        n->c = an;
                    } else { delete e; }
                } else if (e->kind == NK_INC) {
                    // 增量支持 i++ / i--
                    n->c = e;
                } else { delete e; }
            }
            if (hasParen) {
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            }
            n->d = ParseStatement();
            if (!n->d) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_BREAK)) {
            Next();
            Node* n = New(NK_BREAK, line);
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            return n;
        }
        if (Is(T_KW_CONTINUE)) {
            Next();
            Node* n = New(NK_CONTINUE, line);
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete n; return NULL; }
            return n;
        }
        if (Is(T_LB)) {
            Next();
            Node* n = New(NK_BLOCK, line);
            while (!Is(T_RB) && !Is(T_EOF) && !m_err) {
                Node* s = ParseStatement();
                if (s) n->kids.push_back(s);
            }
            if (!Expect(T_RB, L"缺少 '}'")) { delete n; return NULL; }
            return n;
        }
        // 表达式语句（赋值或函数调用）
        Node* e = ParseExpr();
        if (!e) return NULL;
        if ((e->kind == NK_ID || e->kind == NK_PROP || e->kind == NK_INDEX) && MatchAssignOp()) {
            Node* an = New(NK_ASSIGN, line);
            an->op = m_lastOp;
            an->a = e;
            an->b = ParseExpr();
            if (!an->b) { delete an; return NULL; }
            if (!Expect(T_SEMI, L"缺少 ';'")) { delete an; return NULL; }
            return an;
        }
        if (!Expect(T_SEMI, L"缺少 ';'")) { delete e; return NULL; }
        return e;
    }

    bool MatchAssignOp() {
        if (Is(T_ASSIGN) || Is(T_PLUSEQ) || Is(T_MINUSEQ) || Is(T_STAREQ) || Is(T_SLASHEQ)) {
            m_lastOp = Cur().t;
            Next();
            return true;
        }
        return false;
    }

    Node* ParseExpr() { return ParseAssign(); }

    // a = b = c（右结合）
    Node* ParseAssign() {
        Node* l = ParseTernary();
        if (!l) return NULL;
        while (Is(T_ASSIGN) || Is(T_PLUSEQ) || Is(T_MINUSEQ) || Is(T_STAREQ) || Is(T_SLASHEQ)) {
            int line = Cur().line;
            int op = Cur().t;
            Next();
            Node* r = ParseAssign();          // 右结合
            if (!r) { delete l; return NULL; }
            Node* n = New(NK_ASSIGN, line);
            n->op = op; n->a = l; n->b = r;
            l = n;
        }
        return l;
    }

    Node* ParseTernary() {
        Node* c = ParseOr();
        if (!c) return NULL;
        if (Is(T_QUESTION)) {
            int line = Cur().line;
            Next();
            Node* a = ParseOr();
            if (!a) { delete c; return NULL; }
            if (!Expect(T_COLON, L"缺少 ':'")) { delete c; delete a; return NULL; }
            Node* b = ParseTernary();          // 右结合：a ? b : c ? d : e
            if (!b) { delete c; delete a; return NULL; }
            Node* n = New(NK_TERNARY, line);
            n->a = c; n->b = a; n->c = b;
            return n;
        }
        return c;
    }

    Node* ParseOr() {
        Node* a = ParseAnd();
        if (!a) return NULL;
        while (Is(T_OR)) {
            int line = Cur().line;
            Next();
            Node* b = ParseAnd();
            if (!b) { delete a; return NULL; }
            Node* n = New(NK_OP2, line);
            n->op = T_OR;
            n->a = a; n->b = b;
            a = n;
        }
        return a;
    }

    Node* ParseAnd() {
        Node* a = ParseEq();
        if (!a) return NULL;
        while (Is(T_AND)) {
            int line = Cur().line;
            Next();
            Node* b = ParseEq();
            if (!b) { delete a; return NULL; }
            Node* n = New(NK_OP2, line);
            n->op = T_AND;
            n->a = a; n->b = b;
            a = n;
        }
        return a;
    }

    Node* ParseEq() {
        Node* a = ParseRel();
        if (!a) return NULL;
        while (Is(T_EQ) || Is(T_NE)) {
            int line = Cur().line;
            int op = Cur().t;
            Next();
            Node* b = ParseRel();
            if (!b) { delete a; return NULL; }
            Node* n = New(NK_OP2, line);
            n->op = op;
            n->a = a; n->b = b;
            a = n;
        }
        return a;
    }

    Node* ParseRel() {
        Node* a = ParseAdd();
        if (!a) return NULL;
        while (Is(T_LT) || Is(T_LE) || Is(T_GT) || Is(T_GE) || Is(T_KW_IS) || Is(T_KW_AS) || Is(T_KW_INSTANCEOF)) {
            int line = Cur().line;
            int op = Cur().t;
            Next();
            Node* b = ParseAdd();
            if (!b) { delete a; return NULL; }
            Node* n = New(NK_OP2, line);
            n->op = op;
            n->a = a; n->b = b;
            a = n;
        }
        return a;
    }

    Node* ParseAdd() {
        Node* a = ParseMul();
        if (!a) return NULL;
        while (Is(T_PLUS) || Is(T_MINUS)) {
            int line = Cur().line;
            int op = Cur().t;
            Next();
            Node* b = ParseMul();
            if (!b) { delete a; return NULL; }
            Node* n = New(NK_OP2, line);
            n->op = op;
            n->a = a; n->b = b;
            a = n;
        }
        return a;
    }

    Node* ParseMul() {
        Node* a = ParseUnary();
        if (!a) return NULL;
        while (Is(T_STAR) || Is(T_SLASH) || Is(T_PERCENT)) {
            int line = Cur().line;
            int op = Cur().t;
            Next();
            Node* b = ParseUnary();
            if (!b) { delete a; return NULL; }
            Node* n = New(NK_OP2, line);
            n->op = op;
            n->a = a; n->b = b;
            a = n;
        }
        return a;
    }

    Node* ParseUnary() {
        int line = Cur().line;
        if (Is(T_NOT)) {
            Next();
            Node* a = ParseUnary();
            if (!a) return NULL;
            Node* n = New(NK_NOT, line);
            n->a = a;
            return n;
        }
        if (Is(T_MINUS)) {
            Next();
            Node* a = ParseUnary();
            if (!a) return NULL;
            Node* n = New(NK_NEG, line);
            n->a = a;
            return n;
        }
        if (Is(T_INC) || Is(T_DEC)) {         // 前缀自增/自减：++i / --i
            int op = Cur().t;
            Next();
            Node* a = ParseUnary();
            if (!a) return NULL;
            Node* n = New(NK_INC, line);
            n->op = op;
            n->a = a;
            n->bval = true;                    // 前缀：表达式值为自增后的新值
            return n;
        }
        if (Is(T_KW_TYPEOF)) {                // typeof x
            Next();
            Node* a = ParseUnary();
            if (!a) return NULL;
            Node* n = New(NK_TYPEOF, line);
            n->a = a;
            return n;
        }
        if (Is(T_KW_DELETE)) {                // delete arr[i]
            Next();
            Node* a = ParseUnary();
            if (!a) return NULL;
            Node* n = New(NK_DELETE, line);
            n->a = a;
            return n;
        }
        return ParsePostfix();
    }

    Node* ParsePostfix() {
        Node* p = ParsePrimary();
        if (!p) return NULL;
        for (;;) {
            if (Is(T_DOT)) {
                int line = Cur().line;
                Next();
                if (!Expect(T_ID, L"属性名")) { delete p; return NULL; }
                std::wstring nm = m_toks[m_i - 1].str;
                if (Is(T_LP)) {               // 方法调用：arr.push(x) / s.charAt(0)
                    Next();
                    Node* n = New(NK_MCALL, line);
                    n->name = nm;
                    n->a = p;
                    if (!Is(T_RP)) {
                        for (;;) {
                            Node* e = ParseExpr();
                            if (!e) { delete n; return NULL; }
                            n->kids.push_back(e);
                            if (!Match(T_COMMA)) break;
                        }
                    }
                    if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
                    p = n;
                } else {
                    Node* n = New(NK_PROP, line);
                    n->name = nm;
                    n->a = p;
                    p = n;
                }
                continue;
            }
            if (Is(T_LBRACKET)) {             // 下标：arr[i] / arr[i+1] / s[0]
                int line = Cur().line;
                Next();
                Node* n = New(NK_INDEX, line);
                n->a = p;
                n->b = ParseExpr();
                if (!n->b) { delete n; return NULL; }
                if (!Expect(T_RBRACKET, L"缺少 ']'")) { delete n; return NULL; }
                p = n;
                continue;
            }
            break;
        }
        // 后置自增/自减：i++ 元件.ry--
        while (Is(T_INC) || Is(T_DEC)) {
            int line = Cur().line;
            Node* n = New(NK_INC, line);
            n->op = Cur().t;
            n->a = p;
            p = n;
            Next();
        }
        return p;
    }

    Node* ParsePrimary() {
        int line = Cur().line;
        if (Is(T_NUM)) {
            Node* n = New(NK_NUM, line);
            n->num = Cur().num;
            Next();
            return n;
        }
        if (Is(T_STR)) {
            Node* n = New(NK_STR, line);
            n->str = Cur().str;
            Next();
            return n;
        }
        if (Is(T_KW_TRUE) || Is(T_KW_FALSE)) {
            Node* n = New(NK_BOOL, line);
            n->bval = (Cur().t == T_KW_TRUE);
            Next();
            return n;
        }
        if (Is(T_KW_NULL)) {
            Next();
            return New(NK_NULL, line);
        }
        if (Is(T_KW_THIS)) {                  // this（当前脚本所属元件）
            Next();
            return New(NK_THIS, line);
        }
        if (Is(T_KW_NEW)) {                   // new 球("名字") / new Ball("名字")
            Next();
            if (!Expect(T_ID, L"类型名")) return NULL;
            Node* n = New(NK_NEW, line);
            n->name = m_toks[m_i - 1].str;
            if (Match(T_LP)) {
                if (!Is(T_RP)) {
                    n->a = ParseExpr();
                    if (!n->a) { delete n; return NULL; }
                }
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
            }
            return n;
        }
        if (Is(T_LBRACKET)) {                 // 数组字面量 [1, 2, 3]
            Next();
            Node* n = New(NK_ARRAY, line);
            if (!Is(T_RBRACKET)) {
                for (;;) {
                    Node* e = ParseExpr();
                    if (!e) { delete n; return NULL; }
                    n->kids.push_back(e);
                    if (!Match(T_COMMA)) break;
                }
            }
            if (!Expect(T_RBRACKET, L"缺少 ']'")) { delete n; return NULL; }
            return n;
        }
        if (Is(T_ID)) {
            std::wstring id = Cur().str;
            Next();
            if (Is(T_LP)) {                   // 函数调用（参数个数不限）
                Next();
                Node* n = New(NK_CALL, line);
                n->name = id;
                if (!Is(T_RP)) {
                    for (;;) {
                        Node* e = ParseExpr();
                        if (!e) { delete n; return NULL; }
                        n->kids.push_back(e);
                        if (!Match(T_COMMA)) break;
                    }
                }
                if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
                return n;
            }
            if ((id == L"Math" || id == L"Event" || id == L"Key" || id == L"stage" || id == L"KeyboardEvent") && Is(T_DOT)) {
                Next();
                if (!Expect(T_ID, L"成员名")) return NULL;
                std::wstring nm = id + L"." + m_toks[m_i - 1].str;
                if ((id == L"Math" || id == L"Key" || id == L"stage") && Is(T_LP)) {   // Math.sin(x) / Key.isDown(code) / stage.addEventListener(...)
                    Next();
                    Node* n = New(NK_CALL, line);
                    n->name = nm;
                    if (!Is(T_RP)) {
                        for (;;) {
                            Node* e = ParseExpr();
                            if (!e) { delete n; return NULL; }
                            n->kids.push_back(e);
                            if (!Match(T_COMMA)) break;
                        }
                    }
                    if (!Expect(T_RP, L"缺少 ')'")) { delete n; return NULL; }
                    return n;
                }
                Node* n = New(NK_ID, line);   // Math.PI / Event.ENTER_FRAME / stage.keyCode / KeyboardEvent.KEY_DOWN
                n->name = nm;
                return n;
            }
            Node* n = New(NK_ID, line);
            n->name = id;
            return n;
        }
        if (Is(T_LP)) {
            Next();
            Node* e = ParseExpr();
            if (!e) return NULL;
            if (!Expect(T_RP, L"缺少 ')'")) { delete e; return NULL; }
            return e;
        }
        ErrHere(L"这里需要数字、名称或表达式");
        return NULL;
    }
};

// ---------------------------------------------------------------- 解释执行
ScriptEngine::ScriptEngine() : m_prog(NULL), m_hook(NULL), m_hookUser(NULL),
    m_hasFuncs(false), m_hasFrameHandlers(false), m_globalsDone(false),
    m_scene(NULL), m_frameMode(false), m_curFrame(0), m_error(false),
    m_keyCode(0), m_breakFlag(false), m_continueFlag(false), m_returnFlag(false),
    m_stepCount(0), m_callDepth(0), m_throwFlag(false) {
}

ScriptEngine::~ScriptEngine() {
    delete m_prog;
    m_prog = NULL;
}

void ScriptEngine::SetText(const std::wstring& text) {
    if (text == m_text) return;
    m_text = text;
    delete m_prog;
    m_prog = NULL;
    m_funcs.clear();
    m_hasFuncs = false;
    m_hasFrameHandlers = false;
    m_globalsDone = false;
}

bool ScriptEngine::EnsureParsed(std::wstring& err) {
    if (m_prog) return true;
    if (m_text.empty()) return true;
    std::vector<Tok> toks;
    if (!Lex(m_text, toks, err)) return false;
    Parser p(toks);
    m_prog = p.ParseProgram();
    if (!m_prog) {
        p.GetError(err);
        return false;
    }
    ScanAST();
    return true;
}

// 统计脚本里定义的函数与每帧回调（决定 RunFrame 用哪种模式）
void ScriptEngine::ScanAST() {
    m_hasFuncs = false;
    m_hasFrameHandlers = false;
    m_funcs.clear();
    ScanNode(m_prog);
}

void ScriptEngine::ScanNode(Node* n) {
    if (!n) return;
    if (n->kind == NK_FUNC) {
        m_hasFuncs = true;
        m_funcs[n->name] = n;
        if (n->name == L"onEnterFrame" || n->name == L"onEnterframe") m_hasFrameHandlers = true;
    }
    if ((n->kind == NK_CALL || n->kind == NK_MCALL) &&
        (n->name == L"addEventListener" || n->name == L"stage.addEventListener"))
        m_hasFrameHandlers = true;
    ScanNode(n->a); ScanNode(n->b); ScanNode(n->c); ScanNode(n->d);
    for (size_t i = 0; i < n->kids.size(); i++) ScanNode(n->kids[i]);
}

Value* ScriptEngine::FindVar(const std::wstring& name) {
    for (size_t i = m_scopes.size(); i > 0; i--) {
        std::map<std::wstring, Value>::iterator it = m_scopes[i - 1].find(name);
        if (it != m_scopes[i - 1].end()) return &it->second;
    }
    return NULL;
}

void ScriptEngine::ResetRun(Scene3D* scene, bool frameMode, int frame) {
    m_scene = scene;
    m_frameMode = frameMode;
    m_curFrame = frame;
    m_error = false;
    m_breakFlag = false;
    m_continueFlag = false;
    m_returnFlag = false;
    m_returnVal = Value();
    m_scopes.clear();
    m_scopes.push_back(std::map<std::wstring, Value>());
    m_frameFns.clear();
    m_log.clear();
    m_stepCount = 0;
    m_callDepth = 0;
    m_globalsDone = false;
}

// 逐帧模式（AS3 风格）：顶层变量保留，只清运行临时状态
void ScriptEngine::ResetFrameTemp(Scene3D* scene, int frame) {
    m_scene = scene;
    m_frameMode = true;
    m_curFrame = frame;
    m_error = false;
    m_breakFlag = false;
    m_continueFlag = false;
    m_returnFlag = false;
    m_returnVal = Value();
    m_log.clear();
    m_stepCount = 0;
    m_callDepth = 0;
}

void ScriptEngine::RunOnce(Scene3D* scene) {
    if (!scene) return;
    ResetRun(scene, false, scene->frame);
    std::wstring err;
    if (!EnsureParsed(err)) {
        m_log = err;
        m_error = true;
        return;
    }
    RunNode(m_prog);          // 顶层执行一次（NK_FUNC 节点只登记不执行）
    m_globalsDone = true;
}

// 关键帧脚本（Flash 帧动作）：进入关键帧时执行一次。
// frameMode=true → 读写显示值（dx/dy/dz），与导出网页解释器语义一致，
// 也避免被后续 SetFrame 插值（写本地 + SyncDisplayToLocal）把脚本效果拉回。
void ScriptEngine::RunKeyFrame(Scene3D* scene, int frame) {
    if (!scene) return;
    ResetRun(scene, true, frame);
    std::wstring err;
    if (!EnsureParsed(err)) {
        m_log = err;
        m_error = true;
        return;
    }
    RunNode(m_prog);
    m_globalsDone = true;
}

void ScriptEngine::RunFrame(Scene3D* scene, int frame) {
    if (!scene) return;
    std::wstring err;
    if (!EnsureParsed(err)) {
        ResetFrameTemp(scene, frame);
        m_log = err;
        m_error = true;
        return;
    }
    // 旧式脚本（没有函数、或定义了函数但没有每帧回调）：整段每帧执行（兼容旧行为）
    if (!m_hasFuncs || !m_hasFrameHandlers) {
        ResetRun(scene, true, frame);
        RunNode(m_prog);
        return;
    }
    // AS3 风格：顶层只在第一帧执行一次（初始化），之后每帧只调用回调
    if (!m_globalsDone) {
        ResetRun(scene, true, frame);
        RunNode(m_prog);
        m_globalsDone = true;
        if (m_error) return;
    } else {
        ResetFrameTemp(scene, frame);
    }
    // 每帧回调：addEventListener 注册的函数 + onEnterFrame 约定函数
    std::vector<std::wstring> fns = m_frameFns;
    if (m_funcs.find(L"onEnterFrame") != m_funcs.end() &&
        std::find(fns.begin(), fns.end(), L"onEnterFrame") == fns.end())
        fns.push_back(L"onEnterFrame");
    for (size_t i = 0; i < fns.size() && !m_error; i++) {
        std::map<std::wstring, Node*>::iterator it = m_funcs.find(fns[i]);
        if (it == m_funcs.end()) continue;
        std::vector<Value> args(1, Value());   // event 参数占位
        CallUserNode(it->second, args, 0);
    }
}

void ScriptEngine::Err(int line, const std::wstring& msg) {
    if (m_error) return;
    m_error = true;
    wchar_t b[256];
    swprintf(b, 256, L"第%d行：%s", line, msg.c_str());
    m_log += b;
    m_log += L"\r\n";
}

void ScriptEngine::RunNode(Node* n) {
    if (!n || m_error) return;
    switch (n->kind) {
        case NK_BLOCK:
            for (size_t i = 0; i < n->kids.size(); i++) {
                RunNode(n->kids[i]);
                if (m_error || m_breakFlag || m_continueFlag || m_returnFlag) break;
            }
            break;
        case NK_VAR: {
            Value v;
            if (n->a) v = Eval(n->a);
            m_scopes.back()[n->name] = v;
            break;
        }
        case NK_FUNC:
            // 函数定义默认不执行（登记在 m_funcs）；bval=true 为类构造函数（与类同名）→ 顶层执行一次
            if (n->bval && n->a) RunNode(n->a);
            break;
        case NK_ASSIGN:
            AssignNode(n->a, Eval(n->b), n->op);
            break;
        case NK_INC:
            Eval(n);
            break;
        case NK_IF: {
            Value c = Eval(n->a);
            if (IsTrue(c)) RunNode(n->b);
            else if (n->c) RunNode(n->c);
            break;
        }
        case NK_WHILE:
            while (!m_error && !m_breakFlag && !m_returnFlag) {
                Value c = Eval(n->a);
                if (!IsTrue(c)) break;
                if (++m_stepCount > 2000000) { Err(n->line, L"循环次数太多，已自动停止"); break; }
                RunNode(n->b);
                if (m_continueFlag) m_continueFlag = false;   // continue → 回到条件判断
            }
            m_breakFlag = false;
            break;
        case NK_FOR:
            if (n->a) RunNode(n->a);
            while (!m_error && !m_breakFlag && !m_returnFlag) {
                if (n->b) {
                    Value c = Eval(n->b);
                    if (!IsTrue(c)) break;
                }
                if (++m_stepCount > 2000000) { Err(n->line, L"循环次数太多，已自动停止"); break; }
                RunNode(n->d);
                if (m_error || m_breakFlag || m_returnFlag) break;
                if (n->c) RunNode(n->c);   // continue 也执行增量表达式
                if (m_continueFlag) m_continueFlag = false;
            }
            m_breakFlag = false;
            break;
        case NK_BREAK:
            m_breakFlag = true;
            break;
        case NK_CONTINUE:
            m_continueFlag = true;
            break;
        case NK_DOWHILE:                    // do { body } while (cond);
            for (;;) {
                if (++m_stepCount > 2000000) { Err(n->line, L"循环次数太多，已自动停止"); break; }
                RunNode(n->a);
                if (m_error || m_breakFlag || m_returnFlag) break;
                if (m_continueFlag) m_continueFlag = false;
                Value c = Eval(n->b);
                if (m_error) break;
                if (!IsTrue(c)) break;
            }
            m_breakFlag = false;
            break;
        case NK_SWITCH: {                   // switch (x) { case ... }
            Value sv = Eval(n->a);
            if (m_error) break;
            Node* matched = NULL;
            for (size_t i = 0; i < n->kids.size(); i++) {
                Node* cs = n->kids[i];
                if (!cs->a) continue;
                Value cv = Eval(cs->a);
                if (m_error) break;
                bool eq = false;
                if (sv.t == Value::V_NUM && cv.t == Value::V_NUM) eq = (sv.num == cv.num);
                else if (sv.t == Value::V_STR && cv.t == Value::V_STR) eq = (sv.str == cv.str);
                else if (sv.t == Value::V_BOOL && cv.t == Value::V_BOOL) eq = (sv.b == cv.b);
                if (eq) { matched = cs; break; }
            }
            if (!matched) matched = n->d;   // default
            if (matched && matched->b) RunNode(matched->b);
            m_breakFlag = false;
            break;
        }
        case NK_TRY: {                      // try { } catch (e) { } finally { }
            bool b1 = m_breakFlag, c1 = m_continueFlag, r1 = m_returnFlag;
            RunNode(n->a);
            if (m_error && n->b) {          // 捕获：恢复执行
                Value ev = m_throwFlag ? m_throwVal : Value();
                m_error = false;
                m_throwFlag = false;
                m_scopes.back()[n->b->name] = ev;   // 异常变量（throw 的值或空）
                m_breakFlag = b1; m_continueFlag = c1; m_returnFlag = r1;
                RunNode(n->c);
            }
            bool b2 = m_breakFlag, c2 = m_continueFlag, r2 = m_returnFlag, e2 = m_error;
            if (n->d) {                     // finally 总是执行（恢复跳出标志后运行）
                m_breakFlag = b1; m_continueFlag = c1; m_returnFlag = r1;
                RunNode(n->d);
                if (!m_error) { m_error = e2; }
                m_breakFlag = b2; m_continueFlag = c2; m_returnFlag = r2;
            }
            break;
        }
        case NK_THROW:                      // throw "信息";
            m_throwVal = n->a ? Eval(n->a) : Value();
            m_throwFlag = true;
            Err(n->line, m_throwVal.t == Value::V_STR ? m_throwVal.str : m_throwVal.ToString());
            break;
        case NK_FORIN: {                    // for (var k in obj) { }  数组→下标，字符串→字符，元件→属性名
            Value obj = Eval(n->b);
            if (m_error) break;
            if (obj.t == Value::V_ARR && obj.arr) {
                for (size_t i = 0; i < obj.arr->size() && !m_error && !m_breakFlag && !m_returnFlag; i++) {
                    m_scopes.back()[n->name] = Value::MakeNum((double)i);
                    RunNode(n->c);
                    if (m_continueFlag) m_continueFlag = false;
                    if (++m_stepCount > 2000000) { Err(n->line, L"循环次数太多，已自动停止"); break; }
                }
            } else if (obj.t == Value::V_STR) {
                for (size_t i = 0; i < obj.str.size() && !m_error && !m_breakFlag && !m_returnFlag; i++) {
                    m_scopes.back()[n->name] = Value::MakeNum((double)i);
                    RunNode(n->c);
                    if (m_continueFlag) m_continueFlag = false;
                }
            } else if (obj.t == Value::V_OBJ && obj.obj) {
                static const wchar_t* props[] = { L"x", L"y", L"z", L"rx", L"ry", L"rz",
                    L"sx", L"sy", L"sz", L"visible", L"red", L"green", L"blue", L"name" };
                for (int i = 0; i < 14 && !m_error && !m_breakFlag && !m_returnFlag; i++) {
                    m_scopes.back()[n->name] = Value::MakeStr(props[i]);
                    RunNode(n->c);
                    if (m_continueFlag) m_continueFlag = false;
                }
            } else if (obj.t == Value::V_NUM) {
                for (int i = 0; i < (int)obj.num && !m_error && !m_breakFlag && !m_returnFlag; i++) {
                    m_scopes.back()[n->name] = Value::MakeNum((double)i);
                    RunNode(n->c);
                    if (m_continueFlag) m_continueFlag = false;
                }
            }
            m_breakFlag = false;
            break;
        }
        case NK_RETURN:
            m_returnVal = n->a ? Eval(n->a) : Value();
            m_returnFlag = true;
            break;
        case NK_CALL: {
            std::vector<Value> args;
            for (size_t i = 0; i < n->kids.size(); i++) {
                args.push_back(Eval(n->kids[i]));
                if (m_error) return;
            }
            if (m_funcs.find(n->name) != m_funcs.end())
                CallUser(n->name, args, n->line);
            else
                CallFunc(n->name, args, n->line);
            break;
        }
        default:
            Eval(n); // 表达式语句（如单独的 new 表达式无副作用，忽略）
            break;
    }
}

Value ScriptEngine::Eval(Node* n) {
    if (!n || m_error) return Value();
    switch (n->kind) {
        case NK_NUM: {
            Value r = Value::MakeNum(n->num);
            return r;
        }
        case NK_STR: return Value::MakeStr(n->str);
        case NK_BOOL: return Value::MakeBool(n->bval);
        case NK_NULL: return Value();
        case NK_ID: {
            Value* pv = FindVar(n->name);
            if (pv) return *pv;
            if (n->name == L"Math.PI") return Value::MakeNum(kPI);
            if (n->name == L"Math.E") return Value::MakeNum(2.718281828459045);
            if (n->name == L"Math.LN2") return Value::MakeNum(0.6931471805599453);
            if (n->name == L"Math.LN10") return Value::MakeNum(2.302585092994046);
            if (n->name == L"Math.LOG2E") return Value::MakeNum(1.4426950408889634);
            if (n->name == L"Math.LOG10E") return Value::MakeNum(0.4342944819032518);
            if (n->name == L"Math.SQRT2") return Value::MakeNum(1.4142135623730951);
            if (n->name == L"Math.SQRT1_2") return Value::MakeNum(0.7071067811865476);
            if (n->name == L"Event.ENTER_FRAME") return Value::MakeStr(L"enterFrame");
            // KeyboardEvent 常量（AS3 键盘事件类型）
            if (n->name == L"KeyboardEvent.KEY_DOWN") return Value::MakeStr(L"keyDown");
            if (n->name == L"KeyboardEvent.KEY_UP") return Value::MakeStr(L"keyUp");
            // stage.keyCode：最近一次 stage 键盘事件键码（AS3 事件对象简化版）
            if (n->name == L"stage.keyCode" || n->name == L"keyCode")
                return Value::MakeNum((double)m_keyCode);
            // Key 常量（与 AS3 Keyboard 类对齐，方便写游戏：Key.isDown(Key.LEFT)）
            if (n->name == L"Key.LEFT") return Value::MakeNum(37);
            if (n->name == L"Key.UP") return Value::MakeNum(38);
            if (n->name == L"Key.RIGHT") return Value::MakeNum(39);
            if (n->name == L"Key.DOWN") return Value::MakeNum(40);
            if (n->name == L"Key.SPACE") return Value::MakeNum(32);
            if (n->name == L"Key.A") return Value::MakeNum(65);
            if (n->name == L"Key.D") return Value::MakeNum(68);
            if (n->name == L"Key.W") return Value::MakeNum(87);
            if (n->name == L"Key.S") return Value::MakeNum(83);
            if (n->name == L"frame" || n->name == L"帧") return Value::MakeNum((double)m_curFrame);
            if (n->name == L"fps") return Value::MakeNum((double)m_scene->fps);
            if (m_funcs.find(n->name) != m_funcs.end())
                return Value::MakeStr(n->name);   // 函数名作为值（供 addEventListener 用）
            Object3D* o = FindObj(m_scene, n->name);
            if (o) return Value::MakeObj(o);
            Err(n->line, L"找不到元件或变量 '" + n->name + L"'");
            return Value();
        }
        case NK_PROP: {
            Value base = Eval(n->a);
            if (m_error) return Value();
            if (base.t == Value::V_OBJ && base.obj)
                return GetPropValue(base.obj, n->name, n->line);
            if (base.t == Value::V_STR && n->name == L"length")
                return Value::MakeNum((double)base.str.size());
            if (base.t == Value::V_ARR && base.arr && n->name == L"length")
                return Value::MakeNum((double)base.arr->size());
            Err(n->line, L"没有这个属性 '" + n->name + L"'");
            return Value();
        }
        case NK_INDEX: {
            Value base = Eval(n->a);
            Value idx = Eval(n->b);
            if (m_error) return Value();
            if (base.t == Value::V_ARR && base.arr) {
                if (idx.t != Value::V_NUM) { Err(n->line, L"下标需要数字"); return Value(); }
                int i = (int)idx.num;
                if (i < 0 || i >= (int)base.arr->size()) { Err(n->line, L"下标越界"); return Value(); }
                return (*base.arr)[i];
            }
            if (base.t == Value::V_STR) {
                if (idx.t != Value::V_NUM) { Err(n->line, L"下标需要数字"); return Value(); }
                int i = (int)idx.num;
                if (i < 0 || i >= (int)base.str.size()) return Value::MakeStr(L"");
                return Value::MakeStr(std::wstring(1, base.str[i]));
            }
            Err(n->line, L"只能对数组或字符串取下标");
            return Value();
        }
        case NK_ARRAY: {
            Value v = Value::MakeArr();
            for (size_t i = 0; i < n->kids.size(); i++) {
                v.arr->push_back(Eval(n->kids[i]));
                if (m_error) return Value();
            }
            return v;
        }
        case NK_TERNARY: {                 // 条件 ? 值1 : 值2
            Value c = Eval(n->a);
            if (m_error) return Value();
            return IsTrue(c) ? Eval(n->b) : Eval(n->c);
        }
        case NK_TYPEOF: {                  // typeof x → "number"/"string"/"boolean"/"object"
            Value a = Eval(n->a);
            if (m_error) return Value();
            switch (a.t) {
                case Value::V_NUM: return Value::MakeStr(L"number");
                case Value::V_STR: return Value::MakeStr(L"string");
                case Value::V_BOOL: return Value::MakeStr(L"boolean");
                default: return Value::MakeStr(L"object");
            }
        }
        case NK_THIS:                      // this（脚本挂载在场景，返回空；保留 AS3 语法兼容）
            return Value();
        case NK_DELETE: {                  // delete arr[i] / delete arr["k"]
            if (n->a && n->a->kind == NK_INDEX) {
                Value base = Eval(n->a->a);
                Value idx = Eval(n->a->b);
                if (m_error) return Value();
                if (base.t == Value::V_ARR && base.arr && idx.t == Value::V_NUM) {
                    int i = (int)idx.num;
                    if (i >= 0 && i < (int)base.arr->size()) {
                        base.arr->erase(base.arr->begin() + i);
                        return Value::MakeBool(true);
                    }
                    return Value::MakeBool(false);
                }
                if (base.t == Value::V_STR && idx.t == Value::V_NUM) {
                    int i = (int)idx.num;
                    if (i >= 0 && i < (int)base.str.size()) {
                        std::wstring r = base.str;
                        r.erase(i, 1);
                        if (n->a->a->kind == NK_ID) {
                            Value* pv = FindVar(n->a->a->name);
                            if (pv) *pv = Value::MakeStr(r);
                        }
                        return Value::MakeBool(true);
                    }
                    return Value::MakeBool(false);
                }
            }
            return Value::MakeBool(false);
        }
        case NK_MCALL: {
            Value base = Eval(n->a);
            if (m_error) return Value();
            std::vector<Value> args;
            for (size_t i = 0; i < n->kids.size(); i++) {
                args.push_back(Eval(n->kids[i]));
                if (m_error) return Value();
            }
            if (base.t == Value::V_ARR && base.arr) {
                if (n->name == L"push") {
                    for (size_t i = 0; i < args.size(); i++) base.arr->push_back(args[i]);
                    return Value::MakeNum((double)base.arr->size());
                }
                if (n->name == L"pop") {
                    if (base.arr->empty()) return Value();
                    Value last = base.arr->back();
                    base.arr->pop_back();
                    return last;
                }
                if (n->name == L"join") {
                    std::wstring sep = (args.size() > 0 && args[0].t == Value::V_STR) ? args[0].str : L",";
                    std::wstring r;
                    for (size_t i = 0; i < base.arr->size(); i++) {
                        if (i) r += sep;
                        r += (*base.arr)[i].ToString();
                    }
                    return Value::MakeStr(r);
                }
            }
            if (base.t == Value::V_STR) {
                if (n->name == L"charAt") {
                    int i = (args.size() > 0 && args[0].t == Value::V_NUM) ? (int)args[0].num : 0;
                    if (i < 0 || i >= (int)base.str.size()) return Value::MakeStr(L"");
                    return Value::MakeStr(std::wstring(1, base.str[i]));
                }
                if (n->name == L"substring" || n->name == L"substr") {
                    int len = (int)base.str.size();
                    int i = (args.size() > 0 && args[0].t == Value::V_NUM) ? (int)args[0].num : 0;
                    int j = (args.size() > 1 && args[1].t == Value::V_NUM) ? (int)args[1].num : len;
                    if (n->name == L"substr") { if (i < 0) i = 0; if (j < 0) j = len; j = i + j; }
                    if (i < 0) i = 0;
                    if (j > len) j = len;
                    if (i >= j) return Value::MakeStr(L"");
                    return Value::MakeStr(base.str.substr(i, j - i));
                }
                if (n->name == L"toUpperCase") {
                    std::wstring r = base.str;
                    for (size_t i = 0; i < r.size(); i++) r[i] = towupper(r[i]);
                    return Value::MakeStr(r);
                }
                if (n->name == L"toLowerCase") {
                    std::wstring r = base.str;
                    for (size_t i = 0; i < r.size(); i++) r[i] = towlower(r[i]);
                    return Value::MakeStr(r);
                }
            }
            Err(n->line, L"对象没有这个方法 '" + n->name + L"'");
            return Value();
        }
        case NK_NEW: {
            int pt = ResolvePrimType(n->name);
            if (pt < 0) { Err(n->line, L"没有这个类型 '" + n->name + L"'"); return Value(); }
            std::wstring nm;
            if (n->a) {
                Value v = Eval(n->a);
                if (m_error) return Value();
                if (v.t == Value::V_STR) nm = v.str;
            }
            if (nm.empty() || m_scene->FindByName(nm)) {   // 没起名或重名 → 自动起不重复的名字
                wchar_t b[96];
                for (int i = 1; ; i++) {
                    swprintf(b, 96, L"%s%d", Object3D::TypeName(pt), i);
                    if (!m_scene->FindByName(b)) { nm = b; break; }
                }
            }
            Object3D* o = m_scene->AddObject(pt, nm, NULL);
            return o ? Value::MakeObj(o) : Value();
        }
        case NK_CALL: {
            std::vector<Value> args;
            for (size_t i = 0; i < n->kids.size(); i++) {
                args.push_back(Eval(n->kids[i]));
                if (m_error) return Value();
            }
            if (m_funcs.find(n->name) != m_funcs.end())
                return CallUser(n->name, args, n->line);
            return CallFunc(n->name, args, n->line);
        }
        case NK_NEG: {
            Value a = Eval(n->a);
            if (a.t == Value::V_NUM) return Value::MakeNum(-a.num);
            Err(n->line, L"取负运算需要数字");
            return Value();
        }
        case NK_NOT: {
            Value a = Eval(n->a);
            return Value::MakeBool(!IsTrue(a));
        }
        case NK_INC: {
            if (!n->a) { Err(n->line, L"自增/自减缺少目标"); return Value(); }
            double delta = (n->op == T_INC) ? 1.0 : -1.0;
            if (n->a->kind == NK_ID) {
                std::map<std::wstring, Value>::iterator it = m_scopes.back().find(n->a->name);
                if (it == m_scopes.back().end()) {
                    Value* pv = FindVar(n->a->name);
                    if (!pv) {
                        Object3D* o = FindObj(m_scene, n->a->name);
                        if (o) { Err(n->line, L"不能直接给元件自增，请用 元件.属性 的形式"); return Value(); }
                        Err(n->line, L"找不到元件或变量 '" + n->a->name + L"'");
                        return Value();
                    }
                    double old = pv->num;
                    *pv = Value::MakeNum(old + delta);
                    return Value::MakeNum(n->bval ? old + delta : old);
                }
                double old = it->second.num;
                it->second = Value::MakeNum(old + delta);
                return Value::MakeNum(n->bval ? old + delta : old);
            }
            if (n->a->kind == NK_PROP) {
                Value base = Eval(n->a->a);
                if (m_error || base.t != Value::V_OBJ || !base.obj) {
                    if (!m_error) Err(n->line, L"'" + n->a->name + L"' 前面不是元件");
                    return Value();
                }
                Value old = GetPropValue(base.obj, n->a->name, n->line);
                if (m_error) return Value();
                if (old.t != Value::V_NUM) { Err(n->line, L"自增/自减需要数字"); return Value(); }
                double r = old.num + delta;
                SetPropValue(base.obj, n->a->name, Value::MakeNum(r), n->line);
                return Value::MakeNum(n->bval ? r : old.num);
            }
            if (n->a->kind == NK_INDEX) {   // arr[i]++ / ++arr[i]
                Value base = Eval(n->a->a);
                Value idx = Eval(n->a->b);
                if (m_error) return Value();
                if (base.t != Value::V_ARR || !base.arr || idx.t != Value::V_NUM) {
                    if (!m_error) Err(n->line, L"只能对数组元素自增/自减");
                    return Value();
                }
                int i = (int)idx.num;
                if (i < 0 || i >= (int)base.arr->size()) { Err(n->line, L"下标越界"); return Value(); }
                double old = (*base.arr)[i].num;
                (*base.arr)[i] = Value::MakeNum(old + delta);
                return Value::MakeNum(n->bval ? old + delta : old);
            }
            Err(n->line, L"不能给这个表达式自增/自减");
            return Value();
        }
        case NK_OP2: {
            if (n->op == T_AND) {
                Value a = Eval(n->a);
                if (!IsTrue(a)) return Value::MakeBool(false);
                return Value::MakeBool(IsTrue(Eval(n->b)));
            }
            if (n->op == T_OR) {
                Value a = Eval(n->a);
                if (IsTrue(a)) return Value::MakeBool(true);
                return Value::MakeBool(IsTrue(Eval(n->b)));
            }
            Value a = Eval(n->a);
            Value b = Eval(n->b);
            if (m_error) return Value();
            // 字符串拼接：字符串 + 字符串 / 字符串 + 数字 / 数字 + 字符串（必须先于相等比较判断，AS3 允许 str+str）
            if (n->op == T_PLUS && (a.t == Value::V_STR || b.t == Value::V_STR)) {
                wchar_t nb[64];
                if (a.t == Value::V_STR && b.t == Value::V_STR)
                    return Value::MakeStr(a.str + b.str);
                if (a.t == Value::V_STR && b.t == Value::V_NUM) {
                    swprintf(nb, 64, L"%g", b.num);
                    return Value::MakeStr(a.str + nb);
                }
                if (a.t == Value::V_NUM && b.t == Value::V_STR) {
                    swprintf(nb, 64, L"%g", a.num);
                    return Value::MakeStr(nb + b.str);
                }
                if (a.t == Value::V_STR && b.t == Value::V_BOOL)
                    return Value::MakeStr(a.str + b.ToString());
                if (a.t == Value::V_BOOL && b.t == Value::V_STR)
                    return Value::MakeStr(a.ToString() + b.str);
                Err(n->line, L"字符串只能与数字拼接");
                return Value();
            }
            // is / as / instanceof：类型判断（右操作数是类型名，如 obj is Ball）
            if (n->op == T_KW_IS || n->op == T_KW_AS || n->op == T_KW_INSTANCEOF) {
                std::wstring tn = (n->b && n->b->kind == NK_ID) ? n->b->name : L"";
                int pt = ResolvePrimType(tn);
                bool ok = (a.t == Value::V_OBJ && a.obj && pt >= 0 && a.obj->type == pt);
                if (n->op == T_KW_AS) return ok ? a : Value();
                return Value::MakeBool(ok);
            }
            // 严格相等/严格不等：先比类型再比值
            if (n->op == T_SEQ || n->op == T_SNE) {
                bool eq = false;
                if (a.t == b.t) {
                    if (a.t == Value::V_NUM) eq = (a.num == b.num);
                    else if (a.t == Value::V_STR) eq = (a.str == b.str);
                    else if (a.t == Value::V_BOOL) eq = (a.b == b.b);
                    else if (a.t == Value::V_NUL) eq = true;
                    else if (a.t == Value::V_OBJ) eq = (a.obj == b.obj);
                    else if (a.t == Value::V_ARR) eq = (a.arr == b.arr);
                }
                return Value::MakeBool(n->op == T_SEQ ? eq : !eq);
            }
            // 字符串相等比较
            if (a.t == Value::V_STR && b.t == Value::V_STR) {
                if (n->op == T_EQ) return Value::MakeBool(a.str == b.str);
                if (n->op == T_NE) return Value::MakeBool(a.str != b.str);
                Err(n->line, L"字符串只能比较相等");
                return Value();
            }
            if (a.t != Value::V_NUM || b.t != Value::V_NUM) {
                Err(n->line, L"运算需要数字");
                return Value();
            }
            double x = a.num, y = b.num;
            switch (n->op) {
                case T_PLUS: return Value::MakeNum(x + y);
                case T_MINUS: return Value::MakeNum(x - y);
                case T_STAR: return Value::MakeNum(x * y);
                case T_SLASH:
                    if (y == 0.0) { Err(n->line, L"不能除以 0"); return Value(); }
                    return Value::MakeNum(x / y);
                case T_PERCENT:
                    if (y == 0.0) { Err(n->line, L"不能除以 0"); return Value(); }
                    return Value::MakeNum(std::fmod(x, y));
                case T_EQ: return Value::MakeBool(x == y);
                case T_NE: return Value::MakeBool(x != y);
                case T_LT: return Value::MakeBool(x < y);
                case T_LE: return Value::MakeBool(x <= y);
                case T_GT: return Value::MakeBool(x > y);
                case T_GE: return Value::MakeBool(x >= y);
            }
            Err(n->line, L"未知运算");
            return Value();
        }
        default:
            return Value();
    }
}

void ScriptEngine::AssignNode(Node* lv, const Value& v, int op) {
    if (!lv || m_error) return;
    if (lv->kind == NK_ID) {
        Value* pv = FindVar(lv->name);
        if (pv) {
            if (op == T_ASSIGN) { *pv = v; return; }
            // 复合赋值：先读旧值再运算（+= 还支持字符串拼接，AS3 行为）
            Value r;
            if (op == T_PLUSEQ && (pv->t == Value::V_STR || v.t == Value::V_STR)) {
                r = Value::MakeStr(pv->ToString() + v.ToString());
            } else {
                if (pv->t != Value::V_NUM || v.t != Value::V_NUM) { Err(lv->line, L"复合赋值需要数字"); return; }
                switch (op) {
                    case T_PLUSEQ: r = Value::MakeNum(pv->num + v.num); break;
                    case T_MINUSEQ: r = Value::MakeNum(pv->num - v.num); break;
                    case T_STAREQ: r = Value::MakeNum(pv->num * v.num); break;
                    case T_SLASHEQ:
                        if (v.num == 0.0) { Err(lv->line, L"不能除以 0"); return; }
                        r = Value::MakeNum(pv->num / v.num); break;
                    default: r = *pv; break;
                }
            }
            *pv = r;
            return;
        }
        Object3D* o = FindObj(m_scene, lv->name);
        if (o) {
            Err(lv->line, L"不能直接给元件赋值，请用 元件.属性 的形式");
            return;
        }
        // Flash 风格：直接给未声明的名字赋值 = 自动创建全局变量
        m_scopes[0][lv->name] = v;
        return;
    }
    if (lv->kind == NK_PROP) {
        Value base = Eval(lv->a);
        if (m_error || base.t != Value::V_OBJ || !base.obj) {
            if (!m_error) Err(lv->line, L"'" + lv->name + L"' 前面不是元件");
            return;
        }
        // 复合赋值：先读旧值再运算；普通赋值直接写入
        Value newVal = v;
        if (op != T_ASSIGN) {
            Value old = GetPropValue(base.obj, lv->name, lv->line);
            if (m_error) return;
            if (old.t != Value::V_NUM || v.t != Value::V_NUM) {
                Err(lv->line, L"复合赋值需要数字");
                return;
            }
            double r;
            switch (op) {
                case T_PLUSEQ: r = old.num + v.num; break;
                case T_MINUSEQ: r = old.num - v.num; break;
                case T_STAREQ: r = old.num * v.num; break;
                case T_SLASHEQ:
                    if (v.num == 0.0) { Err(lv->line, L"不能除以 0"); return; }
                    r = old.num / v.num;
                    break;
                default: r = old.num; break;
            }
            newVal = Value::MakeNum(r);
        }
        SetPropValue(base.obj, lv->name, newVal, lv->line);
        return;
    }
    if (lv->kind == NK_INDEX) {   // arr[i] = x / arr[i] += x
        Value base = Eval(lv->a);
        Value idx = Eval(lv->b);
        if (m_error) return;
        if (base.t != Value::V_ARR || !base.arr) { Err(lv->line, L"只能给数组元素赋值"); return; }
        if (idx.t != Value::V_NUM) { Err(lv->line, L"下标需要数字"); return; }
        int i = (int)idx.num;
        if (i < 0 || i > (int)base.arr->size()) { Err(lv->line, L"下标越界"); return; }
        Value newVal = v;
        if (op != T_ASSIGN) {
            if (i == (int)base.arr->size()) { Err(lv->line, L"下标越界"); return; }
            Value old = (*base.arr)[i];
            if (old.t != Value::V_NUM || v.t != Value::V_NUM) { Err(lv->line, L"复合赋值需要数字"); return; }
            double r;
            switch (op) {
                case T_PLUSEQ: r = old.num + v.num; break;
                case T_MINUSEQ: r = old.num - v.num; break;
                case T_STAREQ: r = old.num * v.num; break;
                case T_SLASHEQ:
                    if (v.num == 0.0) { Err(lv->line, L"不能除以 0"); return; }
                    r = old.num / v.num;
                    break;
                default: r = old.num; break;
            }
            newVal = Value::MakeNum(r);
        }
        if (i == (int)base.arr->size()) base.arr->push_back(newVal);   // AS3：越界一位 = 追加
        else (*base.arr)[i] = newVal;
        return;
    }
    Err(lv->line, L"不能给这个表达式赋值");
}

Value ScriptEngine::GetPropValue(Object3D* o, const std::wstring& p, int line) {
    if (!o) return Value();
    double v = 0;
    if (p == L"x") v = m_frameMode ? o->dx : o->x;
    else if (p == L"y") v = m_frameMode ? o->dy : o->y;
    else if (p == L"z") v = m_frameMode ? o->dz : o->z;
    else if (p == L"rx") v = m_frameMode ? o->drx : o->rx;
    else if (p == L"ry") v = m_frameMode ? o->dry : o->ry;
    else if (p == L"rz") v = m_frameMode ? o->drz : o->rz;
    else if (p == L"rotationX" || p == L"rotation") v = m_frameMode ? o->drx : o->rx;
    else if (p == L"rotationY") v = m_frameMode ? o->dry : o->ry;
    else if (p == L"rotationZ") v = m_frameMode ? o->drz : o->rz;
    else if (p == L"sx") v = m_frameMode ? o->dsx : o->sx;
    else if (p == L"sy") v = m_frameMode ? o->dsy : o->sy;
    else if (p == L"sz") v = m_frameMode ? o->dsz : o->sz;
    else if (p == L"scaleX") v = m_frameMode ? o->dsx : o->sx;
    else if (p == L"scaleY") v = m_frameMode ? o->dsy : o->sy;
    else if (p == L"scaleZ") v = m_frameMode ? o->dsz : o->sz;
    else if (p == L"s") v = m_frameMode ? o->dsx : o->sx;
    else if (p == L"visible") return Value::MakeBool(o->visible);
    else if (p == L"name") return Value::MakeStr(o->name);
    else if (p == L"parent") return Value::MakeObj(o->parent);
    else if (p == L"type") return Value::MakeStr(Object3D::TypeName(o->type));
    else if (p == L"segments" || p == L"细分") return Value::MakeNum((double)o->segments);
    else if (p == L"red") return Value::MakeNum(o->r * 255.0);
    else if (p == L"green") return Value::MakeNum(o->g * 255.0);
    else if (p == L"blue") return Value::MakeNum(o->b * 255.0);
    else {
        Err(line, L"没有这个属性 '" + p + L"'");
        return Value();
    }
    return Value::MakeNum(v);
}

void ScriptEngine::SetPropValue(Object3D* o, const std::wstring& p, const Value& v, int line) {
    if (!o) return;
    if (p == L"visible") {
        o->visible = IsTrue(v);
        return;
    }
    if (p == L"segments" || p == L"细分") {
        if (v.t != Value::V_NUM) { Err(line, L"细分需要数字"); return; }
        int s = (int)v.num;
        if (s < 1) s = 1;
        if (s > 64) s = 64;
        o->segments = s;
        return;
    }
    if (p == L"red" || p == L"green" || p == L"blue") {
        if (v.t != Value::V_NUM) { Err(line, L"颜色需要数字（0~255）"); return; }
        double c = v.num < 0 ? 0 : (v.num > 255 ? 255 : v.num) / 255.0;
        if (p == L"red") o->r = c;
        else if (p == L"green") o->g = c;
        else o->b = c;
        return;
    }
    if (v.t != Value::V_NUM) { Err(line, L"属性 '" + p + L"' 需要数字"); return; }
    double val = v.num;
    if (p == L"x") { if (m_frameMode) o->dx = val; else o->x = val; }
    else if (p == L"y") { if (m_frameMode) o->dy = val; else o->y = val; }
    else if (p == L"z") { if (m_frameMode) o->dz = val; else o->z = val; }
    else if (p == L"rx") { if (m_frameMode) o->drx = val; else o->rx = val; }
    else if (p == L"ry") { if (m_frameMode) o->dry = val; else o->ry = val; }
    else if (p == L"rz") { if (m_frameMode) o->drz = val; else o->rz = val; }
    else if (p == L"rotationX" || p == L"rotation") { if (m_frameMode) o->drx = val; else o->rx = val; }
    else if (p == L"rotationY") { if (m_frameMode) o->dry = val; else o->ry = val; }
    else if (p == L"rotationZ") { if (m_frameMode) o->drz = val; else o->rz = val; }
    else if (p == L"sx") { if (m_frameMode) o->dsx = val; else o->sx = val; }
    else if (p == L"sy") { if (m_frameMode) o->dsy = val; else o->sy = val; }
    else if (p == L"sz") { if (m_frameMode) o->dsz = val; else o->sz = val; }
    else if (p == L"scaleX") { if (m_frameMode) o->dsx = val; else o->sx = val; }
    else if (p == L"scaleY") { if (m_frameMode) o->dsy = val; else o->sy = val; }
    else if (p == L"scaleZ") { if (m_frameMode) o->dsz = val; else o->sz = val; }
    else if (p == L"s") {
        if (m_frameMode) { o->dsx = o->dsy = o->dsz = val; }
        else { o->sx = o->sy = o->sz = val; }
    }
    else Err(line, L"没有这个属性 '" + p + L"'");
}

// 内置函数（含 Math. 前缀、trace、类型转换、旧中文别名）
Value ScriptEngine::CallFunc(const std::wstring& fn, std::vector<Value>& args, int line) {
    Value a = args.size() > 0 ? args[0] : Value();
    Value b = args.size() > 1 ? args[1] : Value();
    // ---- Math.xxx（AS3 语义：三角函数用弧度） ----
    if (fn.compare(0, 5, L"Math.") == 0) {
        std::wstring m = fn.substr(5);
        if (m == L"sin") return Value::MakeNum(std::sin(a.num));
        if (m == L"cos") return Value::MakeNum(std::cos(a.num));
        if (m == L"tan") return Value::MakeNum(std::tan(a.num));
        if (m == L"asin") return Value::MakeNum(std::asin(a.num));
        if (m == L"acos") return Value::MakeNum(std::acos(a.num));
        if (m == L"atan") return Value::MakeNum(std::atan(a.num));
        if (m == L"atan2") return Value::MakeNum(std::atan2(a.num, b.num));
        if (m == L"sqrt") {
            if (a.num < 0) { Err(line, L"开方不能是负数"); return Value(); }
            return Value::MakeNum(std::sqrt(a.num));
        }
        if (m == L"pow") return Value::MakeNum(std::pow(a.num, b.num));
        if (m == L"abs") return Value::MakeNum(std::fabs(a.num));
        if (m == L"floor") return Value::MakeNum(std::floor(a.num));
        if (m == L"ceil") return Value::MakeNum(std::ceil(a.num));
        if (m == L"round") return Value::MakeNum(std::floor(a.num + 0.5));
        if (m == L"exp") return Value::MakeNum(std::exp(a.num));
        if (m == L"log") return Value::MakeNum(std::log(a.num));
        if (m == L"min") {
            double best = 0;
            for (size_t i = 0; i < args.size(); i++) {
                if (i == 0 || args[i].num < best) best = args[i].num;
            }
            return Value::MakeNum(best);
        }
        if (m == L"max") {
            double best = 0;
            for (size_t i = 0; i < args.size(); i++) {
                if (i == 0 || args[i].num > best) best = args[i].num;
            }
            return Value::MakeNum(best);
        }
        if (m == L"random") return Value::MakeNum((double)std::rand() / (double)RAND_MAX);
        Err(line, L"Math 没有这个函数 '" + m + L"'");
        return Value();
    }
    // ---- Key.isDown(code)：查询某键是否按下（AS3 Keyboard 查询风格） ----
    if (fn == L"Key.isDown") {
        if (args.empty() || args[0].t != Value::V_NUM) { Err(line, L"Key.isDown 需要键码参数"); return Value(); }
        return Value::MakeBool(IsKeyDown((int)args[0].num));
    }
    // ---- 输出 ----
    if (fn == L"trace") {
        for (size_t i = 0; i < args.size(); i++) {
            if (i) m_log += L" ";
            m_log += args[i].ToString();
        }
        m_log += L"\r\n";
        return Value();
    }
    if (fn == L"print" || fn == L"输出") {
        m_log += a.ToString();
        m_log += L"\r\n";
        return Value();
    }
    // ---- 类型转换（AS3 风格） ----
    if (fn == L"Number") {
        if (a.t == Value::V_NUM) return a;
        if (a.t == Value::V_STR) return Value::MakeNum(wcstod(a.str.c_str(), NULL));
        if (a.t == Value::V_BOOL) return Value::MakeNum(a.b ? 1.0 : 0.0);
        return Value::MakeNum(0.0);
    }
    if (fn == L"int" || fn == L"uint") {
        double d = 0;
        if (a.t == Value::V_NUM) d = a.num;
        else if (a.t == Value::V_STR) d = wcstod(a.str.c_str(), NULL);
        else if (a.t == Value::V_BOOL) d = a.b ? 1.0 : 0.0;
        return Value::MakeNum((double)(int)d);
    }
    if (fn == L"String") return Value::MakeStr(a.ToString());
    if (fn == L"Boolean") return Value::MakeBool(IsTrue(a));
    // ---- 事件注册（AS3 风格：stage.addEventListener(KeyboardEvent.KEY_DOWN, 函数名)）----
    if (fn == L"addEventListener" || fn == L"stage.addEventListener") {
        if (args.size() >= 2 && args[0].t == Value::V_STR && args[1].t == Value::V_STR) {
            if (args[0].str == L"enterFrame") {
                bool have = false;
                for (size_t i = 0; i < m_frameFns.size(); i++)
                    if (m_frameFns[i] == args[1].str) { have = true; break; }
                if (!have) m_frameFns.push_back(args[1].str);
            } else if (args[0].str == L"keyDown" || args[0].str == L"keyUp") {
                std::map<std::wstring, std::vector<std::wstring> >::iterator it = m_stageFns.find(args[0].str);
                if (it == m_stageFns.end()) {
                    m_stageFns[args[0].str].push_back(args[1].str);
                } else {
                    bool have = false;
                    for (size_t i = 0; i < it->second.size(); i++)
                        if (it->second[i] == args[1].str) { have = true; break; }
                    if (!have) it->second.push_back(args[1].str);
                }
            } else {
                Err(line, L"不支持的事件类型 '" + args[0].str + L"'（可用 enterFrame / keyDown / keyUp）");
                return Value();
            }
            return Value();
        }
        Err(line, L"addEventListener 需要 (事件, 函数名) 两个参数");
        return Value();
    }
    // ---- 旧式函数（中文别名；正弦/余弦按“度”） ----
    if (fn == L"sin" || fn == L"正弦") { return Value::MakeNum(std::sin(a.num * kDeg2Rad)); }
    if (fn == L"cos" || fn == L"余弦") { return Value::MakeNum(std::cos(a.num * kDeg2Rad)); }
    if (fn == L"tan" || fn == L"正切") { return Value::MakeNum(std::tan(a.num * kDeg2Rad)); }
    if (fn == L"sqrt" || fn == L"开方") {
        if (a.num < 0) { Err(line, L"开方不能是负数"); return Value(); }
        return Value::MakeNum(std::sqrt(a.num));
    }
    if (fn == L"abs" || fn == L"绝对值") { return Value::MakeNum(std::fabs(a.num)); }
    if (fn == L"min" || fn == L"最小") { return Value::MakeNum(a.num < b.num ? a.num : b.num); }
    if (fn == L"max" || fn == L"最大") { return Value::MakeNum(a.num > b.num ? a.num : b.num); }
    if (fn == L"random" || fn == L"随机") { return Value::MakeNum((double)std::rand() / (double)RAND_MAX); }
    if (fn == L"play" || fn == L"播放") { if (m_hook) m_hook(1, 0, m_hookUser); return Value(); }
    if (fn == L"stop" || fn == L"停止") { if (m_hook) m_hook(2, 0, m_hookUser); return Value(); }
    if (fn == L"gotoFrame" || fn == L"跳到帧") { if (m_hook) m_hook(3, a.num, m_hookUser); return Value(); }
    Err(line, L"没有这个函数 '" + fn + L"'");
    return Value();
}

// 舞台事件分发：stage 键盘事件（keyDown/keyUp 边沿），由播放窗口在 OnTimer 键盘轮询里调用
void ScriptEngine::FireStageEvent(const std::wstring& type, int code) {
    m_keyCode = code;
    std::map<std::wstring, std::vector<std::wstring> >::iterator it = m_stageFns.find(type);
    if (it == m_stageFns.end()) return;
    std::vector<std::wstring> fns = it->second;
    for (size_t i = 0; i < fns.size(); i++) {
        if (m_funcs.find(fns[i]) == m_funcs.end()) continue;
        std::vector<Value> args(1, Value::MakeNum((double)code));   // 回调参数 e = 键码（AS3 e.keyCode 简化）
        CallUser(fns[i], args, 0);
        if (m_error) {
            // 事件回调出错：记录日志但不中断播放（与 onEnterFrame 出错停播不同）
            m_log += L"[事件 " + type + L"] 回调出错\r\n";
            m_error = false;
            return;
        }
    }
}

// 调用用户自定义函数（按名字查找）
Value ScriptEngine::CallUser(const std::wstring& fn, std::vector<Value>& args, int line) {
    std::map<std::wstring, Node*>::iterator it = m_funcs.find(fn);
    if (it == m_funcs.end() || !it->second) {
        Err(line, L"没有这个函数 '" + fn + L"'");
        return Value();
    }
    return CallUserNode(it->second, args, line);
}

// 调用用户自定义函数（已知节点）：新开一层作用域，绑定参数，执行函数体
Value ScriptEngine::CallUserNode(Node* f, std::vector<Value>& args, int line) {
    if (!f) return Value();
    if (++m_callDepth > 100) {
        Err(f->line, L"函数嵌套太深（可能无限递归）");
        m_callDepth--;
        return Value();
    }
    m_scopes.push_back(std::map<std::wstring, Value>());
    for (size_t i = 0; i < f->kids.size(); i++) {
        Value v = (i < args.size()) ? args[i] : Value();
        m_scopes.back()[f->kids[i]->name] = v;
    }
    bool savedFlag = m_returnFlag;
    Value savedVal = m_returnVal;
    m_returnFlag = false;
    m_returnVal = Value();
    RunNode(f->a);
    Value r = m_returnVal;
    bool hadRet = m_returnFlag;
    m_returnFlag = savedFlag;
    m_returnVal = savedVal;
    m_scopes.pop_back();
    m_callDepth--;
    return hadRet ? r : Value();
}
