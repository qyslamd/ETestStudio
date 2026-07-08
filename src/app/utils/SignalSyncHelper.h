#ifndef ETEST_APP_SIGNAL_SYNC_HELPER_H_
#define ETEST_APP_SIGNAL_SYNC_HELPER_H_

#include <QString>

namespace icd {
class Node;
class Repository;
}  // namespace icd

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace etest::app {

// 从 icd::Node 向上遍历 parent 链构建 nodePath
// 格式：rootName/childName/grandchildName...
// Node 没有 path() 方法，需遍历 parent 链拼接
QString buildNodePath(const icd::Node* node);

// 桥接 icd::Repository → SignalRegistry
// 遍历 registry 中所有端口绑定，从 Repository 查找帧→节点，生成 SignalEntry，
// 批量注册到 registry。Repository 重建后需重新调用。
void synchronizeRegistry(etest::core::SignalRegistry& registry,
                         const icd::Repository* repo);

}  // namespace etest::app

#endif  // ETEST_APP_SIGNAL_SYNC_HELPER_H_
