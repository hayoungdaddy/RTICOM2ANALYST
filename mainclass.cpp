#include "mainclass.h"

MainClass::MainClass(QString dataFilePath, QObject *parent) : QObject(parent)
{
    haveEQ = false;
    event.eewevid = 0;
    monChanID = 3;

    numOfAliveSta = 0;
    numOfNotUsedSta = 0;
    numOfNoDataSta = 0;
    numOfTooFastMaxPGATimeSta = 0;
    numOfTooFarThisStationWithOrigin = 0;
    numOfOutOfMINMAXVelocityRangeSta = 0;
    numOfOutOfRegressionRangeSta = 0;

    event.datafilepath = dataFilePath;
    loadHeaderFromFile();
    loadStationsFromFile("G");
    loadDataFromFile("G");

	qscdDB = QSqlDatabase::addDatabase("QMYSQL");
	qscdDB.setHostName("210.98.8.76");
	qscdDB.setDatabaseName("QSCD20");
	qscdDB.setUserName("kisstool");
	qscdDB.setPassword("kisstool");

	eventModel = new QSqlQueryModel();

    if(event.eewevid != 0)
        haveEQ = true;

    for(int i=0;i<staListVT_G.count();i++)
    {
        _STATION sta = staListVT_G.at(i);
        if(sta.maxPGA[monChanID] == -1)
        {
            sta.whyout = "NoData";
            staListVT_G.replace(i, sta);
            numOfNoDataSta++;
        }
    }

    if(haveEQ)
    {
        makeDataForDistWithTime1();
        makeDataForDistWithTime2();
        makeDataForDistWithPGA();
    }

    exit(1);
}

void MainClass::openDB()
{
    if(!qscdDB.open())
    {
        qDebug() << "Error connecting to DB: " + qscdDB.lastError().text();
    }
}

void MainClass::runSQL(QString query, QString tableName)
{
    openDB();
    if(tableName.startsWith("event"))
    {
        eventModel->setQuery(query);
        if(eventModel->lastError().isValid())
        {
            qDebug() << eventModel->lastError();
            return;
        }
    }
}

void MainClass::loadStationsFromFile(QString staType)
{
    QString staFileName;
    if(staType.startsWith("G"))
    {
        staListVT_G.clear();
        netStaVTForGetIndex_G.clear();
        staFileName = event.datafilepath + "/SRC/stalist_G.dat" ;
    }

    QFile evStaFile(staFileName);
    if(!evStaFile.exists())
        exit(1);

    if(evStaFile.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&evStaFile);
        QString line, _line;
        int staIndex = 0;

        while(!stream.atEnd())
        {
            line = stream.readLine();
            _line = line.simplified();
            _STATION sta;
            sta.index = staIndex;
            staIndex++;
            //int length = _line.section("=", 0, 0).length();
            //sta.sta = _line.section("=", 0, 0).right(length-2);
            sta.sta = _line.section("=", 0, 0);
            if(staType.startsWith("G"))
            {
                netStaVTForGetIndex_G.push_back(_line.section("=", 0, 0));
            }
            sta.lat = _line.section("=", 1, 1).toDouble();
            sta.lon = _line.section("=", 2, 2).toDouble();
            sta.elev = _line.section("=", 3, 3).toDouble();
            sta.comment = _line.section("=", 4, 4);
            sta.lastPGA = -1;
            sta.lastPGATime = -1;
            for(int i=0;i<5;i++)
            {
                sta.maxPGA[i] = -1;
                sta.maxPGATime[i] = -1;
            }
            sta.inUse =_line.section("=", 5, 5).toInt();
            sta.distanceWithOrigin = 0;
            sta.whyout = "Alive";
            if(sta.inUse != 1)
            {
                numOfNotUsedSta++;
                sta.whyout = "NotUsed";
            }

            if(staType.startsWith("G"))
            {
                staListVT_G.push_back(sta);
            }
        }
        evStaFile.close();
    }
}

