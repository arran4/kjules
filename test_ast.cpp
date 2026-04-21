#include <QCoreApplication>
#include <QDebug>
#include "src/filterparser.h"

// AST reconstruction logic
QSharedPointer<ASTNode> mergeFilterIntoAST(QSharedPointer<ASTNode> node, const QString &type, const QString &value, bool isHide, bool &merged) {
    if (!node) return QSharedPointer<ASTNode>();

    if (isHide) {
        if (auto notNode = qSharedPointerDynamicCast<NotNode>(node)) {
            auto child = notNode->child();
            if (auto kvChild = qSharedPointerDynamicCast<KeyValueNode>(child)) {
                if (kvChild->key() == type) {
                    QList<QSharedPointer<ASTNode>> orChildren;
                    orChildren.append(child);
                    orChildren.append(QSharedPointer<ASTNode>(new KeyValueNode(type, value)));
                    merged = true;
                    return QSharedPointer<ASTNode>(new NotNode(QSharedPointer<ASTNode>(new OrNode(orChildren))));
                }
            } else if (auto orChild = qSharedPointerDynamicCast<OrNode>(child)) {
                bool allSameType = true;
                for (const auto &c : orChild->children()) {
                    auto kv = qSharedPointerDynamicCast<KeyValueNode>(c);
                    if (!kv || kv->key() != type) {
                        allSameType = false;
                        break;
                    }
                }
                if (allSameType) {
                    QList<QSharedPointer<ASTNode>> newOrChildren = orChild->children();
                    newOrChildren.append(QSharedPointer<ASTNode>(new KeyValueNode(type, value)));
                    merged = true;
                    return QSharedPointer<ASTNode>(new NotNode(QSharedPointer<ASTNode>(new OrNode(newOrChildren))));
                }
            }
        }
    }

    if (auto andNode = qSharedPointerDynamicCast<AndNode>(node)) {
        QList<QSharedPointer<ASTNode>> newChildren;
        for (const auto &child : andNode->children()) {
            if (!merged) {
                newChildren.append(mergeFilterIntoAST(child, type, value, isHide, merged));
            } else {
                newChildren.append(child);
            }
        }
        return QSharedPointer<ASTNode>(new AndNode(newChildren));
    } else if (auto orNode = qSharedPointerDynamicCast<OrNode>(node)) {
        QList<QSharedPointer<ASTNode>> newChildren;
        for (const auto &child : orNode->children()) {
            if (!merged) {
                newChildren.append(mergeFilterIntoAST(child, type, value, isHide, merged));
            } else {
                newChildren.append(child);
            }
        }
        return QSharedPointer<ASTNode>(new OrNode(newChildren));
    } else if (auto notNode = qSharedPointerDynamicCast<NotNode>(node)) {
        if (!merged) {
            auto newChild = mergeFilterIntoAST(notNode->child(), type, value, isHide, merged);
            return QSharedPointer<ASTNode>(new NotNode(newChild));
        }
    }

    return node;
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    QSharedPointer<ASTNode> ast = FilterParser::parse("=NOT repo:r1 AND owner:o1");
    bool merged = false;
    ast = mergeFilterIntoAST(ast, "repo", "r2", true, merged);
    if (!merged) {
        QList<QSharedPointer<ASTNode>> andChildren;
        andChildren.append(ast);
        andChildren.append(QSharedPointer<ASTNode>(new NotNode(QSharedPointer<ASTNode>(new KeyValueNode("repo", "r2")))));
        ast = QSharedPointer<ASTNode>(new AndNode(andChildren));
    }

    qDebug() << "Test 1:" << ast->toString();
    return 0;
}
