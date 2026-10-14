// ProjectIO.h : 工程保存/打开（.s3d，UTF-8 文本格式），不依赖 MFC
#pragma once
#include <string>
#include "Scene3D.h"

namespace ProjectIO {

    // 序列化整个场景（含相机/设置/元件/关键帧/脚本）到文本
    // skipBoolCache=true 时不写布尔网格缓存（撤销快照用：网格走二进制侧挂，避免序列化上万顶点文本）
    std::wstring SerializeToString(const Scene3D& scene, bool skipBoolCache = false);

    // 从文本反序列化（先清空场景）
    bool DeserializeFromString(Scene3D& scene, const std::wstring& text, std::wstring& err);

    // 读写 UTF-8 文件
    bool WriteFileUtf8(const std::wstring& path, const std::wstring& text);
    bool ReadFileUtf8(const std::wstring& path, std::wstring& text);

    bool SaveProject(Scene3D& scene, const std::wstring& path, std::wstring& err);
    bool LoadProject(Scene3D& scene, const std::wstring& path, std::wstring& err);

    // 外部文档类文件：与工程同名同目录的 .ss（SuperScript 类文件，便于后续扩展多个类）
    std::wstring DocClassPath(const std::wstring& projectPath);
}