void MainClass::loadHeaderFromFile()
{
    QString headerFileName = event.datafilepath + "/SRC/header.dat";

    QFile headerFile(headerFileName);
    if(!headerFile.exists())
        exit(1);

    if(headerFile.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&headerFile);
        QString line, _line;

        while(!stream.atEnd())
        {
            line = stream.readLine();
            _line = line.simplified();

            if(_line.startsWith("EVID"))
                event.evid = _line.section("=", 1, 1).toInt();
            else if(_line.startsWith("EVENT_TIME"))
                event.eventtime = QDateTime::fromString(_line.section("=", 1, 1), "yyyy-MM-dd hh:mm:ss");
            else if(_line.startsWith("DATA_START_TIME"))
                event.datastarttime = _line.section("=", 1, 1).toInt();
            else if(_line.startsWith("EVENT_TYPE"))
                event.eventtype = _line.section("=", 1, 1);
            else if(_line.startsWith("MAG_THRESHOLD"))
                event.thresholdM = _line.section("=", 1, 1).toDouble();
            else if(_line.startsWith("EEW_INFO")) // EARTHQUAKE_INFO=1551054634:36.77:129.04:3.3
            {
                event.eewevid = _line.section("=", 1, 1).section(":", 0, 0).toInt();
                event.origintime = _line.section("=", 1, 1).section(":", 1, 1).toInt();
                event.lat = _line.section("=", 1, 1).section(":", 2, 2).toDouble();
                event.lon = _line.section("=", 1, 1).section(":", 3, 3).toDouble();
                event.mag = _line.section("=", 1, 1).section(":", 4, 4).toDouble();
                event.nsta = _line.section("=", 1, 1).section(":", 5, 5).toInt();
            }
            else if(_line.startsWith("EVENT_CONDITION"))
            {
                event.insec = _line.section("=", 1, 1).section(":", 0, 0).toInt();
                event.numsta = _line.section("=", 1, 1).section(":", 1, 1).toInt();
                event.thresholdG = _line.section("=", 1, 1).section(":", 2, 2).toDouble();
            }

            else if(_line.startsWith("FIRST_EVENT_INFO"))
                eventFirstInfo.push_back(_line.section("=", 1, 1));
        }
        headerFile.close();
    }
}

void MainClass::loadDataFromFile(QString staType)
{
    QString dataFileName;
    if(staType.startsWith("G"))
    {
        dataHouse_G.clear();
        dataFileName = event.datafilepath + "/SRC/PGA_G.dat";
    }

    // insert to multimap
    QFile dataFile(dataFileName);
    if(!dataFile.exists())
        exit(1);

    if(dataFile.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&dataFile);
        QString line, _line;

        while(!stream.atEnd())
        {
            line = stream.readLine();
            _line = line.simplified();

            int dataTime = _line.section(" ", 0, 0).toInt();

            _QSCD_FOR_MULTIMAP qfmm;
            qfmm.sta = _line.section("=", 0, 0).section(" ", 1, 1);
            qfmm.pga[0] = _line.section("=", 1, 1).section(":", 0, 0).toDouble();
            qfmm.pga[1] = _line.section("=", 1, 1).section(":", 1, 1).toDouble();
            qfmm.pga[2] = _line.section("=", 1, 1).section(":", 2, 2).toDouble();
            qfmm.pga[3] = _line.section("=", 1, 1).section(":", 3, 3).toDouble();
            qfmm.pga[4] = _line.section("=", 1, 1).section(":", 4, 4).toDouble();

            int index;

            if(staType.startsWith("G"))
            {
                index = netStaVTForGetIndex_G.indexOf(qfmm.sta);
            }

            if(index != -1)// && staListForEventVT.at(index).maxPGA < qfmm.pga)
            {
                _STATION sta;
                if(staType.startsWith("G"))
                {
                    sta = staListVT_G.at(index);
                }

                int needReplace = 0;

                for(int i=0;i<5;i++)
                {
                    if(sta.maxPGA[i] < qfmm.pga[i])
                    {
                        sta.maxPGATime[i] = dataTime;
                        sta.maxPGA[i] = qfmm.pga[i];
                        needReplace = 1;
                    }
                }

                if(needReplace == 1)
                {
                    if(staType.startsWith("G"))
                    {
                        staListVT_G.replace(index, sta);
                    }
                }

                if(staType.startsWith("G"))
                {
                    dataHouse_G.insert(dataTime, qfmm);
                }
            }
        }
        dataFile.close();
    }
}

