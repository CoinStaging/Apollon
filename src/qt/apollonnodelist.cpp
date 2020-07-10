#include "apollonnodelist.h"
#include "ui_apollonnodelist.h"

#include "activeapollonnode.h"
#include "clientmodel.h"
#include "init.h"
#include "guiutil.h"
#include "apollonnode-sync.h"
#include "apollonnodeconfig.h"
#include "apollonnodeman.h"
#include "sync.h"
#include "wallet/wallet.h"
#include "walletmodel.h"
#include "hybridui/styleSheet.h"
#include <QTimer>
#include <QMessageBox>

int GetOffsetFromUtc()
{
#if QT_VERSION < 0x050200
    const QDateTime dateTime1 = QDateTime::currentDateTime();
    const QDateTime dateTime2 = QDateTime(dateTime1.date(), dateTime1.time(), Qt::UTC);
    return dateTime1.secsTo(dateTime2);
#else
    return QDateTime::currentDateTime().offsetFromUtc();
#endif
}

ApollonnodeList::ApollonnodeList(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ApollonnodeList),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyApollonnodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyApollonnodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyApollonnodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyApollonnodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyApollonnodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyApollonnodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetApollonnodes->setColumnWidth(0, columnAddressWidth);
    ui->tableWidgetApollonnodes->setColumnWidth(1, columnProtocolWidth);
    ui->tableWidgetApollonnodes->setColumnWidth(2, columnStatusWidth);
    ui->tableWidgetApollonnodes->setColumnWidth(3, columnActiveWidth);
    ui->tableWidgetApollonnodes->setColumnWidth(4, columnLastSeenWidth);

    ui->tableWidgetMyApollonnodes->setContextMenuPolicy(Qt::CustomContextMenu);
    SetObjectStyleSheet(ui->tableWidgetMyApollonnodes, StyleSheetNames::TableViewLight);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMyApollonnodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    fFilterUpdated = false;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
}

ApollonnodeList::~ApollonnodeList()
{
    delete ui;
}

void ApollonnodeList::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model) {
        // try to update list when apollonnode count changes
        // connect(clientModel, SIGNAL(strApollonnodesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void ApollonnodeList::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void ApollonnodeList::showContextMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->tableWidgetMyApollonnodes->itemAt(point);
    if(item) contextMenu->exec(QCursor::pos());
}

void ApollonnodeList::StartAlias(std::string strAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
        if(mne.getAlias() == strAlias) {
            std::string strError;
            CApollonnodeBroadcast mnb;

            bool fSuccess = CApollonnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputApollon(), strError, mnb);

            if(fSuccess) {
                strStatusHtml += "<br>Successfully started apollonnode.";
                mnodeman.UpdateApollonnodeList(mnb);
                mnb.RelayApollonNode();
                mnodeman.NotifyApollonnodeUpdates();
            } else {
                strStatusHtml += "<br>Failed to start apollonnode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void ApollonnodeList::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
        std::string strError;
        CApollonnodeBroadcast mnb;

        int32_t nOutputApollon = 0;
        if(!ParseInt32(mne.getOutputApollon(), &nOutputApollon)) {
            continue;
        }

        COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), nOutputApollon);

        if(strCommand == "start-missing" && mnodeman.Has(CTxIn(outpoint))) continue;

        bool fSuccess = CApollonnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputApollon(), strError, mnb);

        if(fSuccess) {
            nCountSuccessful++;
            mnodeman.UpdateApollonnodeList(mnb);
            mnb.RelayApollonNode();
            mnodeman.NotifyApollonnodeUpdates();
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: " + strError;
        }
    }
    pwalletMain->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d apollonnodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void ApollonnodeList::updateMyApollonnodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint)
{
    bool fOldRowFound = false;
    int nNewRow = 0;

    for(int i = 0; i < ui->tableWidgetMyApollonnodes->rowCount(); i++) {
        if(ui->tableWidgetMyApollonnodes->item(i, 0)->text() == strAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if(nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMyApollonnodes->rowCount();
        ui->tableWidgetMyApollonnodes->insertRow(nNewRow);
    }

    apollonnode_info_t infoMn = mnodeman.GetApollonnodeInfo(CTxIn(outpoint));
    bool fFound = infoMn.fInfoValid;

    QTableWidgetItem *aliasItem = new QTableWidgetItem(strAlias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(fFound ? QString::fromStdString(infoMn.addr.ToString()) : strAddr);
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(fFound ? infoMn.nProtocolVersion : -1));
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(fFound ? CApollonnode::StateToString(infoMn.nActiveState) : "MISSING"));
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(fFound ? (infoMn.nTimeLastPing - infoMn.sigTime) : 0)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M",
                                                                                                   fFound ? infoMn.nTimeLastPing + GetOffsetFromUtc() : 0)));
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(fFound ? CBitcoinAddress(infoMn.pubKeyCollateralAddress.GetID()).ToString() : ""));

    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMyApollonnodes->setItem(nNewRow, 6, pubkeyItem);
}

