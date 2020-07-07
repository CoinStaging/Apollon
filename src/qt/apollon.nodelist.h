#ifndef MASTERNODELIST_H
#define MASTERNODELIST_H

#include "primitives/transaction.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QMenu>
#include <QTimer>
#include <QWidget>

#define MY_MASTERNODELIST_UPDATE_SECONDS                 60
#define MASTERNODELIST_UPDATE_SECONDS                    15
#define MASTERNODELIST_FILTER_COOLDOWN_SECONDS            3

namespace Ui {
    class IndexnodeList;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Indexnode Manager page widget */
class IndexnodeList : public QWidget
{
    Q_OBJECT

public:
    explicit IndexnodeList(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~IndexnodeList();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void StartAlias(std::string strAlias);
    void StartAll(std::string strCommand = "start-all");

private:
    QMenu *contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

public Q_SLOTS:
    void updateMyIndexnodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint);
    void updateMyNodeList(bool fForce = false);
    void updateNodeList();

Q_SIGNALS:

private:
    QTimer *timer;
    Ui::IndexnodeList *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

    // Protects tableWidgetIndexnodes
    CCriticalSection cs_mnlist;

    // Protects tableWidgetMyIndexnodes
    CCriticalSection cs_mymnlist;

    QString strCurrentFilter;

private Q_SLOTS:
    void showContextMenu(const QPoint &);
    void on_filterLineEdit_textChanged(const QString &strFilterIn);
    void on_startButton_clicked();
    void on_startAllButton_clicked();
    void on_startMissingButton_clicked();
    void on_tableWidgetMyIndexnodes_itemSelectionChanged();
    void on_UpdateButton_clicked();
};
#endif // MASTERNODELIST_H