void MainClass::makeDataForDistWithTime1()
{
    int minX = 999, maxX = 0;
    double minY = 999, maxY = 0;

    for(int i=0;i<staListVT_G.count();i++)
    {
        if(staListVT_G.at(i).whyout.startsWith("Alive"))
        {
            if(staListVT_G.at(i).maxPGATime[monChanID] < event.origintime)
            {
                _STATION sta = staListVT_G.at(i);
                sta.whyout = "TooFastMaxPGATime:OriginTime(" + QString::number(event.origintime) + "),MaxPGATime(" + QString::number(sta.maxPGATime[monChanID]) + ")";

                double dist = getDistance(event.lat, event.lon, sta.lat, sta.lon);
                sta.distanceWithOrigin = dist;

                staListVT_G.replace(i, sta);
                numOfTooFastMaxPGATimeSta++;
                continue;
            }
            else
            {
                double dist = getDistance(event.lat, event.lon, staListVT_G.at(i).lat, staListVT_G.at(i).lon);

                if(dist >= 400)
                {
                    _STATION sta = staListVT_G.at(i);
	            sta.distanceWithOrigin = dist;
                    sta.whyout = "TooFarThisStationWithOrigin(>400Km)";
                    numOfTooFarThisStationWithOrigin++;
                    staListVT_G.replace(i, sta);
                    continue;
                }

                if(dist < minX) minX = dist;
                if(dist > maxX) maxX = dist;

                int difftime = staListVT_G.at(i).maxPGATime[monChanID] - event.origintime;
                if(difftime < minY) minY = difftime;
                if(difftime > maxY) maxY = difftime;

                double secondsForMAXVelocity = dist / MAX_VEL;
                double secondsForMINVelocity = dist / MIN_VEL;

                if(difftime >= secondsForMAXVelocity && difftime <= secondsForMINVelocity)
                {
                    maxVelDataSeriesX.push_back(dist);
                    maxVelDataSeriesY.push_back(secondsForMAXVelocity);
                    minVelDataSeriesX.push_back(dist);
                    minVelDataSeriesY.push_back(secondsForMINVelocity);

                    _STATION sta = staListVT_G.at(i);
                    sta.distanceWithOrigin = dist;
                    staListVT_G.replace(i, sta);

                    realDataSeriesX.push_back(dist);
                    realDataSeriesY.push_back(difftime);
                    realDataStaList.push_back(staListVT_G.at(i).sta);
                    realDataPGAList.push_back(staListVT_G.at(i).maxPGA[monChanID]);
                    realDataMaxPGATimeList.push_back(staListVT_G.at(i).maxPGATime[monChanID]);
                }
                else
                {
                    _STATION sta = staListVT_G.at(i);
                    sta.distanceWithOrigin = dist;
                    sta.whyout = "OutOfMIN-MAXVelocityRange(" + QString::number(secondsForMAXVelocity, 'f', 2)
                            + "<seconds<" + QString::number(secondsForMINVelocity, 'f', 2) + ")";
                    staListVT_G.replace(i, sta);
                    numOfOutOfMINMAXVelocityRangeSta++;
                    continue;
                }
            }
        }
    }

    double x[realDataSeriesX.count()];
    double y[realDataSeriesY.count()];

    for(int i=0;i<realDataSeriesX.count();i++)
    {
        x[i] = realDataSeriesX.at(i);
        y[i] = realDataSeriesY.at(i);
    }

    double m, b, r;
    linreg(realDataSeriesX.count(), x, y, &m, &b, &r);
    //qDebug() << m << b << r;

    for(int i=0;i<realDataSeriesX.count();i++)
    {
        regDataX.push_back(x[i]);
        regDataY.push_back((m*x[i])+b);

        if(i==1)
        {
            double ddd = x[i]-(x[i-1]);
            double ttt = ((m*x[i])+b) - ((m*x[i-1])+b);
            double vel = ddd / ttt;
            pgaVelocity1 = vel;
        }
    }

    for(int i=0;i<staListVT_G.count();i++)
    {
        if(staListVT_G.at(i).whyout.startsWith("Alive"))
        {
            double dist = getDistance(event.lat, event.lon, staListVT_G.at(i).lat, staListVT_G.at(i).lon);
            int difftime = staListVT_G.at(i).maxPGATime[monChanID] - event.origintime;

            double minValue, maxValue;
            minValue = (dist*m)+b-REGRESSION_RANGE;
            maxValue = (dist*m)+b+REGRESSION_RANGE;

            if(difftime < minValue || difftime > maxValue)
            {
                _STATION sta = staListVT_G.at(i);
                sta.whyout = "OutOfRegressionRange(" + QString::number(minValue, 'f', 2)
                        + "<seconds<" + QString::number(maxValue, 'f', 2) + ")";
                staListVT_G.replace(i, sta);
                numOfOutOfRegressionRangeSta++;
            }
            else
                numOfAliveSta++;
        }
    }

	numOfOutOfRegressionRangeSta--;

    // save to file
    QDir distanceTimeDir(event.datafilepath + "/1_DISTANCE-TIME");
    if(!distanceTimeDir.exists())
        distanceTimeDir.mkpath(".");

    QFile summaryStaInfoFile(event.datafilepath + "/1_DISTANCE-TIME/summaryStaInfo.dat");
    if(summaryStaInfoFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &summaryStaInfoFile );

        stream << "Total:" << QString::number(staListVT_G.count()) << "\n";
        stream << "NotUsed:" << QString::number(numOfNotUsedSta) << "\n";
        stream << "NoData:" << QString::number(numOfNoDataSta) << "\n";
        stream << "TooFastMaxPGATime:" << QString::number(numOfTooFastMaxPGATimeSta) << "\n";
        stream << "TooFarWithOrigin:" << QString::number(numOfTooFarThisStationWithOrigin) << "\n";
        stream << "OutOfMIN-MAXVelocityRange(" << MIN_VEL << "-" << MAX_VEL << "):" << QString::number(numOfOutOfMINMAXVelocityRangeSta) << "\n";
        stream << "OutOfRegressionRange(" << REGRESSION_RANGE << "):" << QString::number(numOfOutOfRegressionRangeSta) << "\n";
        stream << "Alive:" << QString::number(numOfAliveSta) << "\n";

        summaryStaInfoFile.close();

	if(numOfAliveSta >= NOS_FOR_VOTER)
	{
		QString query = "UPDATE EVENT SET vote=1 WHERE evid=" + QString::number(event.evid);
		runSQL(query, "event");
	}
    }

    QFile totalStaInfoFile(event.datafilepath + "/1_DISTANCE-TIME/outStaList.dat");
    if(totalStaInfoFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &totalStaInfoFile );

        for(int i=0;i<staListVT_G.count();i++)
        {
            stream << staListVT_G.at(i).sta << "="
                   << QString::number(staListVT_G.at(i).distanceWithOrigin, 'f', 4) << "="
                   << QString::number(staListVT_G.at(i).maxPGATime[monChanID]) << "="
                   << QString::number(staListVT_G.at(i).maxPGATime[monChanID] - event.origintime) << "="
                   << QString::number(staListVT_G.at(i).maxPGA[monChanID], 'f', 4) << "="
                   << staListVT_G.at(i).whyout << "\n";
        }

        totalStaInfoFile.close();
    }