void ApollonnodeList::updateMyNodeList(bool fForce)
{
    TRY_LOCK(cs_mymnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my apollonnode list only once in MY_MASTERNODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_MASTERNODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if(nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetApollonnodes->setSortingEnabled(false);
    BOOST_FOREACH(CApollonnodeConfig::CApollonnodeEntry mne, apollonnodeConfig.getEntries()) {
        int32_t nOutputApollon = 0;
        if(!ParseInt32(mne.getOutputApollon(), &nOutputApollon)) {
            continue;
        }

        updateMyApollonnodeInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), COutPoint(uint256S(mne.getTxHash()), nOutputApollon));
    }
    ui->tableWidgetApollonnodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void ApollonnodeList::updateNodeList()
{
    TRY_LOCK(cs_mnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }

    static int64_t nTimeListUpdated = GetTime();

    // to prevent high cpu usage update only once in MASTERNODELIST_UPDATE_SECONDS seconds
    // or MASTERNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
    int64_t nSecondsToWait = fFilterUpdated
                            ? nTimeFilterUpdated - GetTime() + MASTERNODELIST_FILTER_COOLDOWN_SECONDS
                            : nTimeListUpdated - GetTime() + MASTERNODELIST_UPDATE_SECONDS;

    if(fFilterUpdated) ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", nSecondsToWait)));
    if(nSecondsToWait > 0) return;

    nTimeListUpdated = GetTime();
    fFilterUpdated = false;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidgetApollonnodes->setSortingEnabled(false);
    ui->tableWidgetApollonnodes->clearContents();
    ui->tableWidgetApollonnodes->setRowCount(0);
//    std::map<COutPoint, CApollonnode> mapApollonnodes = mnodeman.GetFullApollonnodeMap();
    std::vector<CApollonnode> vApollonnodes = mnodeman.GetFullApollonnodeVector();
    int offsetFromUtc = GetOffsetFromUtc();

    BOOST_FOREACH(CApollonnode & mn, vApollonnodes)
    {
//        CApollonnode mn = mnpair.second;
        // populate list
        // Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString()));
        QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.nProtocolVersion));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(mn.GetStatus()));
        QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
        QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", mn.lastPing.sigTime + offsetFromUtc)));
        QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));

        if (strCurrentFilter != "")
        {
            strToFilter =   addressItem->text() + " " +
                            protocolItem->text() + " " +
                            statusItem->text() + " " +
                            activeSecondsItem->text() + " " +
                            lastSeenItem->text() + " " +
                            pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }

        ui->tableWidgetApollonnodes->insertRow(0);
        ui->tableWidgetApollonnodes->setItem(0, 0, addressItem);
        ui->tableWidgetApollonnodes->setItem(0, 1, protocolItem);
        ui->tableWidgetApollonnodes->setItem(0, 2, statusItem);
        ui->tableWidgetApollonnodes->setItem(0, 3, activeSecondsItem);
        ui->tableWidgetApollonnodes->setItem(0, 4, lastSeenItem);
        ui->tableWidgetApollonnodes->setItem(0, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidgetApollonnodes->rowCount()));
    ui->tableWidgetApollonnodes->setSortingEnabled(true);
}

void ApollonnodeList::on_filterLineEdit_textChanged(const QString &strFilterIn)
{
    strCurrentFilter = strFilterIn;
    nTimeFilterUpdated = GetTime();
    fFilterUpdated = true;
    ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void ApollonnodeList::on_startButton_clicked()
{
    std::string strAlias;
    {
        LOCK(cs_mymnlist);
        // Find selected node alias
        QItemSelectionModel* selectionModel = ui->tableWidgetMyApollonnodes->selectionModel();
        QModelIndexList selected = selectionModel->selectedRows();

        if(selected.count() == 0) return;

        QModelIndex apollon = selected.at(0);
        int nSelectedRow = apollon.row();
        strAlias = ui->tableWidgetMyApollonnodes->item(nSelectedRow, 0)->text().toStdString();
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm apollonnode start"),
        tr("Are you sure you want to start apollonnode %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void ApollonnodeList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all apollonnodes start"),
        tr("Are you sure you want to start ALL apollonnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void ApollonnodeList::on_startMissingButton_clicked()
{

    if(!apollonnodeSync.IsApollonnodeListSynced()) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until apollonnode list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing apollonnodes start"),
        tr("Are you sure you want to start MISSING apollonnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void ApollonnodeList::on_tableWidgetMyApollonnodes_itemSelectionChanged()
{
    if(ui->tableWidgetMyApollonnodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void ApollonnodeList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}
