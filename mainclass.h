#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>

#include <QTimer>
#include <QDir>
#include <QProcess>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QSqlError>

#include "common.h"

class MainClass : public QObject
{
    Q_OBJECT
public:
    explicit MainClass(QString dataFilePath = NULL, QObject *parent = NULL);

private:
    QVector<_STATION> staListVT_G;
    QVector<QString> netStaVTForGetIndex_G;
    QMultiMap<int, _QSCD_FOR_MULTIMAP> dataHouse_G;

    void loadStationsFromFile(QString);
    void loadHeaderFromFile();
    void loadDataFromFile(QString);

    _EVENT event;
    bool haveEQ;
    QVector<QString> eventFirstInfo;

	// About Database & table
	QSqlDatabase qscdDB;
	QSqlQueryModel *eventModel;
	void openDB();
	void runSQL(QString, QString);

    void makeDataForDistWithTime1();
    void makeDataForDistWithTime2();

    QVector<double> realDataSeriesX;
    QVector<double> realDataSeriesY;
    QVector<QString> realDataStaList;
    QVector<double> realDataPGAList;
    QVector<int> realDataMaxPGATimeList;
    QVector<double> maxVelDataSeriesX;
    QVector<double> maxVelDataSeriesY;
    QVector<double> minVelDataSeriesX;
    QVector<double> minVelDataSeriesY;
    QVector<double> regDataX;
    QVector<double> regDataY;
    QVector<double> regDataUpX;
    QVector<double> regDataUpY;
    QVector<double> regDataDownX;
    QVector<double> regDataDownY;
    double pgaVelocity1;
    double pgaVelocity2;
    QVector<_STATION> outStaListVT;
    int numOfAliveSta;
    int numOfNotUsedSta;
    int numOfNoDataSta;
    int numOfTooFastMaxPGATimeSta;
    int numOfTooFarThisStationWithOrigin;
    int numOfOutOfMINMAXVelocityRangeSta;
    int numOfOutOfRegressionRangeSta;

    int monChanID;

    void makeDataForDistWithPGA();

signals:

public slots:
};

#endif // MAINCLASS_H