// for Intensity Map by YUN. 2020-02-17
    QFile finalStaInfoFile(event.datafilepath + "/1_DISTANCE-TIME/tttt.dat");
    if(finalStaInfoFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &finalStaInfoFile );

/*
	stream << "origintime(human)=" << event.eventtime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
	stream << "origintime(epoch)=" << QString::number(event.origintime) << "\n";
	stream << "latitude=" << QString::number(event.lat, 'f', 4) << "\n";
	stream << "longitude=" << QString::number(event.lon, 'f', 4) << "\n";
	stream << "magnitude=" << QString::number(event.mag, 'f', 2) << "\n";
	stream << "numberofstation=" << QString::number(numOfAliveSta) << "\n\n";
*/

        for(int i=0;i<staListVT_G.count();i++)
        {
            if(staListVT_G.at(i).whyout.startsWith("Alive"))
	    {
            stream << staListVT_G.at(i).sta << "\t"
                   << QString::number(staListVT_G.at(i).distanceWithOrigin, 'f', 4) << "\t"
		   << QString::number(staListVT_G.at(i).lat, 'f', 4) << "\t"
		   << QString::number(staListVT_G.at(i).lon, 'f', 4) << "\t"
                   << QString::number(staListVT_G.at(i).maxPGATime[3]) << "\t"
                   << QString::number(staListVT_G.at(i).maxPGA[3], 'f', 4) << "\t"
                   << QString::number(staListVT_G.at(i).maxPGATime[4]) << "\t"
                   << QString::number(staListVT_G.at(i).maxPGA[4], 'f', 4) << "\n";
	    }
        }

        finalStaInfoFile.close();
    }
    //QString cmd2 = "sort -t$'\t' -k2 -n " + event.datafilepath + "/1_DISTANCE-TIME/tttt.dat > " + event.datafilepath + "/1_DISTANCE-TIME/" + event.eventtime.toString("yyyyMMdd_hhmmss") + ".dat";
    QString cmd2 = "sort -t$'\t' -k2 -n " + event.datafilepath + "/1_DISTANCE-TIME/tttt.dat > " + event.datafilepath + "/1_DISTANCE-TIME/" + "finalOutput.dat";
    system(cmd2.toLatin1().data());
    cmd2 = "rm " + event.datafilepath + "/1_DISTANCE-TIME/tttt.dat";
    system(cmd2.toLatin1().data());
