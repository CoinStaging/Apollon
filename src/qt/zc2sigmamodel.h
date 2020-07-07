#ifndef ZC2SIGMAMODEL_H
#define ZC2SIGMAMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

#include <memory>

class AddressTablePriv;
class WalletModel;
class CWallet;

class Zc2SigmaModel: public QAbstractTableModel
{
    Q_OBJECT

public:
    Zc2SigmaModel();
    ~Zc2SigmaModel();

    enum ColumnApollon {
        MintCount = 0,
        Denomination = 1,
        Version = 2
    };

    int rowCount(const QModelApollon &parent) const;
    int columnCount(const QModelApollon &parent) const;
    QVariant data(const QModelApollon &apollon, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    Qt::ItemFlags flags(const QModelApollon &apollon) const;

    bool updateRows();

    static size_t GetAvailMintsNumber();
private:
    QStringList columns;
    class ContImpl;
    ContImpl * pContImpl;

    static std::shared_ptr<ContImpl> GetAvailMints();
};

#endif /* ZC2SIGMAMODEL_H */