/////////////////////////////////////////

    QString cmd = "grep Alive " + event.datafilepath + "/1_DISTANCE-TIME/outStaList.dat > " + event.datafilepath + "/1_DISTANCE-TIME/temp.dat";
    system(cmd.toLatin1().data());
    cmd = "sort -t$'=' -k2 -n " + event.datafilepath + "/1_DISTANCE-TIME/temp.dat > " + event.datafilepath + "/1_DISTANCE-TIME/lastStaList.dat";
    system(cmd.toLatin1().data());
    cmd = "rm " + event.datafilepath + "/1_DISTANCE-TIME/temp.dat";
    system(cmd.toLatin1().data());

    QFile realDataFile1(event.datafilepath + "/1_DISTANCE-TIME/realData.dat");
    if(realDataFile1.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &realDataFile1 );

        for(int i=0;i<realDataSeriesX.count();i++)
        {
            stream << QString::number(realDataSeriesX.at(i), 'f', 2) << " "
                   << QString::number(realDataSeriesY.at(i), 'f', 2) << " "
                   << realDataStaList.at(i) << " "
                   << QString::number(realDataPGAList.at(i), 'f', 6) << " "
                   << QString::number(realDataMaxPGATimeList.at(i)) << "\n";
        }

        realDataFile1.close();
    }

    QFile maxVelDataFile(event.datafilepath + "/1_DISTANCE-TIME/maxVelData.dat");
    if(maxVelDataFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &maxVelDataFile );

        for(int i=0;i<maxVelDataSeriesX.count();i++)
        {
            stream << QString::number(maxVelDataSeriesX.at(i), 'f', 2) << " "
                   << QString::number(maxVelDataSeriesY.at(i), 'f', 2) << "\n";
        }

        maxVelDataFile.close();
    }

    QFile minVelDataFile(event.datafilepath + "/1_DISTANCE-TIME/minVelData.dat");
    if(minVelDataFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &minVelDataFile );

        for(int i=0;i<minVelDataSeriesX.count();i++)
        {
            stream << QString::number(minVelDataSeriesX.at(i), 'f', 2) << " "
                   << QString::number(minVelDataSeriesY.at(i), 'f', 2) << "\n";
        }

        minVelDataFile.close();
    }

    QFile linregHeaderFile(event.datafilepath + "/1_DISTANCE-TIME/linregHeader.dat");
    if(linregHeaderFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &linregHeaderFile );

        stream << "slope:" << QString::number(m, 'f', 5) << "\n"
               << "intercept:" << QString::number(b, 'f', 5) << "\n"
               << "coefficient:" << QString::number(r, 'f', 5) << "\n";

        if(b < 0)
            stream << "y=" + QString::number(m, 'f', 5) << "x" << QString::number(b, 'f', 5) << "\n";
        else
            stream << "y=" + QString::number(m, 'f', 5) << "x+" << QString::number(b, 'f', 5) << "\n";

        stream << "PGA_Velocity:" << QString::number(pgaVelocity1, 'f', 2);

        linregHeaderFile.close();
    }

    QFile regDataFile(event.datafilepath + "/1_DISTANCE-TIME/linregData.dat");
    if(regDataFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &regDataFile );

        for(int i=0;i<regDataX.count();i++)
        {
            stream << QString::number(regDataX.at(i), 'f', 2) << " "
                   << QString::number(regDataY.at(i), 'f', 2) << "\n";
        }

        regDataFile.close();
    }
}

void MainClass::makeDataForDistWithTime2()
{
    int minX = 999, maxX = 0;
    double minY = 999, maxY = 0;

    realDataSeriesX.clear();
    realDataSeriesY.clear();
    realDataStaList.clear();
    realDataPGAList.clear();
    realDataMaxPGATimeList.clear();
    regDataX.clear();
    regDataY.clear();
    regDataUpX.clear();
    regDataUpY.clear();
    regDataDownX.clear();
    regDataDownY.clear();

    for(int i=0;i<staListVT_G.count();i++)
    {
        if(staListVT_G.at(i).whyout.startsWith("Alive"))
        {
            double dist = getDistance(event.lat, event.lon, staListVT_G.at(i).lat, staListVT_G.at(i).lon);

            if(dist < minX) minX = dist;
            if(dist > maxX) maxX = dist;

            int difftime = staListVT_G.at(i).maxPGATime[monChanID] - event.origintime;
            if(difftime < minY) minY = difftime;
            if(difftime > maxY) maxY = difftime;

            realDataSeriesX.push_back(dist);
            realDataSeriesY.push_back(difftime);
            realDataStaList.push_back(staListVT_G.at(i).sta);
            realDataPGAList.push_back(staListVT_G.at(i).maxPGA[monChanID]);
            realDataMaxPGATimeList.push_back(staListVT_G.at(i).maxPGATime[monChanID]);
        }
    }

    double x[realDataSeriesX.count()];
    double y[realDataSeriesY.count()];

    for(int i=0;i<realDataSeriesX.count();i++)
    {
        x[i] = realDataSeriesX.at(i);
        y[i] = realDataSeriesY.at(i);
    }

    double m, b, r;
    linreg(realDataSeriesX.count(), x, y, &m, &b, &r);
    //qDebug() << m << b << r;

    for(int i=0;i<realDataSeriesX.count();i++)
    {
        regDataX.push_back(x[i]);
        regDataY.push_back((m*x[i])+b);
        regDataUpX.push_back(x[i]);
        regDataUpY.push_back((m*x[i])+b+REGRESSION_RANGE);
        regDataDownX.push_back(x[i]);
        regDataDownY.push_back((m*x[i])+b-REGRESSION_RANGE);

        if(i==1)
        {
            double ddd = x[i]-(x[i-1]);
            double ttt = ((m*x[i])+b) - ((m*x[i-1])+b);
            double vel = ddd / ttt;
            pgaVelocity2 = vel;
        }
    }

    /*
    int count = 0;
    for(int x=minX;x<maxX;x++)
    {
        regDataX.push_back(x);
        regDataY.push_back((m*x)+b);
        regDataUpX.push_back(x);
        regDataUpY.push_back((m*x)+b+REGRESSION_RANGE);
        regDataDownX.push_back(x);
        regDataDownY.push_back((m*x)+b-REGRESSION_RANGE);

        if(count != 0)
        {
            double ddd = x-(x-1);
            double ttt = ((m*x)+b) - ((m*(x-1))+b);
            double vel = ddd / ttt;
            pgaVelocity2 = vel;
        }
        count++;
    }
    */

    // save to file
    QDir distanceTimeDir(event.datafilepath + "/1_DISTANCE-TIME");
    if(!distanceTimeDir.exists())
        distanceTimeDir.mkpath(".");

    QFile realDataFile1(event.datafilepath + "/1_DISTANCE-TIME/realDataAfterSD.dat");
    if(realDataFile1.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &realDataFile1 );

        for(int i=0;i<realDataSeriesX.count();i++)
        {
            stream << QString::number(realDataSeriesX.at(i), 'f', 2) << " "
                   << QString::number(realDataSeriesY.at(i), 'f', 2) << " "
                   << realDataStaList.at(i) << " "
                   << QString::number(realDataPGAList.at(i), 'f', 6) << " "
                   << QString::number(realDataMaxPGATimeList.at(i)) << "\n";
        }

        realDataFile1.close();
    }

    QFile linregHeaderAfterSDFile(event.datafilepath + "/1_DISTANCE-TIME/linregHeaderAfterSD.dat");
    if(linregHeaderAfterSDFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &linregHeaderAfterSDFile );

        stream << "slope:" << QString::number(m, 'f', 5) << "\n"
               << "intercept:" << QString::number(b, 'f', 5) << "\n"
               << "coefficient:" << QString::number(r, 'f', 5) << "\n";

        if(b < 0)
            stream << "y=" + QString::number(m, 'f', 5) << "x" << QString::number(b, 'f', 5) << "\n";
        else
            stream << "y=" + QString::number(m, 'f', 5) << "x+" << QString::number(b, 'f', 5) << "\n";

        stream << "PGA_Velocity:" << QString::number(pgaVelocity2, 'f', 2);

        linregHeaderAfterSDFile.close();
    }

    QFile regDataAfterSDFile(event.datafilepath + "/1_DISTANCE-TIME/linregDataAfterSD.dat");
    if(regDataAfterSDFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &regDataAfterSDFile );

        for(int i=0;i<regDataX.count();i++)
        {
            stream << QString::number(regDataX.at(i), 'f', 2) << " "
                   << QString::number(regDataY.at(i), 'f', 2) << "\n";
        }

        regDataAfterSDFile.close();
    }

    QFile regDataUPFile(event.datafilepath + "/1_DISTANCE-TIME/linregUpData.dat");
    if(regDataUPFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &regDataUPFile );

        for(int i=0;i<regDataUpX.count();i++)
        {
            stream << QString::number(regDataUpX.at(i), 'f', 2) << " "
                   << QString::number(regDataUpY.at(i), 'f', 2) << "\n";
        }

        regDataUPFile.close();
    }

    QFile regDataDOWNFile(event.datafilepath + "/1_DISTANCE-TIME/linregDownData.dat");
    if(regDataDOWNFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QTextStream stream( &regDataDOWNFile );

        for(int i=0;i<regDataDownX.count();i++)
        {
            stream << QString::number(regDataDownX.at(i), 'f', 2) << " "
                   << QString::number(regDataDownY.at(i), 'f', 2) << "\n";
        }

        regDataDOWNFile.close();
    }
}

void MainClass::makeDataForDistWithPGA()
{
    // save to file
    QDir distancePGADir(event.datafilepath + "/2_DISTANCE-PGA");
    if(!distancePGADir.exists())
        distancePGADir.mkpath(".");

    for(int i=0;i<staListVT_G.count();i++)
    {
        if(staListVT_G.at(i).whyout.startsWith("Alive"))
        {
            QFile netPGAFile;
            netPGAFile.setFileName(event.datafilepath + "/2_DISTANCE-PGA/" + staListVT_G.at(i).sta.left(2) + ".dat");

            if(netPGAFile.open(QIODevice::WriteOnly | QIODevice::Append))
            {
                QTextStream stream( &netPGAFile );

                stream << staListVT_G.at(i).sta << " "
                       << QString::number(staListVT_G.at(i).distanceWithOrigin, 'f', 4) << " "
                       << QString::number(staListVT_G.at(i).maxPGATime[monChanID]) << " "
                       << QString::number(staListVT_G.at(i).maxPGA[monChanID], 'f', 4) << "\n";

                netPGAFile.close();
            }

            QFile staPGAFile;
            QDir staPGADir(event.datafilepath + "/3_PGA/" + staListVT_G.at(i).sta.left(2));
            if(!staPGADir.exists())
                staPGADir.mkpath(".");

        }
    }
    /////////////////////////////////////

    for(int i=dataHouse_G.firstKey();i<=dataHouse_G.lastKey();i++)
    {
        QList<_QSCD_FOR_MULTIMAP> pgaList = dataHouse_G.values(i);

        int staIndex;

        for(int j=0;j<pgaList.count();j++)
        {
            staIndex = netStaVTForGetIndex_G.indexOf(pgaList.at(j).sta);

            if(staIndex != -1 && staListVT_G.at(staIndex).whyout.startsWith("Alive"))
            {
                QFile staPGAFile;
                staPGAFile.setFileName(event.datafilepath + "/3_PGA/" + staListVT_G.at(staIndex).sta.left(2) + "/" + staListVT_G.at(staIndex).sta + ".dat");

                if(staPGAFile.open(QIODevice::WriteOnly | QIODevice::Append))
                {
                    QTextStream stream( &staPGAFile );

                    stream << QString::number(i) << " "
                           << QString::number(pgaList.at(j).pga[monChanID], 'f', 6) << "\n";

                    staPGAFile.close();
                }
            }
        }
    }
}
